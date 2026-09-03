#include <lost_audio/core/ConferenceProcessor.h>

#include <algorithm>
#include <cmath>

namespace lost_audio::core
{
namespace
{
constexpr float pi = 3.14159265358979323846f;
float clamp01(float value) noexcept { return std::clamp(value, 0.0f, 1.0f); }

struct ModeBase { float hp, lp, mid, hump, rate, loss, suppress, noise, agc; int bits; };
ModeBase baseFor(ConferenceMode mode) noexcept
{
    switch (mode)
    {
        case ConferenceMode::zoom:     return { 145, 7200, 2100, 1.5f, 32000, 0.75f, 0.48f, 0.65f, 0.54f, 13 };
        case ConferenceMode::skype:    return { 230, 4700, 1680, 3.1f, 24000, 0.92f, 0.34f, 0.92f, 0.45f, 11 };
        case ConferenceMode::cellular: return { 310, 3400, 1820, 4.2f, 16000, 1.25f, 0.62f, 0.78f, 0.62f, 10 };
        case ConferenceMode::discord:
        default:                       return { 120, 8500, 2350, 1.2f, 48000, 1.00f, 0.52f, 0.58f, 0.58f, 14 };
    }
}
}

ConferenceMacroTargets mapConferenceMacros(ConferenceMode mode, float bandwidth, float codec,
                                             float dropouts, float jitter, float robot, float noise) noexcept
{
    const auto bw = clamp01(bandwidth), crush = std::pow(clamp01(codec), 1.15f);
    const auto loss = std::pow(clamp01(dropouts), 1.45f), wander = std::pow(clamp01(jitter), 1.25f);
    const auto bot = std::pow(clamp01(robot), 1.35f), bed = std::pow(clamp01(noise), 1.35f);
    const auto narrow = std::pow(1.0f - bw, 1.25f);
    const auto base = baseFor(mode);
    ConferenceMacroTargets t;
    t.highPassHz = std::round(base.hp + narrow * (mode == ConferenceMode::cellular ? 390.0f : 270.0f));
    t.lowPassHz = std::round(std::max(900.0f, base.lp - narrow * (base.lp - 1700.0f)));
    t.midHumpDb = std::round((base.hump + narrow * 5.5f) * 20.0f) / 20.0f;
    t.midFrequencyHz = std::round(base.mid - narrow * 330.0f);
    t.packetLoss = clamp01((0.008f + loss * 0.52f) * base.loss);
    t.packetMs = std::round(10.0f + loss * 52.0f + wander * 16.0f);
    t.repeatMs = std::round(12.0f + loss * 72.0f + bot * 34.0f);
    t.jitterMs = 0.04f + wander * 5.8f;
    t.jitterRate = 3.0f + wander * 52.0f;
    t.gate = clamp01(0.04f + crush * 0.25f + bed * 0.16f);
    t.bits = std::clamp((int) std::lround((float) base.bits - crush * 7.0f), 4, 16);
    t.converterRateHz = std::round(std::max(6000.0f, base.rate - crush * (base.rate - 7800.0f)));
    t.robot = clamp01(bot * 0.92f + loss * 0.12f);
    t.noise = clamp01(0.015f + bed * base.noise);
    t.burstiness = clamp01(0.18f + loss * 0.74f);
    t.suppression = clamp01(base.suppress + crush * 0.25f + bed * 0.10f);
    t.agc = clamp01(base.agc + crush * 0.22f);
    t.bufferSlip = clamp01(wander * 0.72f + loss * 0.18f);
    t.bandwidthSwitch = clamp01(loss * 0.28f + wander * 0.20f);
    t.comfortNoise = clamp01(0.08f + bed * 0.78f);
    t.outputGain = mode == ConferenceMode::cellular ? 0.93f : 0.98f;
    t.ceiling = mode == ConferenceMode::skype ? 0.89f : 0.92f;
    return t;
}

float ConferenceProcessor::Biquad::process(float input) noexcept
{
    const auto output = b0 * input + z1; z1 = b1 * input - a1 * output + z2; z2 = b2 * input - a2 * output; return output;
}

void ConferenceProcessor::Biquad::setHighPass(double sr, float frequency, float q) noexcept
{
    const auto f = std::clamp(frequency, 20.0f, (float) sr * 0.45f), w = 2.0f * pi * f / (float) sr;
    const auto c = std::cos(w), a = std::sin(w) / (2.0f * std::max(0.08f, q)), a0 = 1.0f + a;
    b0 = ((1.0f + c) * 0.5f) / a0; b1 = -(1.0f + c) / a0; b2 = b0; a1 = -2.0f * c / a0; a2 = (1.0f - a) / a0;
}
void ConferenceProcessor::Biquad::setLowPass(double sr, float frequency, float q) noexcept
{
    const auto f = std::clamp(frequency, 20.0f, (float) sr * 0.45f), w = 2.0f * pi * f / (float) sr;
    const auto c = std::cos(w), a = std::sin(w) / (2.0f * std::max(0.08f, q)), a0 = 1.0f + a;
    b0 = ((1.0f - c) * 0.5f) / a0; b1 = (1.0f - c) / a0; b2 = b0; a1 = -2.0f * c / a0; a2 = (1.0f - a) / a0;
}
void ConferenceProcessor::Biquad::setPeak(double sr, float frequency, float q, float gainDb) noexcept
{
    const auto f = std::clamp(frequency, 20.0f, (float) sr * 0.45f), w = 2.0f * pi * f / (float) sr;
    const auto c = std::cos(w), alpha = std::sin(w) / (2.0f * std::max(0.08f, q)), gain = std::pow(10.0f, gainDb / 40.0f);
    const auto a0 = 1.0f + alpha / gain; b0 = (1.0f + alpha * gain) / a0; b1 = -2.0f * c / a0;
    b2 = (1.0f - alpha * gain) / a0; a1 = b1; a2 = (1.0f - alpha / gain) / a0;
}

void ConferenceProcessor::prepare(double sampleRate, std::size_t channelCount)
{
    sampleRate_ = std::clamp(sampleRate, 8000.0, 384000.0);
    preparedChannels_ = std::clamp(channelCount, std::size_t { 1 }, maxChannels);
    // Keep the live path playable. Longer repeats use the history ring and do
    // not require the host to compensate a full codec-frame delay.
    latencySamples_ = std::max(1, (int) std::lround(sampleRate_ * 0.002));
    const auto historySize = (std::size_t) std::ceil(sampleRate_ * 0.75) + (std::size_t) latencySamples_ + 8;
    for (auto& state : channel_)
    {
        state.wetHistory.assign(historySize, 0.0f); state.dryHistory.assign(historySize, 0.0f);
        state.robotGrain.assign((std::size_t) std::ceil(sampleRate_ * 0.09) + 8, 0.0f);
    }
    reset(seed_);
}

void ConferenceProcessor::reset(std::uint32_t seed) noexcept
{
    seed_ = seed == 0 ? 0x636f6e66u : seed; randomState_ = seed_;
    for (auto& state : channel_)
    {
        std::fill(state.wetHistory.begin(), state.wetHistory.end(), 0.0f);
        std::fill(state.dryHistory.begin(), state.dryHistory.end(), 0.0f);
        std::fill(state.robotGrain.begin(), state.robotGrain.end(), 0.0f);
        state.writeIndex = 0; state.envelope = 0; state.gateGain = state.agcGain = 1; state.heldSample = 0;
        state.codecPrevious = state.smear = state.narrowSmear = state.narrowBlend = state.lastGood = state.lossStart = state.comfortState = state.lossBlend = 0; state.rateCounter = 0;
        for (auto& filter : state.tone) filter.reset();
    }
    frameRemaining_ = 0; frameLength_ = 1; badBurst_ = packetLost_ = false;
    manualLossRemaining_ = 0; manualLossTotal_ = 1; manualLossDepth_ = 1.0f; previousLossState_ = false;
    recoveryRemaining_ = 0; recoveryLength_ = 1; jitterTarget_ = jitterCurrent_ = 0;
    slipIndicator_ = narrowFrames_ = robotRemaining_ = robotLength_ = robotPosition_ = 0;
    robotTotal_ = 1; robotBlend_ = 1.0f; robotEnvelope_ = 0.0f; pendingRobotTrigger_ = false;
    requestedRobotDuration_ = requestedRobotGrain_ = 0; requestedRobotStrength_ = 1.0f;
    lossProgress_ = robotProgress_ = jitterActivity_ = suppressionActivity_ = 0.0f;
    agcActivity_ = comfortNoiseActivity_ = limiterActivity_ = 0.0f;
    inputPeak_.fill(0); outputPeak_.fill(0);
}

void ConferenceProcessor::triggerPacketLoss(float depth, float durationSeconds) noexcept
{
    manualLossDepth_ = clamp01(depth);
    manualLossTotal_ = std::max(1, (int) std::lround(std::clamp(durationSeconds, 0.001f, 8.0f) * (float) sampleRate_));
    manualLossRemaining_ = manualLossTotal_;
}

void ConferenceProcessor::triggerRobot(float strength, float durationSeconds, float grainMilliseconds) noexcept
{
    requestedRobotStrength_ = clamp01(strength);
    requestedRobotDuration_ = std::max(8, (int) std::lround(std::clamp(durationSeconds, 0.001f, 8.0f) * (float) sampleRate_));
    requestedRobotGrain_ = std::max(8, (int) std::lround(std::clamp(grainMilliseconds, 2.0f, 90.0f) * 0.001f * (float) sampleRate_));
    pendingRobotTrigger_ = true;
}

std::uint32_t ConferenceProcessor::nextU32(std::uint32_t& state) noexcept
{
    auto x = state == 0 ? 0x12345678u : state; x ^= x << 13u; x ^= x >> 17u; x ^= x << 5u; state = x; return x;
}
float ConferenceProcessor::nextFloat(std::uint32_t& state) noexcept { return (float) ((double) nextU32(state) / 4294967295.0); }
float ConferenceProcessor::nextSigned(std::uint32_t& state) noexcept { return nextFloat(state) * 2.0f - 1.0f; }

float ConferenceProcessor::readHistory(const std::vector<float>& history, std::size_t writeIndex, float delaySamples) const noexcept
{
    if (history.empty()) return 0.0f;
    auto read = (float) writeIndex - std::clamp(delaySamples, 1.0f, (float) history.size() - 2.0f);
    while (read < 0.0f) read += (float) history.size();
    const auto base = std::floor(read); const auto first = (std::size_t) base % history.size(); const auto second = (first + 1) % history.size();
    return history[first] + (history[second] - history[first]) * (read - base);
}

void ConferenceProcessor::updateFilters(const ConferenceParameters& p) noexcept
{
    for (auto& state : channel_)
    {
        state.tone[0].setHighPass(sampleRate_, p.highPassHz, 0.707f); state.tone[1].setHighPass(sampleRate_, p.highPassHz, 0.707f);
        state.tone[2].setPeak(sampleRate_, 650.0f, 0.9f, -0.35f * p.midHumpDb);
        state.tone[3].setPeak(sampleRate_, p.midFrequencyHz, 1.25f, p.midHumpDb);
        state.tone[4].setLowPass(sampleRate_, p.lowPassHz, 0.85f); state.tone[5].setLowPass(sampleRate_, p.lowPassHz, 0.85f);
    }
}

void ConferenceProcessor::process(float* const* channels, std::size_t channelCount, std::size_t sampleCount,
                                  const ConferenceParameters& raw) noexcept
{
    const auto count = std::min({ channelCount, preparedChannels_, maxChannels });
    if (count == 0 || sampleCount == 0 || channels == nullptr) return;
    auto p = raw; p.highPassHz = std::clamp(p.highPassHz, 40.0f, 1200.0f); p.lowPassHz = std::clamp(p.lowPassHz, 800.0f, 16000.0f);
    p.midHumpDb = std::clamp(p.midHumpDb, 0.0f, 14.0f); p.midFrequencyHz = std::clamp(p.midFrequencyHz, 600.0f, 5000.0f);
    updateFilters(p);
    const auto sr = (float) sampleRate_; const auto packetSamples = std::max(1, (int) std::lround(std::clamp(p.packetMs, 4.0f, 240.0f) * 0.001f * sr));
    const auto repeatSamples = std::max(1.0f, std::clamp(p.repeatMs, 1.0f, 300.0f) * 0.001f * sr);
    const auto lossChance = clamp01(p.packetLoss), burst = clamp01(p.burstiness), robot = clamp01(p.robot);
    const auto converterRate = std::clamp(p.converterRateHz, 6000.0f, std::min(48000.0f, sr));
    const auto holdPeriod = std::max(1, (int) std::lround(sr / converterRate));
    const auto bits = std::clamp(p.bits, 4, 16); const auto quant = (float) ((1 << (bits - 1)) - 1);
    const auto envelopeAttack = std::exp(-1.0f / (0.0025f * sr)), envelopeRelease = std::exp(-1.0f / (0.090f * sr));
    const auto gateThreshold = 0.0015f + clamp01(p.gate) * clamp01(p.gate) * 0.055f;
    const auto gateAttack = 1.0f - std::exp(-1.0f / (0.002f * sr)), gateRelease = 1.0f - std::exp(-1.0f / (0.045f * sr));
    const auto jitterSmooth = 1.0f - std::exp(-1.0f / (std::max(0.003f, 1.0f / std::max(1.0f, p.jitterRate)) * sr));
    const auto mode = p.mode; const auto modeSmear = mode == ConferenceMode::skype ? 0.30f : mode == ConferenceMode::cellular ? 0.38f : mode == ConferenceMode::zoom ? 0.18f : 0.10f;
    const auto codecAmount = clamp01((16.0f - (float) bits) / 12.0f + (48000.0f - converterRate) / 60000.0f);
    const auto finalMix = clamp01(p.mix), ceiling = std::clamp(p.ceiling, 0.2f, 1.0f);
    inputPeak_.fill(0); outputPeak_.fill(0);
    suppressionActivity_ = agcActivity_ = comfortNoiseActivity_ = limiterActivity_ = 0.0f;

    for (std::size_t sample = 0; sample < sampleCount; ++sample)
    {
        if (pendingRobotTrigger_)
        {
            robotLength_ = std::clamp(requestedRobotGrain_, 8, (int) channel_[0].robotGrain.size());
            robotTotal_ = robotRemaining_ = std::max(robotLength_, requestedRobotDuration_);
            robotPosition_ = 0; robotBlend_ = requestedRobotStrength_; pendingRobotTrigger_ = false;
            for (std::size_t ch = 0; ch < count; ++ch)
                for (int i = 0; i < robotLength_; ++i)
                    channel_[ch].robotGrain[(std::size_t) i] = readHistory(channel_[ch].wetHistory, channel_[ch].writeIndex,
                        (float) (robotLength_ - i + latencySamples_));
        }
        if (frameRemaining_ <= 0 || frameLength_ != packetSamples)
        {
            frameLength_ = packetSamples; frameRemaining_ = packetSamples;
            const auto wasLost = packetLost_;
            if (badBurst_) badBurst_ = nextFloat(randomState_) < (0.12f + burst * 0.84f);
            else badBurst_ = nextFloat(randomState_) < lossChance * (1.1f - burst * 0.72f);
            packetLost_ = badBurst_;
            if (wasLost && !packetLost_) { recoveryLength_ = std::max(8, (int) std::lround(sr * 0.004f)); recoveryRemaining_ = recoveryLength_; }
            jitterTarget_ = nextSigned(randomState_) * clamp01(p.jitterMs / 8.0f) * (p.jitterMs * 0.001f * sr);
            if (nextFloat(randomState_) < clamp01(p.bufferSlip) * 0.28f)
            {
                jitterTarget_ += (nextFloat(randomState_) < 0.5f ? -1.0f : 1.0f) * (0.18f + 0.34f * clamp01(p.bufferSlip)) * packetSamples;
                slipIndicator_ = packetSamples;
            }
            if (nextFloat(randomState_) < clamp01(p.bandwidthSwitch) * 0.22f) narrowFrames_ = 2 + (int) (nextFloat(randomState_) * 5.0f);
            else if (narrowFrames_ > 0) --narrowFrames_;
            if (robotRemaining_ <= 0 && nextFloat(randomState_) < robot * robot * 0.38f)
            {
                robotLength_ = std::clamp((int) std::lround((0.008f + nextFloat(randomState_) * (0.038f + robot * 0.028f)) * sr), 8, (int) channel_[0].robotGrain.size());
                robotRemaining_ = (int) std::lround(robotLength_ * (1.3f + robot * 6.0f)); robotTotal_ = robotRemaining_; robotPosition_ = 0; robotBlend_ = 1.0f;
                for (std::size_t ch = 0; ch < count; ++ch)
                    for (int i = 0; i < robotLength_; ++i)
                        channel_[ch].robotGrain[(std::size_t) i] = readHistory(channel_[ch].wetHistory, channel_[ch].writeIndex, (float) (robotLength_ - i + latencySamples_));
            }
        }
        --frameRemaining_; if (slipIndicator_ > 0) --slipIndicator_;
        jitterCurrent_ += (jitterTarget_ - jitterCurrent_) * jitterSmooth;
        const auto robotTarget = robotRemaining_ > 0 ? robotBlend_ : 0.0f;
        robotEnvelope_ += (robotTarget - robotEnvelope_) / (robotTarget > robotEnvelope_ ? 96.0f : 192.0f);
        if (std::abs(robotEnvelope_ - robotTarget) < 1.0e-5f) robotEnvelope_ = robotTarget;
        const auto lossNow = manualLossRemaining_ > 0 || packetLost_;
        if (previousLossState_ && !lossNow)
        {
            recoveryLength_ = std::max(8, (int) std::lround(sr * 0.004f));
            recoveryRemaining_ = recoveryLength_;
        }
        previousLossState_ = lossNow;
        const auto lossDepth = manualLossRemaining_ > 0 ? manualLossDepth_ : 1.0f;

        for (std::size_t ch = 0; ch < count; ++ch)
        {
            auto& state = channel_[ch]; const auto input = channels[ch][sample]; inputPeak_[ch] = std::max(inputPeak_[ch], std::abs(input));
            state.dryHistory[state.writeIndex] = input;
            state.wetHistory[state.writeIndex] = input * std::clamp(p.inputGain, 0.0f, 4.0f);
            state.writeIndex = (state.writeIndex + 1) % state.wetHistory.size();
            const auto baseDelay = (float) latencySamples_ + 1.0f + std::max(-(float) latencySamples_ * 0.75f, jitterCurrent_);
            auto x = readHistory(state.wetHistory, state.writeIndex, std::max(1.0f, baseDelay));
            for (auto& filter : state.tone) x = filter.process(x);
            state.narrowSmear += (x - state.narrowSmear) * 0.08f;
            const auto narrowTarget = narrowFrames_ > 0 ? 1.0f : 0.0f;
            state.narrowBlend += (narrowTarget - state.narrowBlend) / 96.0f;
            if (std::abs(state.narrowBlend - narrowTarget) < 1.0e-5f) state.narrowBlend = narrowTarget;
            x += (state.narrowSmear - x) * state.narrowBlend;

            const auto magnitude = std::abs(x), envCoeff = magnitude > state.envelope ? envelopeAttack : envelopeRelease;
            state.envelope = magnitude + envCoeff * (state.envelope - magnitude);
            const auto suppression = clamp01(p.suppression), gateFloor = 1.0f - suppression * 0.94f;
            const auto gateTarget = state.envelope >= gateThreshold ? 1.0f : gateFloor;
            state.gateGain += (gateTarget - state.gateGain) * (gateTarget > state.gateGain ? gateAttack : gateRelease);
            suppressionActivity_ = std::max(suppressionActivity_, 1.0f - state.gateGain); x *= state.gateGain;
            const auto agcTarget = std::clamp(0.16f / std::max(0.02f, state.envelope), 0.55f, 3.2f);
            state.agcGain += (agcTarget - state.agcGain) * (1.0f - std::exp(-1.0f / (0.18f * sr)));
            agcActivity_ = std::max(agcActivity_, std::min(1.0f, std::abs(state.agcGain - 1.0f) / 2.2f));
            x *= 1.0f + (state.agcGain - 1.0f) * clamp01(p.agc);

            if (++state.rateCounter >= holdPeriod) { state.rateCounter = 0; state.heldSample = x; }
            const auto encoded = mode == ConferenceMode::cellular ? std::copysign(std::log1p(127.0f * std::abs(std::clamp(state.heldSample, -1.0f, 1.0f))) / std::log1p(127.0f), state.heldSample) : state.heldSample;
            auto decoded = std::round(std::clamp(encoded, -1.0f, 1.0f) * quant) / quant;
            if (mode == ConferenceMode::cellular) decoded = std::copysign(std::expm1(std::abs(decoded) * std::log1p(127.0f)) / 127.0f, decoded);
            state.smear += (decoded - state.smear) * (1.0f - modeSmear * codecAmount); x = decoded * (1.0f - modeSmear * codecAmount) + state.smear * modeSmear * codecAmount;

            if (robotLength_ > 0 && robotEnvelope_ > 1.0e-5f)
            {
                const auto repeated = state.robotGrain[(std::size_t) (robotPosition_ % robotLength_)];
                x += (repeated - x) * robotEnvelope_;
            }
            const auto comfortDepth = clamp01(p.noise) * clamp01(p.comfortNoise) * (0.0022f + codecAmount * 0.0035f);
            state.comfortState += (nextSigned(randomState_) - state.comfortState) * 0.18f;
            const auto comfort = state.comfortState * comfortDepth;
            comfortNoiseActivity_ = std::max(comfortNoiseActivity_, std::min(1.0f, std::abs(comfort) * 180.0f));
            const auto lossTarget = lossNow ? lossDepth : 0.0f;
            const auto lossRampSamples = lossNow ? std::max(96.0f, sr * 0.005f) : (float) std::max(8, recoveryLength_);
            state.lossBlend += (lossTarget - state.lossBlend) / lossRampSamples;
            if (std::abs(state.lossBlend - lossTarget) < 1.0e-5f) state.lossBlend = lossTarget;
            if (lossNow || state.lossBlend > 1.0e-5f)
            {
                const auto healthy = x;
                float concealed = state.lastGood * (0.86f + 0.12f * (float) frameRemaining_ / std::max(1, frameLength_)) + comfort;
                if (p.concealment == ConferenceConcealment::mute) concealed = comfort;
                else if (p.concealment == ConferenceConcealment::repeat) concealed = readHistory(state.wetHistory, state.writeIndex, repeatSamples + latencySamples_) * (0.94f - 0.18f * burst) + comfort;
                else if (p.concealment == ConferenceConcealment::interpolate) concealed = state.lossStart * ((float) frameRemaining_ / std::max(1, frameLength_)) + comfort;
                x = healthy + (concealed - healthy) * state.lossBlend;
            }
            if (!lossNow)
            {
                state.lastGood = x; state.lossStart = x;
            }
            x += comfort * (lossNow ? 1.0f : 0.34f);
            x = std::tanh(x * (1.0f + codecAmount * 0.42f)) * std::clamp(p.outputGain, 0.0f, 1.5f);
            limiterActivity_ = std::max(limiterActivity_, std::max(0.0f, std::abs(x) - ceiling) / std::max(0.05f, ceiling));
            x = std::clamp(x, -ceiling, ceiling);
            const auto dry = readHistory(state.dryHistory, state.writeIndex, (float) latencySamples_ + 1.0f);
            const auto output = std::clamp(dry * (1.0f - finalMix) + x * finalMix, -1.0f, 1.0f);
            channels[ch][sample] = output; outputPeak_[ch] = std::max(outputPeak_[ch], std::abs(output));
        }
        if (robotRemaining_ > 0) { --robotRemaining_; robotPosition_ = robotLength_ > 0 ? (robotPosition_ + 1) % robotLength_ : 0; }
        if (manualLossRemaining_ > 0) --manualLossRemaining_;
        if (recoveryRemaining_ > 0) --recoveryRemaining_;
    }
    lossProgress_ = manualLossRemaining_ > 0
        ? 1.0f - (float) manualLossRemaining_ / (float) std::max(1, manualLossTotal_)
        : packetLost_ ? 1.0f - (float) frameRemaining_ / (float) std::max(1, frameLength_) : 0.0f;
    robotProgress_ = robotRemaining_ > 0 ? 1.0f - (float) robotRemaining_ / (float) std::max(1, robotTotal_) : 0.0f;
    jitterActivity_ = std::min(1.0f, std::abs(jitterCurrent_) / std::max(1.0f, sr * 0.008f));
}
}
