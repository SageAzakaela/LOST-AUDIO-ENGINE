#include <lost_audio/core/CartridgeProcessor.h>

#include <algorithm>
#include <cmath>

namespace lost_audio::core
{
namespace
{
constexpr float pi=3.14159265358979323846f; float clamp01(float v) noexcept {return std::clamp(v,0.0f,1.0f);}
struct SpeakerProfile {float hp,lp,dipHz,dipDb,humpHz,humpDb,q;};
SpeakerProfile profile(CartridgeSpeaker s) noexcept
{
    switch(s){case CartridgeSpeaker::direct:return{35,15000,720,0,1800,0,1};case CartridgeSpeaker::television:return{105,7200,620,-2.2f,1450,4.1f,1.25f};
    case CartridgeSpeaker::cabinet:return{72,8600,980,-1.6f,2380,3.8f,1.1f};case CartridgeSpeaker::pcSpeaker:return{360,4300,900,-4.5f,2350,7.4f,2.1f};
    case CartridgeSpeaker::handheld:default:return{180,5200,720,-3.2f,2050,5.2f,1.55f};}
}
}

CartridgeMacroTargets mapCartridgeMacros(CartridgeCodec mode, CartridgeSpeaker speakerModel, float quality, float codec, float grit, float noise) noexcept
{
    const auto q=clamp01(quality), c=std::pow(clamp01(codec),1.2f), g=std::pow(clamp01(grit),1.25f), n=std::pow(clamp01(noise),1.35f); const auto sp=profile(speakerModel);
    CartridgeMacroTargets t; const float baseBits[]={14,10,12,13,8}, baseRate[]={42000,18000,30000,32000,16000}; const auto index=std::clamp((int)mode,0,4);
    t.bits=std::clamp((int)std::lround(baseBits[index]-(1-q)*6-c*3),2,16); t.converterRateHz=std::round(std::max(6000.0f,baseRate[index]*(.48f+.52f*q)-c*5000));
    t.jitter=clamp01(.01f+(1-q)*.20f+g*.20f); t.lowPassHz=std::round(std::max(2500.0f,sp.lp+(q-.5f)*4800)); t.highPassHz=std::round(sp.hp*(.45f+.55f*(1-q)));
    t.preEmphasis=clamp01(.08f+c*.42f); t.companding=clamp01(mode==CartridgeCodec::muLaw?.45f:c*.72f); t.blockMs=mode==CartridgeCodec::pcm?0.0f:std::round(3+c*15);
    t.saturation=clamp01(.05f+g*.64f); t.edge=clamp01(.04f+g*.72f); t.dcDrift=clamp01(.015f+g*.25f); t.hum=clamp01(.008f+n*.20f); t.whine=clamp01(.008f+n*.24f); t.noise=clamp01(.004f+n*.28f); t.noiseTracking=.45f+n*.3f;
    t.speaker=clamp01(speakerModel==CartridgeSpeaker::direct?.08f:.45f+(1-q)*.35f); t.microDelayMs=std::round(speakerModel==CartridgeSpeaker::cabinet?10.0f:speakerModel==CartridgeSpeaker::television?6.0f:3.0f);
    t.microDelayMix=clamp01((speakerModel==CartridgeSpeaker::direct?0.0f:.04f)+g*.12f); t.room=clamp01((speakerModel==CartridgeSpeaker::cabinet?.14f:speakerModel==CartridgeSpeaker::television?.08f:.025f)+g*.08f); t.roomMs=std::round(24+t.room*160);
    t.limiter=.18f+g*.45f; t.ceiling=.95f-g*.08f; t.outputGain=.95f; return t;
}

float CartridgeProcessor::Biquad::process(float x) noexcept {const auto y=b0*x+z1;z1=b1*x-a1*y+z2;z2=b2*x-a2*y;return y;}
void CartridgeProcessor::Biquad::setHighPass(double sr,float f,float q) noexcept {f=std::clamp(f,10.0f,(float)sr*.45f);const auto w=2*pi*f/(float)sr,c=std::cos(w),a=std::sin(w)/(2*std::max(.08f,q)),a0=1+a;b0=((1+c)*.5f)/a0;b1=-(1+c)/a0;b2=b0;a1=-2*c/a0;a2=(1-a)/a0;}
void CartridgeProcessor::Biquad::setLowPass(double sr,float f,float q) noexcept {f=std::clamp(f,20.0f,(float)sr*.45f);const auto w=2*pi*f/(float)sr,c=std::cos(w),a=std::sin(w)/(2*std::max(.08f,q)),a0=1+a;b0=((1-c)*.5f)/a0;b1=(1-c)/a0;b2=b0;a1=-2*c/a0;a2=(1-a)/a0;}
void CartridgeProcessor::Biquad::setPeak(double sr,float f,float q,float db) noexcept {f=std::clamp(f,20.0f,(float)sr*.45f);const auto w=2*pi*f/(float)sr,c=std::cos(w),a=std::sin(w)/(2*std::max(.08f,q)),gain=std::pow(10.0f,db/40),a0=1+a/gain;b0=(1+a*gain)/a0;b1=-2*c/a0;b2=(1-a*gain)/a0;a1=b1;a2=(1-a/gain)/a0;}

void CartridgeProcessor::prepare(double sr,std::size_t channels){sampleRate_=std::clamp(sr,8000.0,384000.0);preparedChannels_=std::clamp(channels,std::size_t{1},maxChannels);for(auto& s:channel_){s.delay.assign((std::size_t)std::ceil(sampleRate_*.05)+4,0);s.roomA.assign((std::size_t)std::ceil(sampleRate_*.18)+4,0);s.roomB.assign((std::size_t)std::ceil(sampleRate_*.18)+4,0);s.romHistory.assign((std::size_t)std::ceil(sampleRate_*.75)+4,0);}reset(seed_);}
void CartridgeProcessor::reset(std::uint32_t seed) noexcept
{
    seed_=seed==0?0xfeedc0deu:seed;for(std::size_t ch=0;ch<2;++ch){codecRandom_[ch]=seed_^(0x434f4445u+(std::uint32_t)ch*0x9e3779b9u);textureRandom_[ch]=seed_^(0x4e4f4953u+(std::uint32_t)ch*0x85ebca6bu);auto& s=channel_[ch];std::fill(s.delay.begin(),s.delay.end(),0.0f);std::fill(s.roomA.begin(),s.roomA.end(),0.0f);std::fill(s.roomB.begin(),s.roomB.end(),0.0f);std::fill(s.romHistory.begin(),s.romHistory.end(),0.0f);s.delayIndex=s.roomAIndex=s.roomBIndex=s.romWriteIndex=s.stallReadIndex=0;for(auto& f:s.filters)f.reset();s.preZ=s.deZ=s.previous1=s.previous2=s.noiseError=s.hold=s.dc=s.envelope=s.limiterEnvelope=s.roomLpA=s.roomLpB=0;s.adpcmStep=.02f;s.holdCount=s.blockRemaining=0;}
    bleepRandom_=seed_^0x0b1ee0f5u;humPhase_=whinePhase_=inputEnvelope_=0;bleepRemaining_=bleepCooldown_=bleepClock_=bleepStep_=0;bleepTotal_=1;bleepPhase_=bleepVibratoPhase_=0;manualBleep_=false;stallRemaining_=bankRemaining_=0;stallTotal_=bankTotal_=stallRepeatSamples_=1;stallStrength_=bankStrength_=0;bankSegment_=0;inputPeak_.fill(0);outputPeak_.fill(0);
}

void CartridgeProcessor::setBleepClockSamples(int samplesUntilNext) noexcept
{
    bleepClock_ = std::max(0, samplesUntilNext);
    bleepCooldown_ = 0;
}
void CartridgeProcessor::triggerBleep() noexcept { manualBleep_ = true; bleepCooldown_ = 0; }
void CartridgeProcessor::triggerRomStall(float strength, int durationSamples, int repeatSamples) noexcept
{
    stallStrength_=clamp01(strength);stallTotal_=stallRemaining_=std::max(1,durationSamples);stallRepeatSamples_=std::max(1,repeatSamples);
    for(auto& s:channel_){const auto back=(std::size_t)std::min<int>((int)s.romHistory.size()-1,std::max(stallRepeatSamples_*2,32));s.stallReadIndex=(s.romWriteIndex+s.romHistory.size()-back)%s.romHistory.size();}
}
void CartridgeProcessor::triggerBankFault(float strength, int durationSamples) noexcept
{
    bankStrength_=clamp01(strength);bankTotal_=bankRemaining_=std::max(1,durationSamples);bankSegment_=(int)(nextU32(bleepRandom_)&7u);
}
std::uint32_t CartridgeProcessor::nextU32(std::uint32_t& s) noexcept {auto x=s==0?0x12345678u:s;x^=x<<13u;x^=x>>17u;x^=x<<5u;s=x;return x;}
float CartridgeProcessor::nextFloat(std::uint32_t& s) noexcept{return(float)((double)nextU32(s)/4294967295.0);} float CartridgeProcessor::nextSigned(std::uint32_t& s) noexcept{return nextFloat(s)*2-1;}
float CartridgeProcessor::read(const std::vector<float>& b,std::size_t w,float d) const noexcept {if(b.empty())return 0;auto p=(float)w-std::clamp(d,1.0f,(float)b.size()-2);while(p<0)p+=(float)b.size();const auto base=std::floor(p);const auto a=(std::size_t)base%b.size(),c=(a+1)%b.size();return b[a]+(b[c]-b[a])*(p-base);}
void CartridgeProcessor::updateFilters(const CartridgeParameters& p) noexcept
{
    const auto sp=profile(p.speakerModel); const auto amount=clamp01(p.speaker);
    const auto hp=p.highPassHz*(1-amount)+std::max(p.highPassHz,sp.hp)*amount,lp=15000*(1-amount)+sp.lp*amount;
    for(auto& s:channel_){s.filters[0].setLowPass(sampleRate_,p.lowPassHz,.707f);s.filters[1].setHighPass(sampleRate_,hp,.707f);s.filters[2].setPeak(sampleRate_,sp.dipHz,.9f,sp.dipDb*amount);s.filters[3].setPeak(sampleRate_,sp.humpHz,sp.q,sp.humpDb*amount);s.filters[4].setLowPass(sampleRate_,lp,.85f);s.filters[5].setLowPass(sampleRate_,lp,.85f);}
}

float CartridgeProcessor::codecSample(ChannelState& s,float y,const CartridgeParameters& p,std::size_t ch) noexcept
{
    const auto bits=std::clamp(p.bits,2,16); const auto levels=(float)((1<<(bits-1))-1); const auto mode=p.codecMode;
    float decoded=y;
    if(mode==CartridgeCodec::dpcm){const auto prediction=s.previous1*.86f,residual=std::clamp(y-prediction,-1.0f,1.0f);decoded=std::clamp(prediction+std::round(residual*levels)/levels,-1.0f,1.0f);}
    else if(mode==CartridgeCodec::adpcm){const auto prediction=1.55f*s.previous1-.62f*s.previous2,residual=std::clamp(y-prediction,-1.0f,1.0f),step=std::clamp(s.adpcmStep,.002f,.25f);const auto code=std::clamp(std::round(residual/step),-7.0f,7.0f);decoded=std::clamp(prediction+code*step,-1.0f,1.0f);s.adpcmStep=std::clamp(step*(.92f+.12f*std::abs(code)),.002f,.25f);}
    else if(mode==CartridgeCodec::brr){const auto prediction=1.3f*s.previous1-.45f*s.previous2,residual=std::clamp(y-prediction,-1.0f,1.0f);decoded=std::clamp(prediction+std::round(residual*levels*.55f)/(levels*.55f),-1.0f,1.0f);}
    else if(mode==CartridgeCodec::muLaw){const auto mu=255.0f,enc=std::copysign(std::log1p(mu*std::abs(std::clamp(y,-1.0f,1.0f)))/std::log1p(mu),y),encLevels=std::max(31.0f,127-p.companding*96);const auto q=std::round(enc*encLevels)/encLevels;decoded=std::copysign(std::expm1(std::abs(q)*std::log1p(mu))/mu,q);}
    else decoded=std::round(std::clamp(y,-1.0f,1.0f)*levels)/levels;
    if(p.dither)decoded+=nextSigned(codecRandom_[ch])*(.35f/std::max(2.0f,levels));if(p.noiseShaping){decoded+=s.noiseError*.65f;const auto q=std::round(decoded*levels)/levels;s.noiseError=decoded-q;decoded=q;}
    s.previous2=s.previous1;s.previous1=decoded;return decoded;
}

float CartridgeProcessor::bleepSample(float magnitude,const CartridgeParameters& p) noexcept
{
    if(!p.bleepsEnabled||p.bleepMix<=.0001f||p.bleepRate<=.0001f){bleepRemaining_=0;return 0;}const auto sr=(float)sampleRate_,attack=1-std::exp(-1/(.0025f*sr)),release=1-std::exp(-1/(.055f*sr));const auto previous=inputEnvelope_;inputEnvelope_+=(magnitude-inputEnvelope_)*(magnitude>inputEnvelope_?attack:release);if(bleepCooldown_>0)--bleepCooldown_;if(bleepClock_>0)--bleepClock_;
    const auto transient=magnitude>.018f&&magnitude>previous*1.85f&&bleepCooldown_<=0,clocked=bleepClock_<=0;const auto trigger=manualBleep_||(p.bleepTrigger==BleepTrigger::transient?transient:p.bleepTrigger==BleepTrigger::clock?clocked:(transient||clocked));
    if(trigger&&bleepRemaining_<=0){manualBleep_=false;static constexpr int scales[4][8]={{0,3,5,7,10,12,15,17},{0,2,3,5,7,8,10,12},{0,2,4,5,7,9,11,12},{0,1,2,3,4,5,6,7}};static constexpr int melody[12]={0,4,2,5,1,3,2,0,5,3,1,4};const auto si=std::clamp((int)p.bleepScale,0,3),note=36+(int)std::lround(clamp01(p.bleepPitch)*24)+scales[si][melody[bleepStep_%12]%8];bleepFrequency_=440*std::pow(2.0f,(note-69)/12.0f);++bleepStep_;bleepTotal_=bleepRemaining_=std::max(8,(int)std::lround((.035f+.065f*(1-clamp01(p.bleepPitch)))*sr));bleepPhase_=nextFloat(bleepRandom_);bleepVibratoPhase_=nextFloat(bleepRandom_);bleepAmplitude_=std::min(.15f,.055f+.07f*std::min(1.0f,magnitude*4+.25f));activeWave_=p.bleepWave==BleepWave::alternate?(bleepStep_%4==0?BleepWave::triangle:BleepWave::pulse):p.bleepWave;bleepDuty_=bleepStep_%3==0?.125f:bleepStep_%3==1?.25f:.5f;bleepCooldown_=std::max(1,(int)(sr/std::max(1.0f,p.bleepRate*1.5f)));bleepClock_=std::max(1,(int)(sr/std::max(.15f,p.bleepRate)));}
    if(bleepRemaining_<=0)return 0;const auto t=1-(float)bleepRemaining_/bleepTotal_,env=std::min(1.0f,t/.035f)*std::pow(1-t,1.65f),vib=std::sin(bleepVibratoPhase_*2*pi)*clamp01(p.bleepVibrato)*clamp01(p.bleepVibrato)*.012f;bleepPhase_+=bleepFrequency_*(1+vib)/sr;bleepVibratoPhase_+=5.2f/sr;const auto phase=bleepPhase_-std::floor(bleepPhase_);float osc=0;if(activeWave_==BleepWave::saw)osc=2*(phase-.5f);else if(activeWave_==BleepWave::triangle)osc=1-4*std::abs(phase-.5f);else if(activeWave_==BleepWave::noise)osc=nextSigned(bleepRandom_);else osc=phase<bleepDuty_?1.0f:-1.0f;--bleepRemaining_;return std::clamp(osc*bleepAmplitude_*env*clamp01(p.bleepMix),-.16f,.16f);
}

void CartridgeProcessor::process(float* const* channels,std::size_t channelCount,std::size_t sampleCount,const CartridgeParameters& raw) noexcept
{
    const auto count=std::min({channelCount,preparedChannels_,maxChannels});if(count==0||sampleCount==0||channels==nullptr)return;auto p=raw;p.lowPassHz=std::clamp(p.lowPassHz,2500.0f,18000.0f);p.highPassHz=std::clamp(p.highPassHz,20.0f,420.0f);updateFilters(p);
    const auto sr=(float)sampleRate_; const auto rate=std::clamp(p.converterRateHz,6000.0f,std::min(48000.0f,sr));
    const auto period=std::max(1,(int)std::lround(sr/rate)); const auto block=std::max(0,(int)std::lround(p.blockMs*.001f*sr));
    const auto sat=1+clamp01(p.saturation)*4.2f,edge=1+clamp01(p.edge)*5,edgeBias=clamp01(p.edge)*.035f;
    const auto delaySamples=std::clamp(p.microDelayMs,0.0f,30.0f)*.001f*sr,roomSamples=std::clamp(p.roomMs,10.0f,120.0f)*.001f*sr,roomFeedback=.08f+clamp01(p.room)*.52f,roomDamp=.18f+clamp01(p.room)*.48f,limAttack=std::exp(-1/(sr*.0015f)),limRelease=std::exp(-1/(sr*.05f)),mix=clamp01(p.mix),ceiling=std::clamp(p.ceiling,.2f,1.0f);inputPeak_.fill(0);outputPeak_.fill(0);
    for(std::size_t i=0;i<sampleCount;++i){float magnitude=0;for(std::size_t ch=0;ch<count;++ch)magnitude=std::max(magnitude,std::abs(channels[ch][i]));const auto chip=bleepSample(magnitude,p);humPhase_+=2*pi*60/sr;whinePhase_+=2*pi*(780+1180*clamp01(p.whine))/sr;if(humPhase_>2*pi)humPhase_-=2*pi;if(whinePhase_>2*pi)whinePhase_-=2*pi;
        for(std::size_t ch=0;ch<count;++ch){auto& s=channel_[ch];const auto dry=channels[ch][i];inputPeak_[ch]=std::max(inputPeak_[ch],std::abs(dry));auto y=(dry*std::clamp(p.inputGain,0.0f,4.0f)+chip);y=s.filters[0].process(y);const auto diff=y-s.preZ;s.preZ=y;y+=diff*(.6f*clamp01(p.preEmphasis));s.dc=std::clamp(s.dc*.999995f+nextSigned(textureRandom_[ch])*clamp01(p.dcDrift)*clamp01(p.dcDrift)*3e-6f,-.018f,.018f);y+=s.dc;
            s.romHistory[s.romWriteIndex]=y;if(stallRemaining_>0){const auto stalled=s.romHistory[s.stallReadIndex];y=y*(1-stallStrength_)+stalled*stallStrength_;s.stallReadIndex=(s.stallReadIndex+1)%s.romHistory.size();if(stallRepeatSamples_>0&&((stallTotal_-stallRemaining_+1)%stallRepeatSamples_==0)){const auto back=(std::size_t)std::min<int>((int)s.romHistory.size()-1,stallRepeatSamples_);s.stallReadIndex=(s.romWriteIndex+s.romHistory.size()-back)%s.romHistory.size();}}s.romWriteIndex=(s.romWriteIndex+1)%s.romHistory.size();
            const auto wear=clamp01(p.addressWear);if(bankRemaining_>0||wear>.0001f){const auto strength=bankRemaining_>0?bankStrength_:wear;const auto segment=(((bankTotal_-bankRemaining_)>>4)+bankSegment_+(int)ch*3)&7;if(bankRemaining_>0||nextFloat(codecRandom_[ch])<wear*.0015f){const auto folded=std::floor((y+1.0f)*8.0f);const auto corrupt=(segment&1)?-y:y;y=(1-strength)*y+strength*std::clamp(corrupt+(folded-(float)segment)*.018f*clamp01(p.busDepth),-1.0f,1.0f);}}
            if(block>0&&s.blockRemaining<=0){s.blockRemaining=block;
                // A cartridge block carries a predictor seed.  Collapsing the
                // predictor toward zero at every boundary created periodic
                // full-band clicks; seed from the current source sample while
                // retaining the intended block-by-block codec character.
                s.previous1=y;s.previous2=y;s.adpcmStep=std::clamp(std::abs(y)*.18f+.008f,.004f,.18f);}if(s.blockRemaining>0)--s.blockRemaining;const auto center=std::tanh(edgeBias*edge);y=(std::tanh((y+edgeBias)*edge)-center)*(.18f/std::max(1e-5f,std::tanh(.18f*edge)));
            if(s.holdCount<=0){s.hold=y;s.holdCount=std::max(1,(int)std::lround(period*(1+nextSigned(codecRandom_[ch])*.45f*clamp01(p.jitter))));}y=s.hold;--s.holdCount;y=codecSample(s,y,p,ch);const auto a=std::abs(y),envC=a>s.envelope?std::exp(-1/(sr*.004f)):std::exp(-1/(sr*.08f));s.envelope=a+envC*(s.envelope-a);const auto inv=1-std::clamp(s.envelope*3.2f,0.0f,1.0f),hiss=clamp01(p.noise)*clamp01(p.noise)*.012f*(1-clamp01(p.noiseTracking)+clamp01(p.noiseTracking)*(.35f+.65f*inv));y+=std::sin(humPhase_)*clamp01(p.hum)*clamp01(p.hum)*.012f+std::sin(whinePhase_)*clamp01(p.whine)*clamp01(p.whine)*.009f+nextSigned(textureRandom_[ch])*hiss;y=std::tanh(y*sat)*(.2f/std::max(1e-5f,std::tanh(.2f*sat)));
            const auto tap=read(s.delay,s.delayIndex,std::max(1.0f,delaySamples));s.delay[s.delayIndex]=y;s.delayIndex=(s.delayIndex+1)%s.delay.size();y=y*(1-clamp01(p.microDelayMix))+tap*clamp01(p.microDelayMix);const auto tapA=read(s.roomA,s.roomAIndex,std::max(1.0f,roomSamples*.61f)),tapB=read(s.roomB,s.roomBIndex,std::max(1.0f,roomSamples*.89f));s.roomLpA+=((tapA)-s.roomLpA)*(1-roomDamp);s.roomLpB+=((tapB)-s.roomLpB)*(1-roomDamp);s.roomA[s.roomAIndex]=std::clamp(y*.18f+s.roomLpB*roomFeedback,-1.2f,1.2f);s.roomB[s.roomBIndex]=std::clamp(y*.18f+s.roomLpA*roomFeedback,-1.2f,1.2f);s.roomAIndex=(s.roomAIndex+1)%s.roomA.size();s.roomBIndex=(s.roomBIndex+1)%s.roomB.size();y=y*(1-clamp01(p.room)*.45f)+(tapA+tapB)*.22f*clamp01(p.room);
            for(std::size_t f=1;f<s.filters.size();++f)y=s.filters[f].process(y);auto processed=y*std::clamp(p.outputGain,0.0f,1.5f);const auto mag=std::abs(processed),lc=mag>s.limiterEnvelope?limAttack:limRelease;s.limiterEnvelope=mag+lc*(s.limiterEnvelope-mag);const auto wanted=s.limiterEnvelope>ceiling?ceiling/(s.limiterEnvelope+1e-6f):1.0f;processed*=1-clamp01(p.limiter)+clamp01(p.limiter)*wanted;processed=std::clamp(processed,-ceiling,ceiling);const auto output=std::clamp(dry*(1-mix)+processed*mix,-1.0f,1.0f);channels[ch][i]=output;outputPeak_[ch]=std::max(outputPeak_[ch],std::abs(output));}if(stallRemaining_>0)--stallRemaining_;if(bankRemaining_>0)--bankRemaining_;}
}
}
