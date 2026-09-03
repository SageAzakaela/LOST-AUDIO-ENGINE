#include <lost_audio/core/CamcorderProcessor.h>

#include <algorithm>
#include <cmath>

namespace lost_audio::core
{
namespace
{
constexpr float pi = 3.14159265358979323846f;
float clamp01(float value) noexcept { return std::clamp(value, 0.0f, 1.0f); }
struct FormatBase { float hp, lp, body, bodyHz, rate, flutter, hiss, motor; int bits; };
FormatBase baseFor(CamcorderFormat format) noexcept
{
    switch (format)
    {
        case CamcorderFormat::vhsc:      return { 62, 8600, 4.1f, 1420, 28000, .48f, .18f, .34f, 12 };
        case CamcorderFormat::video8:    return { 70, 10300, 3.5f, 1540, 32000, .34f, .14f, .27f, 13 };
        case CamcorderFormat::digicam:   return { 95, 12500, 2.8f, 1920, 26000, .08f, .09f, .08f, 12 };
        case CamcorderFormat::actionCam: return { 120, 15100, 3.6f, 2250, 32000, .10f, .08f, .05f, 13 };
        case CamcorderFormat::miniDV:
        default:                         return { 72, 13200, 2.6f, 1780, 32000, .12f, .10f, .14f, 14 };
    }
}
struct MicBase { float hpAdd, lpScale, bodyAdd, contact; };
MicBase micFor(CameraMic mic) noexcept
{
    switch (mic)
    {
        case CameraMic::cheapMono:     return { 85, .58f, 2.4f, 1.18f };
        case CameraMic::stereoCapsule: return { -18, 1.12f, -.7f, .82f };
        case CameraMic::waterproof:    return { 115, .48f, 3.3f, 1.35f };
        case CameraMic::shotgun:       return { 22, 1.18f, -.9f, .70f };
        case CameraMic::electret:
        default:                       return { 0, 1.0f, 0, 1.0f };
    }
}
}

CamcorderMacroTargets mapCamcorderMacros(CamcorderFormat format, CameraMic mic, float coverage,
                                          float movement, float corruption, float agcDrive) noexcept
{
    const auto cov = std::pow(clamp01(coverage), 1.30f), mov = std::pow(clamp01(movement), 1.22f);
    const auto damage = std::pow(clamp01(corruption), 1.32f), drive = std::pow(clamp01(agcDrive), 1.20f);
    const auto base = baseFor(format); const auto capsule = micFor(mic);
    CamcorderMacroTargets t;
    t.highPassHz = std::round(base.hp + capsule.hpAdd + cov * 95.0f);
    t.lowPassHz = std::round(std::max(950.0f, base.lp * capsule.lpScale - cov * (base.lp * capsule.lpScale - 2600.0f)));
    t.bodyDb = std::round(std::clamp(base.body + capsule.bodyAdd + cov * 5.4f, 0.0f, 14.0f) * 20.0f) / 20.0f;
    t.bodyHz = std::round(base.bodyHz + cov * 260.0f);
    t.agcAmount = clamp01(.34f + drive * .52f + cov * .12f);
    t.agcSpeed = clamp01(.22f + drive * .58f);
    t.agcPump = clamp01(.18f + drive * .55f + mov * .12f);
    t.clip = clamp01(.04f + drive * .70f);
    t.crush = clamp01(.025f + damage * .70f);
    t.bits = std::clamp((int) std::lround((float) base.bits - damage * 7.0f), 4, 16);
    t.converterRateHz = std::round(std::max(8000.0f, base.rate - damage * (base.rate - 9000.0f)));
    t.flutter = clamp01(base.flutter + mov * (format <= CamcorderFormat::video8 ? .46f : .12f) + damage * .12f);
    t.dropout = clamp01(.01f + damage * .72f);
    t.dropoutMs = std::round(12.0f + damage * 128.0f);
    t.repeatMs = std::round(24.0f + damage * 132.0f);
    t.chirp = clamp01(damage * .62f);
    t.handling = clamp01((.035f + mov * .72f) * capsule.contact);
    t.rub = clamp01((.025f + mov * .58f) * capsule.contact);
    t.hiss = clamp01(base.hiss + cov * .12f + damage * .07f);
    t.motorBleed = clamp01(base.motor + mov * .08f);
    t.outputGain = std::round((.94f + drive * .10f) * 100.0f) / 100.0f;
    t.ceiling = .94f - drive * .09f;
    return t;
}

float CamcorderProcessor::Biquad::process(float input) noexcept
{ const auto output = b0 * input + z1; z1 = b1 * input - a1 * output + z2; z2 = b2 * input - a2 * output; return output; }
void CamcorderProcessor::Biquad::setHighPass(double sr, float frequency, float q) noexcept
{
    const auto f = std::clamp(frequency, 10.0f, (float) sr * .45f), w = 2 * pi * f / (float) sr;
    const auto c = std::cos(w), alpha = std::sin(w) / (2 * std::max(.08f, q)), a0 = 1 + alpha;
    b0 = ((1 + c) * .5f) / a0; b1 = -(1 + c) / a0; b2 = b0; a1 = -2 * c / a0; a2 = (1 - alpha) / a0;
}
void CamcorderProcessor::Biquad::setLowPass(double sr, float frequency, float q) noexcept
{
    const auto f = std::clamp(frequency, 20.0f, (float) sr * .45f), w = 2 * pi * f / (float) sr;
    const auto c = std::cos(w), alpha = std::sin(w) / (2 * std::max(.08f, q)), a0 = 1 + alpha;
    b0 = ((1 - c) * .5f) / a0; b1 = (1 - c) / a0; b2 = b0; a1 = -2 * c / a0; a2 = (1 - alpha) / a0;
}
void CamcorderProcessor::Biquad::setPeak(double sr, float frequency, float q, float gainDb) noexcept
{
    const auto f = std::clamp(frequency, 20.0f, (float) sr * .45f), w = 2 * pi * f / (float) sr;
    const auto c = std::cos(w), alpha = std::sin(w) / (2 * std::max(.08f, q)), gain = std::pow(10.0f, gainDb / 40.0f), a0 = 1 + alpha / gain;
    b0 = (1 + alpha * gain) / a0; b1 = -2 * c / a0; b2 = (1 - alpha * gain) / a0; a1 = b1; a2 = (1 - alpha / gain) / a0;
}

void CamcorderProcessor::prepare(double sampleRate, std::size_t channelCount)
{
    sampleRate_ = std::clamp(sampleRate, 8000.0, 384000.0); preparedChannels_ = std::clamp(channelCount, std::size_t { 1 }, maxChannels);
    latencySamples_ = std::max(1, (int) std::lround(sampleRate_ * .003));
    const auto size = (std::size_t) std::ceil(sampleRate_ * .9) + (std::size_t) latencySamples_ + 8;
    for (auto& state : channel_) { state.history.assign(size, 0); state.dryHistory.assign(size, 0); }
    reset(seed_);
}
void CamcorderProcessor::reset(std::uint32_t seed) noexcept
{
    seed_ = seed == 0 ? 0x43414d45u : seed;
    transportRandom_ = seed_ ^ 0x74617065u; corruptionRandom_ = seed_ ^ 0x636f7272u;
    handlingRandom_ = seed_ ^ 0x68616e64u; windRandom_ = seed_ ^ 0x77696e64u;
    for (std::size_t ch = 0; ch < maxChannels; ++ch)
    {
        codecRandom_[ch] = seed_ ^ (0x434f4445u + (std::uint32_t) ch * 0x9e3779b9u);
        textureRandom_[ch] = seed_ ^ (0x54455854u + (std::uint32_t) ch * 0x85ebca6bu);
        windNoiseRandom_[ch] = seed_ ^ (0x57494e44u + (std::uint32_t) ch * 0xc2b2ae35u);
    }
    for (auto& state : channel_)
    {
        std::fill(state.history.begin(), state.history.end(), 0.0f); std::fill(state.dryHistory.begin(), state.dryHistory.end(), 0.0f); state.writeIndex = 0;
        for (auto& filter : state.tone) filter.reset(); state.micLow = state.micBand = state.envelope = state.muffle = state.held = state.lastGood = state.dropStart = state.dropoutBlend = state.limiter = 0;
        state.agcGain = 1; state.rubLow = state.rubBand = state.hissPrevious = state.windLow = state.windMid = 0; state.holdCount = 0;
    }
    flutterPhaseA_ = nextFloat(transportRandom_); flutterPhaseB_ = nextFloat(transportRandom_); flutterNoise_ = 0;
    dropoutRemaining_ = 0; dropoutTotal_ = 1; chirpRemaining_ = 0; chirpTotal_ = 1; chirpPhase_ = chirpAmplitude_ = 0;
    thumpRemaining_ = 0; thumpTotal_ = 1; thumpPhase_ = thumpAmplitude_ = 0; scrapeRemaining_ = 0; scrapeTotal_ = 1; scrapeEnvelope_ = 0;
    windRemaining_ = 0; windTotal_ = 1; windPhase_ = windAmplitude_ = 0; motorPhaseA_ = nextFloat(transportRandom_); motorPhaseB_ = nextFloat(transportRandom_);
    dropoutProgress_ = corruptionProgress_ = handlingProgress_ = windProgress_ = 0;
    agcActivity_ = flutterActivity_ = limiterActivity_ = 0;
    inputPeak_.fill(0); outputPeak_.fill(0);
}
void CamcorderProcessor::triggerDropout(float durationSeconds) noexcept
{
    dropoutTotal_ = dropoutRemaining_ = std::max(8, (int) std::lround(std::clamp(durationSeconds, .001f, 8.0f) * (float) sampleRate_));
    for (auto& state : channel_) state.dropStart = state.lastGood;
}
void CamcorderProcessor::triggerCodecFault(float strength, float durationSeconds) noexcept
{
    const auto amount = clamp01(strength);
    chirpTotal_ = chirpRemaining_ = std::max(8, (int) std::lround(std::clamp(durationSeconds, .001f, 4.0f) * (float) sampleRate_));
    chirpPhase_ = nextFloat(corruptionRandom_); chirpStartHz_ = 380.0f + 2100.0f * nextFloat(corruptionRandom_);
    chirpEndHz_ = chirpStartHz_ + 900.0f + 5200.0f * nextFloat(corruptionRandom_); chirpAmplitude_ = .008f + .085f * amount;
}
void CamcorderProcessor::triggerHandling(float strength) noexcept
{
    const auto amount = clamp01(strength);
    thumpTotal_ = thumpRemaining_ = std::max(8, (int) std::lround((.018f + .065f * amount) * (float) sampleRate_));
    thumpPhase_ = 0; thumpHz_ = 38.0f + 58.0f * nextFloat(handlingRandom_); thumpAmplitude_ = .025f + .24f * amount;
}
std::uint32_t CamcorderProcessor::nextU32(std::uint32_t& state) noexcept
{ auto x = state == 0 ? 0x12345678u : state; x ^= x << 13u; x ^= x >> 17u; x ^= x << 5u; state = x; return x; }
float CamcorderProcessor::nextFloat(std::uint32_t& state) noexcept { return (float) ((double) nextU32(state) / 4294967295.0); }
float CamcorderProcessor::nextSigned(std::uint32_t& state) noexcept { return nextFloat(state) * 2 - 1; }
float CamcorderProcessor::read(const std::vector<float>& history, std::size_t writeIndex, float delay) const noexcept
{
    if (history.empty()) return 0; auto position = (float) writeIndex - std::clamp(delay, 1.0f, (float) history.size() - 2);
    while (position < 0) position += (float) history.size(); const auto base = std::floor(position);
    const auto first = (std::size_t) base % history.size(), second = (first + 1) % history.size(); return history[first] + (history[second] - history[first]) * (position - base);
}
void CamcorderProcessor::updateFilters(const CamcorderParameters& p) noexcept
{
    for (auto& state : channel_)
    {
        state.tone[0].setHighPass(sampleRate_, p.highPassHz, .707f); state.tone[1].setPeak(sampleRate_, p.bodyHz, 1.2f, p.bodyDb);
        state.tone[2].setPeak(sampleRate_, 650, .9f, -.35f * p.bodyDb); state.tone[3].setLowPass(sampleRate_, p.lowPassHz, .85f); state.tone[4].setLowPass(sampleRate_, p.lowPassHz, .85f);
    }
}

void CamcorderProcessor::process(float* const* channels, std::size_t channelCount, std::size_t sampleCount, const CamcorderParameters& raw, const float* auxiliaryMono) noexcept
{
    const auto count = std::min({ channelCount, preparedChannels_, maxChannels }); if (count == 0 || sampleCount == 0 || channels == nullptr) return;
    auto p = raw; p.highPassHz = std::clamp(p.highPassHz, 10.0f, 1200.0f); p.lowPassHz = std::clamp(p.lowPassHz, 800.0f, 22000.0f);
    p.bodyDb = std::clamp(p.bodyDb, 0.0f, 14.0f); p.bodyHz = std::clamp(p.bodyHz, 650.0f, 4200.0f); updateFilters(p);
    if (!p.windEnabled) windRemaining_ = 0;
    const auto sr = (float) sampleRate_, coverage = clamp01(p.coverage), movement = clamp01(p.movement), corruption = clamp01(p.corruption);
    const auto micIndex = std::clamp((int) p.microphone, 0, 4); const float micHp[] { 75, 155, 55, 185, 90 }, micLp[] { 13200, 6800, 15500, 6100, 16500 }, contact[] { 1, 1.18f, .82f, 1.35f, .70f };
    const auto micHpA = std::exp(-2 * pi * micHp[micIndex] / sr), micLpA = std::exp(-2 * pi * micLp[micIndex] / sr);
    const auto target = .11f + clamp01(p.agcDrive) * .11f, attack = .002f + (1 - clamp01(p.agcSpeed)) * .02f, release = .05f + (1 - clamp01(p.agcSpeed)) * .28f;
    const auto envAttack = std::exp(-1 / (attack * sr)), envRelease = std::exp(-1 / (release * sr));
    const auto gainAttack = std::exp(-1 / ((.006f + (1 - clamp01(p.agcSpeed)) * .025f) * sr)), gainRelease = std::exp(-1 / ((.08f + (1 - clamp01(p.agcSpeed)) * .5f) * sr));
    const auto compPower = .12f + clamp01(p.agcAmount) * .72f, maxAgc = 1.4f + clamp01(p.agcAmount) * 5.6f;
    const auto baseCut = 14000 - coverage * 11500, loudClose = .35f + .55f * coverage;
    const auto formatIndex = std::clamp((int) p.format, 0, 4); const float depthMs[] { 1.15f, .58f, .07f, .025f, .06f }, flutterHz[] { .62f, .9f, 7.2f, 13, 9 };
    const auto flutterRateHz = p.flutterRateHz > 0.0f ? std::clamp(p.flutterRateHz, .05f, 40.0f) : flutterHz[formatIndex];
    const auto flutterDepth = clamp01(p.flutter) * clamp01(p.flutter) * depthMs[formatIndex] * (.35f + corruption * 1.35f) * .001f * sr;
    const auto digital = formatIndex >= 2 ? 1.0f : 0.0f;
    const auto effectiveBits = std::clamp((int) std::lround((float) std::clamp(p.bits, 4, 16) - clamp01(p.crush) * 6), 4, 16);
    const auto levels = (float) ((1 << (effectiveBits - 1)) - 1), bitMix = clamp01(std::max(clamp01(p.crush) * (.42f + digital * .58f), ((14 - p.bits) / 8.0f) * (.18f + digital * .82f)));
    const auto rate = std::clamp(p.converterRateHz, 8000.0f, std::min(48000.0f, sr));
    const auto basePeriod = std::max(1, (int) std::lround(sr / rate));
    const auto rateMix = clamp01((digital > 0 ? .18f : .06f)
                               + clamp01(p.crush) * (digital > 0 ? .62f : .28f)
                               + corruption * .12f);
    const auto dropSamples = std::max(8, (int) std::lround(std::clamp(p.dropoutMs, 1.0f, 500.0f) * .001f * sr));
    const auto dropChance = p.dropout <= .0001f ? 0.0f : (9e-7f + clamp01(p.dropout) * clamp01(p.dropout) * 6e-5f) * (1 + 3.2f * corruption);
    const auto repeatSamples = std::max(1.0f, std::clamp(p.repeatMs, 1.0f, 600.0f) * .001f * sr);
    const auto chirpChance = p.chirp <= .0001f ? 0.0f : std::pow(clamp01(p.chirp), 3.0f) * 2e-5f * (.3f + corruption * .7f);
    const auto thumpChance = (4e-6f + movement * movement * 3.5e-4f) * (.55f + clamp01(p.handling));
    const auto scrapeChance = movement <= .0001f ? 0.0f : movement * movement * 8e-6f;
    const auto windChance = p.windEnabled ? (1.1e-6f + movement * movement * 2.2e-4f) * .9f : 0.0f;
    const auto rubDepth = clamp01(p.rub) * clamp01(p.rub) * (.04f + .075f * movement) * contact[micIndex], hissDepth = clamp01(p.hiss) * clamp01(p.hiss) * .018f;
    const auto rubA90 = std::exp(-2 * pi * 90 / sr), rubA1800 = std::exp(-2 * pi * 1800 / sr);
    const auto drive = 1 + clamp01(p.agcDrive) * 2.7f, clip = clamp01(p.clip), asym = .018f * clamp01(p.agcDrive);
    const auto limiterAttack = std::exp(-1 / (.002f * sr)), limiterRelease = std::exp(-1 / (.06f * sr));
    const auto mix = clamp01(p.mix), ceiling = std::clamp(p.ceiling, .2f, 1.0f), windLevel = std::clamp(p.windLevel, 0.0f, 1.5f);
    inputPeak_.fill(0); outputPeak_.fill(0); agcActivity_ = flutterActivity_ = limiterActivity_ = 0;

    for (std::size_t sample = 0; sample < sampleCount; ++sample)
    {
        flutterNoise_ = flutterNoise_ * .9985f + nextSigned(transportRandom_) * .0015f; flutterPhaseA_ += flutterRateHz / sr; flutterPhaseB_ += (flutterRateHz * 6.83f + 1.7f) / sr;
        if (flutterPhaseA_ >= 1) --flutterPhaseA_; if (flutterPhaseB_ >= 1) --flutterPhaseB_;
        const auto flutterMod = std::clamp(std::sin(flutterPhaseA_ * 2 * pi) * .68f + std::sin(flutterPhaseB_ * 2 * pi) * .22f + flutterNoise_ * 1.8f, -1.0f, 1.0f);
        flutterActivity_ = std::max(flutterActivity_, std::abs(flutterMod) * clamp01(p.flutter));
        if (dropoutRemaining_ <= 0 && nextFloat(corruptionRandom_) < dropChance) { dropoutRemaining_ = dropoutTotal_ = dropSamples; for (std::size_t ch = 0; ch < count; ++ch) channel_[ch].dropStart = channel_[ch].lastGood; }
        if (chirpRemaining_ <= 0 && nextFloat(corruptionRandom_) < chirpChance)
        {
            const auto durationMs = 10 + nextFloat(corruptionRandom_) * (35 + clamp01(p.chirp) * 55); chirpRemaining_ = chirpTotal_ = std::max(8, (int) std::lround(durationMs * .001f * sr)); chirpPhase_ = nextFloat(corruptionRandom_);
            const auto analog = formatIndex <= 1; const auto base = analog ? 160 + nextFloat(corruptionRandom_) * 620 : 900 + nextFloat(corruptionRandom_) * 2500;
            chirpStartHz_ = base; chirpEndHz_ = analog ? base * (.72f + nextFloat(corruptionRandom_) * .75f) : base + 3000 + nextFloat(corruptionRandom_) * 5000;
            chirpAmplitude_ = (.010f + .072f * clamp01(p.chirp)) * (.65f + .7f * nextFloat(corruptionRandom_));
        }
        if (thumpRemaining_ <= 0 && p.handling > .0001f && nextFloat(handlingRandom_) < thumpChance)
        { thumpRemaining_ = thumpTotal_ = std::max(8, (int) std::lround((.010f + nextFloat(handlingRandom_) * .080f) * sr)); thumpPhase_ = 0; thumpHz_ = 35 + nextFloat(handlingRandom_) * 75; thumpAmplitude_ = (.03f + .24f * clamp01(p.handling)) * contact[micIndex] * (.75f + .65f * nextFloat(handlingRandom_)); }
        if (scrapeRemaining_ <= 0 && nextFloat(handlingRandom_) < scrapeChance)
        { scrapeRemaining_ = scrapeTotal_ = std::max(8, (int) std::lround((.008f + nextFloat(handlingRandom_) * .047f) * sr)); scrapeEnvelope_ = .02f + .13f * movement; }
        if (windRemaining_ <= 0 && nextFloat(windRandom_) < windChance)
        { windRemaining_ = windTotal_ = std::max(8, (int) std::lround((.035f + nextFloat(windRandom_) * .145f) * sr)); windPhase_ = nextFloat(windRandom_); windAmplitude_ = (.035f + .18f * movement) * (.75f + .65f * nextFloat(windRandom_)); }

        float chirpSignal = 0, thumpSignal = 0, windEnvelope = 0, scrapeEnv = 0;
        if (chirpRemaining_ > 0) { const auto t = 1 - (float) chirpRemaining_ / chirpTotal_, frequency = chirpStartHz_ + (chirpEndHz_ - chirpStartHz_) * t; chirpPhase_ += frequency / sr; if (chirpPhase_ >= 1) --chirpPhase_; chirpSignal = std::sin(chirpPhase_ * 2 * pi) * chirpAmplitude_ * std::sin(pi * t) * (1 - t); --chirpRemaining_; }
        if (thumpRemaining_ > 0) { const auto t = 1 - (float) thumpRemaining_ / thumpTotal_; thumpPhase_ += thumpHz_ / sr; if (thumpPhase_ >= 1) --thumpPhase_; thumpSignal = std::sin(thumpPhase_ * 2 * pi) * thumpAmplitude_ * std::pow(1 - t, 2.05f) * std::sin(std::min(1.0f, t / .1f) * pi * .5f); --thumpRemaining_; }
        if (windRemaining_ > 0) { const auto t = 1 - (float) windRemaining_ / windTotal_; windEnvelope = std::sin(pi * t) * (1 - .2f * t); windPhase_ += (35 + 55 * (1 - t)) / sr; if (windPhase_ >= 1) --windPhase_; --windRemaining_; }
        if (scrapeRemaining_ > 0) { const auto t = 1 - (float) scrapeRemaining_ / scrapeTotal_; scrapeEnv = std::pow(1 - t, 1.8f) * scrapeEnvelope_; --scrapeRemaining_; }
        motorPhaseA_ += (formatIndex <= 1 ? 60.0f : 120.0f) / sr; motorPhaseB_ += (formatIndex <= 1 ? 540.0f : 1850.0f) / sr; if (motorPhaseA_ >= 1) --motorPhaseA_; if (motorPhaseB_ >= 1) --motorPhaseB_;
        const auto motor = (std::sin(motorPhaseA_ * 2 * pi) * .65f + std::sin(motorPhaseB_ * 2 * pi) * .18f) * clamp01(p.motorBleed) * .012f;

        for (std::size_t ch = 0; ch < count; ++ch)
        {
            auto& state = channel_[ch]; const auto dryInput = channels[ch][sample]; const auto input = dryInput + (auxiliaryMono ? auxiliaryMono[sample] : 0.0f); inputPeak_[ch] = std::max(inputPeak_[ch], std::abs(input)); state.dryHistory[state.writeIndex] = dryInput;
            const auto drivenInput = input * std::clamp(p.inputGain, 0.0f, 4.0f); state.micLow = (1 - micHpA) * drivenInput + micHpA * state.micLow; const auto high = drivenInput - state.micLow;
            state.micBand = (1 - micLpA) * high + micLpA * state.micBand; auto x = state.micBand;
            const auto magnitude = std::abs(x), envCoefficient = magnitude > state.envelope ? envAttack : envRelease; state.envelope = magnitude + envCoefficient * (state.envelope - magnitude);
            const auto desired = 1 + (std::clamp(std::pow(target / (state.envelope + 1e-6f), compPower), .32f, maxAgc) - 1) * clamp01(p.agcAmount);
            const auto gainCoefficient = desired < state.agcGain ? gainAttack : gainRelease; state.agcGain = desired + gainCoefficient * (state.agcGain - desired);
            agcActivity_ = std::max(agcActivity_, std::min(1.0f, std::abs(state.agcGain - 1.0f) / std::max(1.0f, maxAgc - 1.0f)));
            const auto pumping = 1 - clamp01(p.agcPump) * .16f * std::min(1.0f, state.envelope * 4.0f); auto y = x * state.agcGain * pumping;
            if (clip > .0001f) { const auto amount = drive * (1 + clip * 3.4f), norm = std::tanh(std::max(1.0f, amount)); y = (std::tanh((y + asym) * amount) - std::tanh(asym * amount) * .7f) / std::max(.5f, norm); } else y *= 1 + clamp01(p.agcDrive) * .35f;
            const auto loud = std::clamp(state.envelope * 3.2f, 0.0f, 1.0f), cut = std::max(350.0f, baseCut * (1 - loud * loudClose * .55f)), coefficient = std::exp(-2 * pi * cut / sr);
            state.muffle = (1 - coefficient) * y + coefficient * state.muffle; y = state.muffle; for (auto& filter : state.tone) y = filter.process(y);
            state.history[state.writeIndex] = y; state.writeIndex = (state.writeIndex + 1) % state.history.size();
            y = read(state.history, state.writeIndex, (float) latencySamples_ + 1 + flutterDepth * (.52f + flutterMod * .48f));
            if (state.holdCount <= 0) { const auto jitter = corruption > 0 ? nextSigned(codecRandom_[ch]) * corruption * .35f : 0; state.holdCount = std::max(1, (int) std::lround(basePeriod * (1 + jitter))); state.held = y; }
            y = y * (1 - rateMix) + state.held * rateMix; --state.holdCount; const auto quantized = std::round(std::clamp(y, -1.0f, 1.0f) * levels) / levels; y += (quantized - y) * bitMix;
            const auto dropoutTarget = dropoutRemaining_ > 0 ? 1.0f : 0.0f;
            state.dropoutBlend += (dropoutTarget - state.dropoutBlend) / (dropoutTarget > state.dropoutBlend ? 96.0f : 192.0f);
            if (std::abs(state.dropoutBlend - dropoutTarget) < 1.0e-5f) state.dropoutBlend = dropoutTarget;
            if (dropoutRemaining_ > 0 || state.dropoutBlend > 1.0e-5f)
            {
                const auto t = 1 - (float) dropoutRemaining_ / dropoutTotal_;
                const auto healthy = y;
                auto concealed = state.lastGood;
                if (p.concealment == CameraConcealment::mute) concealed = 0;
                else if (p.concealment == CameraConcealment::interpolate) concealed = state.dropStart * (1 - t) + x * t;
                else if (p.concealment == CameraConcealment::repeat) concealed = read(state.history, state.writeIndex, repeatSamples + latencySamples_);
                y = healthy + (concealed - healthy) * state.dropoutBlend;
            }
            else state.lastGood = y;
            state.windLow += (nextSigned(windNoiseRandom_[ch]) - state.windLow) * .018f; state.windMid += (state.windLow - state.windMid) * .12f;
            const auto wind = p.windEnabled ? (std::sin(windPhase_ * 2 * pi) * windAmplitude_ + state.windMid * windAmplitude_ * .72f) * windEnvelope * windLevel : 0.0f;
            const auto scrapeNoise = nextSigned(textureRandom_[ch]); state.rubLow = (1 - rubA90) * scrapeNoise + rubA90 * state.rubLow; const auto rubHigh = scrapeNoise - state.rubLow; state.rubBand = (1 - rubA1800) * rubHigh + rubA1800 * state.rubBand;
            const auto hissNoise = nextSigned(textureRandom_[ch]), hiss = (hissNoise - state.hissPrevious) * hissDepth; state.hissPrevious = hissNoise;
            y += chirpSignal + thumpSignal + wind + state.rubBand * (rubDepth + scrapeEnv) + hiss + motor;
            auto processed = y * std::clamp(p.outputGain, 0.0f, 1.5f); const auto postMagnitude = std::abs(processed), limiterCoefficient = postMagnitude > state.limiter ? limiterAttack : limiterRelease;
            limiterActivity_ = std::max(limiterActivity_, std::max(0.0f, postMagnitude - ceiling) / std::max(.05f, ceiling));
            state.limiter = postMagnitude + limiterCoefficient * (state.limiter - postMagnitude); if (state.limiter > ceiling) processed *= ceiling / (state.limiter + 1e-6f); processed = std::clamp(processed, -ceiling, ceiling);
            const auto dry = read(state.dryHistory, state.writeIndex, (float) latencySamples_ + 1); const auto output = std::clamp(dry * (1 - mix) + processed * mix, -1.0f, 1.0f);
            channels[ch][sample] = output; outputPeak_[ch] = std::max(outputPeak_[ch], std::abs(output));
        }
        if (dropoutRemaining_ > 0) --dropoutRemaining_;
    }
    dropoutProgress_ = dropoutRemaining_ > 0 ? 1.0f - (float) dropoutRemaining_ / std::max(1, dropoutTotal_) : 0.0f;
    corruptionProgress_ = chirpRemaining_ > 0 ? 1.0f - (float) chirpRemaining_ / std::max(1, chirpTotal_) : 0.0f;
    const auto contactRemaining = std::max(thumpRemaining_, scrapeRemaining_);
    const auto contactTotal = thumpRemaining_ >= scrapeRemaining_ ? thumpTotal_ : scrapeTotal_;
    handlingProgress_ = contactRemaining > 0 ? 1.0f - (float) contactRemaining / std::max(1, contactTotal) : 0.0f;
    windProgress_ = windRemaining_ > 0 ? 1.0f - (float) windRemaining_ / std::max(1, windTotal_) : 0.0f;
}
}
