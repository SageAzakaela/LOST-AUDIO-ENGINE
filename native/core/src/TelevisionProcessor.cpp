#include <lost_audio/core/TelevisionProcessor.h>

#include <algorithm>
#include <cmath>

namespace lost_audio::core
{
namespace
{
constexpr float pi = 3.14159265358979323846f;
float clamp01(float v) noexcept { return std::clamp(v, 0.0f, 1.0f); }
float normalisedTanh(float x, float drive) noexcept { const auto d = std::max(.1f, drive); return std::tanh(x * d) / std::tanh(d); }
struct ModelProfile { float hp, lp, midHz, midDb, resonance, rattle; };
ModelProfile profile(TelevisionModel model) noexcept
{
    switch (model)
    {
        case TelevisionModel::portable: return { 150, 6200, 2050, 3.4f, .48f, .18f };
        case TelevisionModel::console: return { 68, 9300, 1650, 2.0f, .65f, .09f };
        case TelevisionModel::broadcastMonitor: return { 45, 14500, 1350, .7f, .25f, .025f };
        case TelevisionModel::kitchen: return { 125, 7200, 2250, 3.0f, .55f, .15f };
        case TelevisionModel::motel: return { 90, 7800, 1820, 2.6f, .72f, .22f };
    }
    return { 68, 9300, 1650, 2.0f, .65f, .09f };
}
float receptionNoise(TelevisionReception mode) noexcept
{
    switch (mode) { case TelevisionReception::baseband: return .15f; case TelevisionReception::antenna: return 1.0f; case TelevisionReception::cable: return .38f; case TelevisionReception::detuned: return 1.65f; }
    return 1.0f;
}
}

TelevisionMacroTargets mapTelevisionMacros(TelevisionModel model, TelevisionReception reception, float vibe, float speaker, float agc, float staticAmount) noexcept
{
    const auto v = std::pow(clamp01(vibe), 1.15f), sp = std::pow(clamp01(speaker), 1.15f), a = std::pow(clamp01(agc), 1.25f), st = std::pow(clamp01(staticAmount), 1.2f);
    const auto m = profile(model); TelevisionMacroTargets t;
    t.highPassHz = std::clamp(m.hp + (1.0f - sp) * 100.0f + v * 24.0f, 20.0f, 1200.0f);
    t.lowPassHz = std::clamp(m.lp + sp * 2200.0f - v * 2200.0f, 800.0f, 18000.0f);
    t.midHumpDb = std::clamp(m.midDb + (1.0f - sp) * 2.2f + v * .8f, -6.0f, 10.0f);
    t.midFrequencyHz = std::clamp(m.midHz + (1.0f - sp) * 380.0f, 600.0f, 5000.0f);
    t.noiseHiss = clamp01(.32f + st * .62f); t.noiseCrackle = clamp01(.015f + v * .14f + st * .08f);
    t.compression = clamp01(.06f + a * .72f); t.drive = clamp01(.12f + v * .45f + a * .22f);
    const auto receptionWear = reception == TelevisionReception::detuned ? .32f : reception == TelevisionReception::antenna ? .12f : .025f;
    t.tunerDrift = clamp01(receptionWear + v * .20f); t.syncInstability = clamp01(receptionWear * .7f + v * v * .18f); t.powerSag = clamp01(.025f + v * .24f + a * .08f);
    t.cabinet = clamp01(m.resonance + (1.0f - sp) * .22f); t.cabinetRattle = clamp01(m.rattle + v * v * .22f);
    t.limiter = clamp01(.3f + a * .35f + v * .14f); t.ceiling = std::clamp(.96f - v * .08f, .72f, 1.0f); t.outputGain = std::clamp(1.0f + a * .08f - v * .05f, 0.0f, 1.5f); return t;
}

float TelevisionProcessor::Biquad::process(float x) noexcept { const auto y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
void TelevisionProcessor::Biquad::setHighPass(double sr, float hz, float q) noexcept { const auto w=2*pi*std::clamp(hz,10.0f,(float)sr*.46f)/(float)sr,c=std::cos(w),s=std::sin(w),a=s/(2*q),a0=1+a;b0=((1+c)/2)/a0;b1=-(1+c)/a0;b2=b0;a1=-2*c/a0;a2=(1-a)/a0; }
void TelevisionProcessor::Biquad::setLowPass(double sr, float hz, float q) noexcept { const auto w=2*pi*std::clamp(hz,10.0f,(float)sr*.46f)/(float)sr,c=std::cos(w),s=std::sin(w),a=s/(2*q),a0=1+a;b0=((1-c)/2)/a0;b1=(1-c)/a0;b2=b0;a1=-2*c/a0;a2=(1-a)/a0; }
void TelevisionProcessor::Biquad::setPeak(double sr, float hz, float q, float db) noexcept { const auto A=std::pow(10.0f,db/40.0f),w=2*pi*std::clamp(hz,10.0f,(float)sr*.46f)/(float)sr,c=std::cos(w),s=std::sin(w),a=s/(2*q),a0=1+a/A;b0=(1+a*A)/a0;b1=-2*c/a0;b2=(1-a*A)/a0;a1=b1;a2=(1-a/A)/a0; }

std::uint32_t TelevisionProcessor::nextU32(std::uint32_t& s) noexcept { if(s==0)s=1;s^=s<<13;s^=s>>17;s^=s<<5;return s; }
float TelevisionProcessor::nextFloat(std::uint32_t& s) noexcept { return (nextU32(s)>>8)*(1.0f/16777216.0f); }
float TelevisionProcessor::nextSigned(std::uint32_t& s) noexcept { return nextFloat(s)*2.0f-1.0f; }

void TelevisionProcessor::prepare(double sr, std::size_t channels) { sampleRate_=std::max(8000.0,sr);preparedChannels_=std::clamp<std::size_t>(channels,1,maxChannels);reset(); }
void TelevisionProcessor::triggerSyncFault(float strength, float durationSeconds) noexcept
{
    pendingSyncDurationSeconds_.store(std::max(0.0f, durationSeconds), std::memory_order_relaxed);
    pendingSyncStrength_.store(clamp01(strength), std::memory_order_release);
}
float TelevisionProcessor::syncProgress() const noexcept
{
    return syncRemaining_ > 0 && syncLength_ > 0
        ? clamp01(1.0f - static_cast<float>(syncRemaining_) / static_cast<float>(syncLength_)) : 0.0f;
}
void TelevisionProcessor::reset(std::uint32_t seed) noexcept
{
    eventRandom_=seed?seed:1;for(std::size_t ch=0;ch<2;++ch){noiseRandom_[ch]=eventRandom_^(0x9e3779b9u*(std::uint32_t)(ch+1));textureRandom_[ch]=eventRandom_^(0x85ebca6bu*(std::uint32_t)(ch+3));channel_[ch]={};inputPeak_[ch]=outputPeak_[ch]=0;}
    humPhase_=flybackPhase_=flybackSubPhase_=driftPhase_=staticEnvelope_=electricalEnvelope_=rattleEnvelope_=0;syncStrength_=1;syncCountdown_=0;syncRemaining_=0;syncLength_=1;pendingSyncStrength_.store(0);pendingSyncDurationSeconds_.store(0);
}
void TelevisionProcessor::updateFilters(const TelevisionParameters& p) noexcept
{
    const auto model = profile(p.model); for(std::size_t ch=0;ch<preparedChannels_;++ch){auto& f=channel_[ch].filters;f[0].setHighPass(sampleRate_,p.highPassHz,.707f);f[1].setHighPass(sampleRate_,p.highPassHz,.707f);f[2].setPeak(sampleRate_,650,.9f,-.35f*p.midHumpDb);f[3].setPeak(sampleRate_,p.midFrequencyHz,1.15f,p.midHumpDb);f[4].setLowPass(sampleRate_,p.lowPassHz,.85f);f[5].setPeak(sampleRate_,model.midHz*.42f,1.3f,5.0f*p.cabinet);}
}

void TelevisionProcessor::process(float* const* channels, std::size_t count, std::size_t samples, const TelevisionParameters& raw, const float* auxiliaryMono) noexcept
{
    if(!channels||samples==0||preparedChannels_==0)return;count=std::min({count,preparedChannels_,maxChannels});auto p=raw;p.staticAmount=clamp01(p.staticAmount);p.noiseHiss=clamp01(p.noiseHiss);p.noiseCrackle=clamp01(p.noiseCrackle);p.hum=clamp01(p.hum);p.whine=clamp01(p.whine);p.tunerDrift=clamp01(p.tunerDrift);p.syncInstability=clamp01(p.syncInstability);p.powerSag=clamp01(p.powerSag);p.cabinet=clamp01(p.cabinet);p.cabinetRattle=clamp01(p.cabinetRattle);p.mix=clamp01(p.mix);p.limiter=clamp01(p.limiter);p.ceiling=std::clamp(p.ceiling,.2f,1.0f);updateFilters(p);
    const auto sr=(float)sampleRate_,noiseScale=receptionNoise(p.reception),flybackHz=std::min(15734.0f,sr*.45f),attack=std::exp(-1.0f/(sr*.008f)),release=std::exp(-1.0f/(sr*.14f)),limAtk=std::exp(-1.0f/(sr*.0015f)),limRel=std::exp(-1.0f/(sr*.09f));
    for(std::size_t i=0;i<samples;++i)
    {
        humPhase_+=2*pi*60/sr;if(humPhase_>2*pi)humPhase_-=2*pi;flybackPhase_+=2*pi*flybackHz/sr;if(flybackPhase_>2*pi)flybackPhase_-=2*pi;flybackSubPhase_+=pi*flybackHz/sr;if(flybackSubPhase_>2*pi)flybackSubPhase_-=2*pi;driftPhase_+=2*pi*(.08f+p.tunerDrift*.21f)/sr;if(driftPhase_>2*pi)driftPhase_-=2*pi;
        const auto requestedStrength=pendingSyncStrength_.exchange(0.0f,std::memory_order_acq_rel);if(requestedStrength>0){const auto duration=pendingSyncDurationSeconds_.exchange(0.0f,std::memory_order_relaxed);syncLength_=std::max(1,(int)(sr*(duration>0?duration:(.006f+p.syncInstability*.055f))));syncRemaining_=syncLength_;syncStrength_=requestedStrength;}
        if(syncCountdown_<=0&&p.syncInstability>0&&nextFloat(eventRandom_)<p.syncInstability*p.syncInstability*.000014f){syncLength_=(int)(sr*(.006f+p.syncInstability*.055f));syncRemaining_=syncLength_;syncStrength_=1.0f;syncCountdown_=(int)(sr*(.08f+nextFloat(eventRandom_)*.7f));}else if(syncCountdown_>0)--syncCountdown_;
        const auto syncPosition=syncRemaining_>0?1.0f-(float)syncRemaining_/(float)syncLength_:0.0f,rawSyncGain=std::clamp(.18f+std::abs(std::sin(syncPosition*pi*5))* .55f,0.0f,1.0f),syncGain=syncRemaining_>0?1.0f+(rawSyncGain-1.0f)*syncStrength_:1.0f;if(syncRemaining_>0)--syncRemaining_;
        const auto drift=.5f+.5f*std::sin(driftPhase_)+.2f*std::sin(driftPhase_*2.73f),hum=std::sin(humPhase_)*p.hum*p.hum*.024f,fly=std::sin(flybackPhase_)*p.whine*p.whine*.0025f+std::sin(flybackSubPhase_)*std::pow(p.whine,3.0f)*.0045f;
        electricalEnvelope_=std::max(std::abs(hum+fly),electricalEnvelope_*.999f);
        for(std::size_t ch=0;ch<count;++ch)
        {
            auto& s=channel_[ch];auto* data=channels[ch];if(!data)continue;const auto dry=data[i],in=(dry+(auxiliaryMono?auxiliaryMono[i]*p.auxiliaryGain:0.0f))*p.inputGain;inputPeak_[ch]=std::max(std::abs(in),inputPeak_[ch]*.999f);
            const auto magnitude=std::abs(in);s.agcEnvelope=magnitude+(magnitude>s.agcEnvelope?attack:release)*(s.agcEnvelope-magnitude);const auto target=std::clamp(.24f/(s.agcEnvelope+1e-4f),.55f,1.0f+clamp01(p.compression)*2.1f);s.agcGain+= (target-s.agcGain)*(.0004f+clamp01(p.compression)*.0022f);
            auto y=in*s.agcGain*syncGain;for(int f=0;f<5;++f)y=s.filters[(std::size_t)f].process(y);y=normalisedTanh(y,1.0f+clamp01(p.drive)*4.0f);y=s.filters[5].process(y);
            const auto white=nextSigned(noiseRandom_[ch]);s.noiseHigh=.60f*(s.noiseHigh+white-s.previousNoise);s.previousNoise=white;s.noiseLow+=(s.noiseHigh-s.noiseLow)*(.004f+(1-p.noiseHiss)*.06f);
            if(s.crackleRemaining>0){--s.crackleRemaining;s.crackle*=.91f;}else if(p.staticAmount>0&&nextFloat(textureRandom_[ch])<(.000004f+p.noiseCrackle*p.noiseCrackle*.00020f)){s.crackleRemaining=4+(int)(nextFloat(textureRandom_[ch])*24);s.crackle=nextSigned(textureRandom_[ch])*(.06f+.12f*p.noiseCrackle);}
            const auto snow=(s.noiseLow*.055f+s.crackle)*p.staticAmount*p.staticAmount*noiseScale*(.55f+.45f*drift);staticEnvelope_=std::max(std::abs(snow),staticEnvelope_*.999f);
            const auto excitation=std::max(0.0f,magnitude-.16f)*p.cabinetRattle;const auto rattling=nextSigned(textureRandom_[ch])*excitation*.08f;s.rattleVelocity+=(-s.rattle*.012f-s.rattleVelocity*.055f+rattling);s.rattle+=s.rattleVelocity;
            rattleEnvelope_=std::max(std::abs(s.rattle),rattleEnvelope_*.999f);
            const auto sag=1.0f-p.powerSag*.12f*(.5f+.5f*std::sin(humPhase_));y=(y+s.rattle+snow+hum+fly)*sag;
            const auto driven=dry+(y-dry)*p.mix;auto out=driven*p.outputGain;const auto absOut=std::abs(out);s.limiterEnvelope=absOut+(absOut>s.limiterEnvelope?limAtk:limRel)*(s.limiterEnvelope-absOut);if(p.limiter>0&&s.limiterEnvelope>p.ceiling){const auto wanted=p.ceiling/(s.limiterEnvelope+1e-6f);out*=1.0f+(wanted-1.0f)*p.limiter;}out=std::clamp(out,-p.ceiling,p.ceiling);data[i]=out;outputPeak_[ch]=std::max(std::abs(out),outputPeak_[ch]*.999f);
        }
    }
}
}
