#include <lost_audio/core/TransmissionProcessor.h>

#include <algorithm>
#include <cmath>

namespace lost_audio::core
{
namespace
{
constexpr float pi = 3.14159265358979323846f;

float clamp01(float value) noexcept { return std::clamp(value, 0.0f, 1.0f); }
float dbToGain(float db) noexcept { return std::pow(10.0f, db / 20.0f); }
float softClip(float value) noexcept { return std::tanh(value); }
}

TransmissionMacroTargets mapTransmissionMacros(
    float bandwidth, float drive, float badConnection, float noiseProfile) noexcept
{
    const auto bw = clamp01(bandwidth);
    const auto driven = clamp01(drive);
    const auto bad = clamp01(badConnection);
    const auto noise = clamp01(noiseProfile);
    TransmissionMacroTargets targets;
    targets.highPassHz = 650.0f - std::pow(bw, 0.8f) * 560.0f;
    targets.lowPassHz = 2000.0f + std::pow(bw, 1.35f) * 12500.0f;
    targets.midGainDb = std::pow(1.0f - bw, 1.15f) * 6.2f;
    targets.midQ = 0.85f + (1.0f - bw) * 1.55f;
    targets.midFrequencyHz = 1450.0f + bw * 350.0f;
    targets.boxDipDb = (1.0f - bw) * 2.2f;
    targets.asymmetry = driven * 0.6f;
    targets.compression = 0.18f + driven * 0.65f;
    targets.wowDepth = bad;
    targets.dropoutRate = bad;
    targets.dropoutDepth = bad;
    targets.crackle = bad;
    targets.lfoRateHz = 0.45f + bad * 1.6f;
    targets.hiss = noise * 0.95f;
    targets.noiseColor = std::max(0.0f, (noise - 0.55f) * 2.0f);
    return targets;
}

float TransmissionProcessor::Biquad::process(float input) noexcept
{
    const auto output = b0 * input + z1;
    z1 = b1 * input - a1 * output + z2;
    z2 = b2 * input - a2 * output;
    return output;
}

void TransmissionProcessor::Biquad::setHighPass(double sr, float frequency, float q) noexcept
{
    const auto f = std::clamp(frequency, 20.0f, (float) sr * 0.45f);
    const auto omega = 2.0f * pi * f / (float) sr;
    const auto cosine = std::cos(omega);
    const auto alpha = std::sin(omega) / (2.0f * std::max(0.08f, q));
    const auto a0 = 1.0f + alpha;
    b0 = ((1.0f + cosine) * 0.5f) / a0;
    b1 = -(1.0f + cosine) / a0;
    b2 = b0;
    a1 = (-2.0f * cosine) / a0;
    a2 = (1.0f - alpha) / a0;
}

void TransmissionProcessor::Biquad::setLowPass(double sr, float frequency, float q) noexcept
{
    const auto f = std::clamp(frequency, 20.0f, (float) sr * 0.45f);
    const auto omega = 2.0f * pi * f / (float) sr;
    const auto cosine = std::cos(omega);
    const auto alpha = std::sin(omega) / (2.0f * std::max(0.08f, q));
    const auto a0 = 1.0f + alpha;
    b0 = ((1.0f - cosine) * 0.5f) / a0;
    b1 = (1.0f - cosine) / a0;
    b2 = b0;
    a1 = (-2.0f * cosine) / a0;
    a2 = (1.0f - alpha) / a0;
}

void TransmissionProcessor::Biquad::setPeak(double sr, float frequency, float q, float gainDb) noexcept
{
    const auto f = std::clamp(frequency, 20.0f, (float) sr * 0.45f);
    const auto omega = 2.0f * pi * f / (float) sr;
    const auto cosine = std::cos(omega);
    const auto alpha = std::sin(omega) / (2.0f * std::max(0.08f, q));
    const auto a = std::pow(10.0f, gainDb / 40.0f);
    const auto a0 = 1.0f + alpha / a;
    b0 = (1.0f + alpha * a) / a0;
    b1 = (-2.0f * cosine) / a0;
    b2 = (1.0f - alpha * a) / a0;
    a1 = b1;
    a2 = (1.0f - alpha / a) / a0;
}

void TransmissionProcessor::prepare(double sampleRate, std::size_t channelCount)
{
    sampleRate_ = std::clamp(sampleRate, 8000.0, 384000.0);
    preparedChannels_ = std::clamp(channelCount, std::size_t { 1 }, maxChannels);
    stageDelaySamples_ = std::max(1, (int) std::llround(sampleRate_ * stageLatencySeconds));
    const auto stageBufferSize = (std::size_t) std::max(stageDelaySamples_ + 8,
        (int) std::ceil(sampleRate_ * 0.012) + 4);
    for (auto& channel : stages_)
        for (auto& stage : channel)
            stage.delay.assign(stageBufferSize, 0.0f);
    for (auto& delay : dryDelay_)
        delay.samples.assign((std::size_t) latencySamples() + 1, 0.0f);
    walkie_.rmsRing.assign((std::size_t) std::max(1, (int) std::floor(sampleRate_ * 0.01)), 0.0f);
    reset(seed_);
}

void TransmissionProcessor::reset(std::uint32_t seed) noexcept
{
    seed_ = seed == 0 ? 0x7472616eu : seed;
    for (std::size_t channel = 0; channel < maxChannels; ++channel)
    {
        for (std::size_t pass = 0; pass < maxPasses; ++pass)
        {
            auto& stage = stages_[channel][pass];
            std::fill(stage.delay.begin(), stage.delay.end(), 0.0f);
            stage.delayIndex = 0;
            stage.envelope = 0.0f;
            stage.crushHold = 0.0f;
            stage.crushPhase = 0;
            stage.lfoPhase = 0.0f;
            stage.driftNoise = 0.0f;
            stage.pink = {};
            stage.previousNoise = 0.0f;
            stage.randomState = mixSeed(seed_, (std::uint32_t) (0x70005700u ^ (pass * 0x9e3779b9u) ^ (channel * 0x85ebca6bu)));
            stage.dropoutRemaining = stage.dropoutTotal = 0;
            stage.dropoutDepth = 1.0f;
            stage.crackleRemaining = stage.crackleTotal = 0;
            stage.crackleState = 0.0f;
            stage.hp1.reset(); stage.hp2.reset(); stage.lp1.reset(); stage.lp2.reset();
            stage.dip.reset(); stage.mid.reset();
        }
        auto& dry = dryDelay_[channel];
        std::fill(dry.samples.begin(), dry.samples.end(), 0.0f);
        dry.index = 0;
    }
    std::fill(walkie_.rmsRing.begin(), walkie_.rmsRing.end(), 0.0f);
    walkie_.ringIndex = 0;
    walkie_.sumSquares = 0.0f;
    walkie_.belowCount = 0;
    walkie_.inSilence = false;
    walkie_.clickRemaining = walkie_.clickTotal = 0;
    walkie_.clickAmplitude = 0.0f;
    walkie_.clickFrequency = 1800.0f;
    walkie_.clickPhase = 0.0f;
    walkie_.noiseHpState = 0.0f;
    walkie_.randomState = mixSeed(seed_, 0xa11ce001u);
    pendingDropoutStrength_.store(0.0f);
    pendingDropoutDuration_.store(0.0f);
    carrierDisplacementMs_ = dropoutProgress_ = compressionReduction_ = 0.0f;
    noiseActivity_ = crackleActivity_ = limiterActivity_ = 0.0f;
    dropoutActive_ = false;
}

void TransmissionProcessor::triggerDropout(float strength, float durationSeconds) noexcept
{
    pendingDropoutDuration_.store(std::max(0.001f, durationSeconds), std::memory_order_relaxed);
    pendingDropoutStrength_.store(clamp01(strength), std::memory_order_release);
}

int TransmissionProcessor::latencySamples() const noexcept
{
    return stageDelaySamples_ * (int) maxPasses;
}

std::uint32_t TransmissionProcessor::mixSeed(std::uint32_t base, std::uint32_t salt) noexcept
{
    auto x = base ^ salt;
    x = (x ^ (x >> 16u)) * 0x7feb352du;
    x = (x ^ (x >> 15u)) * 0x846ca68bu;
    return (x ^ (x >> 16u)) | 1u;
}

std::uint32_t TransmissionProcessor::nextU32(std::uint32_t& state) noexcept
{
    auto x = state == 0 ? 0x12345678u : state;
    x ^= x << 13u;
    x ^= x >> 17u;
    x ^= x << 5u;
    state = x;
    return x;
}

float TransmissionProcessor::nextFloat(std::uint32_t& state) noexcept
{
    return (float) ((double) nextU32(state) / 4294967295.0);
}

float TransmissionProcessor::nextSigned(std::uint32_t& state) noexcept
{
    return nextFloat(state) * 2.0f - 1.0f;
}

float TransmissionProcessor::pinkFromWhite(PinkNoiseState& s, float white) noexcept
{
    s.p0 = 0.99886f * s.p0 + white * 0.0555179f;
    s.p1 = 0.99332f * s.p1 + white * 0.0750759f;
    s.p2 = 0.96900f * s.p2 + white * 0.1538520f;
    s.p3 = 0.86650f * s.p3 + white * 0.3104856f;
    s.p4 = 0.55000f * s.p4 + white * 0.5329522f;
    s.p5 = -0.7616f * s.p5 - white * 0.0168980f;
    const auto pink = s.p0 + s.p1 + s.p2 + s.p3 + s.p4 + s.p5 + s.p6 + white * 0.5362f;
    s.p6 = white * 0.115926f;
    return pink * 0.11f;
}

void TransmissionProcessor::updateFilters(const TransmissionParameters& p) noexcept
{
    const auto q = 0.9f + (1.0f - std::clamp((p.lowPassHz - 2000.0f) / 12500.0f, 0.0f, 1.0f)) * 0.35f;
    for (auto& channel : stages_)
    {
        for (auto& stage : channel)
        {
            stage.hp1.setHighPass(sampleRate_, p.highPassHz, q);
            stage.hp2.setHighPass(sampleRate_, p.highPassHz, q);
            stage.lp1.setLowPass(sampleRate_, p.lowPassHz, q);
            stage.lp2.setLowPass(sampleRate_, p.lowPassHz, q);
            stage.dip.setPeak(sampleRate_, 680.0f, 0.8f, -std::abs(p.boxDipDb));
            stage.mid.setPeak(sampleRate_, p.midFrequencyHz, p.midQ, p.midGainDb);
        }
    }
}

float TransmissionProcessor::readDelay(const StageState& state, float delaySamples) const noexcept
{
    if (state.delay.empty()) return 0.0f;
    auto read = (float) state.delayIndex - delaySamples;
    while (read < 0.0f) read += (float) state.delay.size();
    const auto floorRead = std::floor(read);
    const auto i0 = (std::size_t) floorRead % state.delay.size();
    const auto i1 = (i0 + 1) % state.delay.size();
    const auto fraction = read - floorRead;
    return state.delay[i0] + (state.delay[i1] - state.delay[i0]) * fraction;
}

float TransmissionProcessor::processStage(StageState& s, float input, bool active,
                                          int activePasses, const TransmissionParameters& p) noexcept
{
    if (!active)
    {
        s.delay[s.delayIndex] = input;
        const auto delayed = readDelay(s, (float) stageDelaySamples_);
        s.delayIndex = (s.delayIndex + 1) % s.delay.size();
        return delayed;
    }

    const auto passCount = (float) std::max(1, activePasses);
    const auto mapPerStage = [passCount](float amount)
    {
        return 1.0f - std::pow(1.0f - clamp01(amount), 1.0f / passCount);
    };
    const auto preAmount = mapPerStage(std::pow(clamp01(p.drive), 0.85f) * 0.75f);
    const auto postAmount = mapPerStage(p.drive);
    const auto pre = 1.0f + preAmount * 10.5f;
    const auto preBias = clamp01(p.asymmetry) * 0.18f;
    const auto preCenter = softClip(preBias * pre);
    const auto preMatch = 0.18f / std::max(1.0e-6f, softClip(0.18f * pre));
    auto value = (softClip((input + preBias) * pre) - preCenter) * preMatch;
    value = s.hp1.process(value);
    value = s.hp2.process(value);
    value = s.lp1.process(value);
    value = s.lp2.process(value);
    value = s.dip.process(value);
    value = s.mid.process(value);

    const auto post = 1.0f + postAmount * 12.0f;
    const auto postBias = clamp01(p.asymmetry) * 0.22f;
    const auto compAmount = clamp01(p.compression) * 0.75f;
    const auto threshold = 0.18f + (1.0f - postAmount) * 0.16f;
    const auto attack = std::exp(-1.0f / ((float) sampleRate_ * (0.003f + 0.01f * (1.0f - postAmount))));
    const auto release = std::exp(-1.0f / ((float) sampleRate_ * (0.06f + 0.16f * (1.0f - postAmount))));
    auto driven = (value + postBias) * post;
    const auto magnitude = std::abs(driven);
    const auto envelopeCoeff = magnitude > s.envelope ? attack : release;
    s.envelope = magnitude + envelopeCoeff * (s.envelope - magnitude);
    auto gain = 1.0f;
    if (s.envelope > threshold)
        gain = 1.0f / (1.0f + compAmount * (s.envelope - threshold) * 4.2f);
    compressionReduction_ = std::max(compressionReduction_ * 0.999f, 1.0f - gain);
    const auto postCenter = softClip(postBias * post * gain);
    const auto postMatch = 0.2f / std::max(1.0e-6f, softClip(0.2f * post));
    value = (softClip(driven * gain) - postCenter) * postMatch;

    const auto crush = clamp01(p.crush);
    if (crush > 0.0001f)
    {
        const auto bits = (int) std::round(16.0f - crush * 12.0f);
        const auto quant = std::pow(2.0f, (float) std::max(1, bits - 1));
        const auto downsample = std::max(1, (int) std::round(1.0f + crush * 15.0f));
        if (s.crushPhase == 0) s.crushHold = std::clamp(std::round(value * quant) / quant, -1.0f, 1.0f);
        s.crushPhase = (s.crushPhase + 1) % downsample;
        value = s.crushHold;
    }
    else
    {
        s.crushPhase = 0;
        s.crushHold = value;
    }

    const auto dropRate = clamp01(p.dropoutRate);
    if (dropRate > 0.0f && s.dropoutRemaining <= 0
        && nextFloat(s.randomState) < (0.15f + 1.9f * dropRate * dropRate) / (float) sampleRate_)
    {
        s.dropoutTotal = std::max(1, (int) (((18.0f + 140.0f * dropRate) / 1000.0f) * (float) sampleRate_));
        s.dropoutRemaining = s.dropoutTotal;
        const auto minGain = 1.0f - clamp01(p.dropoutDepth) * 0.95f;
        s.dropoutDepth = std::clamp(minGain * (0.78f + 0.22f * nextFloat(s.randomState)), 0.02f, 1.0f);
    }
    const auto crackleAmount = clamp01(p.crackle);
    if (crackleAmount > 0.0f && s.crackleRemaining <= 0
        && nextFloat(s.randomState) < (0.35f + 7.5f * crackleAmount * crackleAmount) / (float) sampleRate_)
    {
        s.crackleTotal = std::max(1, (int) (((2.0f + 10.0f * crackleAmount) / 1000.0f) * (float) sampleRate_));
        s.crackleRemaining = s.crackleTotal;
    }

    s.lfoPhase += 2.0f * pi * std::clamp(p.lfoRateHz, 0.1f, 6.0f) / (float) sampleRate_;
    if (s.lfoPhase > 2.0f * pi) s.lfoPhase -= 2.0f * pi;
    s.driftNoise = s.driftNoise * 0.9992f + nextSigned(s.randomState) * 0.0008f;
    s.delay[s.delayIndex] = value;
    const auto wow = clamp01(p.wowDepth);
    const auto driftSeconds = wow * (std::sin(s.lfoPhase) * 0.0018f + s.driftNoise * 0.0012f);
    carrierDisplacementMs_ = std::max(carrierDisplacementMs_ * 0.999f, std::abs(driftSeconds) * 1000.0f);
    const auto delay = std::clamp(((float) stageLatencySeconds + driftSeconds) * (float) sampleRate_,
                                  1.0f, (float) s.delay.size() - 2.0f);
    value = readDelay(s, delay);
    s.delayIndex = (s.delayIndex + 1) % s.delay.size();
    const auto carrierFade = 1.0f - wow * 0.08f * (0.5f + 0.5f * std::sin(s.lfoPhase));

    auto dropoutGain = 1.0f;
    if (s.dropoutRemaining > 0)
    {
        const auto t = 1.0f - (float) s.dropoutRemaining / (float) s.dropoutTotal;
        const auto fade = t < 0.2f ? t / 0.2f : (t > 0.85f ? (1.0f - t) / 0.15f : 1.0f);
        dropoutGain = 1.0f - (1.0f - s.dropoutDepth) * fade;
        dropoutActive_ = true;
        dropoutProgress_ = std::max(dropoutProgress_, t);
        --s.dropoutRemaining;
    }
    auto crackle = 0.0f;
    if (s.crackleRemaining > 0)
    {
        const auto t = 1.0f - (float) s.crackleRemaining / (float) s.crackleTotal;
        const auto envelope = (t < 0.15f ? t / 0.15f : 1.0f) * (t > 0.7f ? (1.0f - t) / 0.3f : 1.0f);
        const auto target = nextSigned(s.randomState);
        const auto smoothing = 0.10f + crackleAmount * 0.12f;
        s.crackleState += (target - s.crackleState) * smoothing;
        crackle = s.crackleState * (0.06f + 0.24f * crackleAmount) * envelope;
        crackleActivity_ = std::max(crackleActivity_ * 0.999f, std::abs(crackle));
        --s.crackleRemaining;
    }
    else
    {
        s.crackleState *= 0.92f;
    }
    const auto white = nextSigned(s.randomState);
    const auto pink = pinkFromWhite(s.pink, white);
    const auto colored = (1.0f - clamp01(p.noiseColor)) * white + clamp01(p.noiseColor) * pink;
    const auto hissHp = colored - s.previousNoise;
    s.previousNoise = colored;
    const auto stageNoiseScale = std::pow(passCount, -0.4f);
    const auto noiseLevel = std::pow(clamp01(p.noise) * stageNoiseScale, 1.2f) * 0.055f;
    const auto noiseOutput = colored * noiseLevel + hissHp * (noiseLevel * clamp01(p.hiss) * 0.45f);
    noiseActivity_ = std::max(noiseActivity_ * 0.999f, std::abs(noiseOutput));
    const auto passLoss = 1.0f / std::sqrt(1.0f + 0.55f * (passCount - 1.0f));
    const auto stageGain = std::pow(passLoss, 1.0f / passCount);
    return std::clamp((value * carrierFade * dropoutGain + crackle + noiseOutput) * stageGain, -1.0f, 1.0f);
}

float TransmissionProcessor::processWalkie(float monoInput, const TransmissionParameters& p) noexcept
{
    auto& w = walkie_;
    if (w.rmsRing.empty()) return 0.0f;
    const auto old = w.rmsRing[w.ringIndex];
    const auto squared = monoInput * monoInput;
    w.rmsRing[w.ringIndex] = squared;
    w.ringIndex = (w.ringIndex + 1) % w.rmsRing.size();
    w.sumSquares += squared - old;
    const auto rms = std::sqrt(std::max(0.0f, w.sumSquares / (float) w.rmsRing.size()));
    const auto threshold = dbToGain(std::clamp(p.walkieThresholdDb, -80.0f, -20.0f));
    const auto silenceSamples = std::max(1, (int) ((std::clamp(p.walkieMinSilenceMs, 80.0f, 600.0f) / 1000.0f) * (float) sampleRate_));
    const auto trigger = [&]
    {
        w.clickTotal = std::max(1, (int) ((std::clamp(p.walkieClickMs, 5.0f, 200.0f) / 1000.0f) * (float) sampleRate_));
        w.clickRemaining = w.clickTotal;
        w.clickAmplitude = clamp01(p.walkieClickLevel) * (0.55f + 0.55f * nextFloat(w.randomState));
        w.clickFrequency = 1300.0f + nextFloat(w.randomState) * 1600.0f;
        w.clickPhase = nextFloat(w.randomState) * 2.0f * pi;
    };
    if (p.walkieEnabled)
    {
        if (rms < threshold) ++w.belowCount; else w.belowCount = 0;
        const auto nowSilent = w.belowCount >= silenceSamples;
        if (!w.inSilence && nowSilent) { w.inSilence = true; trigger(); }
        else if (w.inSilence && rms >= threshold * 1.15f) { w.inSilence = false; w.belowCount = 0; trigger(); }
    }
    else
    {
        w.inSilence = false;
        w.belowCount = 0;
    }
    if (w.clickRemaining <= 0) return 0.0f;
    const auto t = 1.0f - (float) w.clickRemaining / (float) w.clickTotal;
    const auto noise = nextSigned(w.randomState) * 0.10f;
    const auto hp = (noise - w.noiseHpState) + 0.995f * w.noiseHpState;
    w.noiseHpState = noise;
    float result = 0.0f;
    if (p.walkieDispatchMode)
    {
        const auto hz = t < 0.52f ? 1150.0f : 820.0f;
        const auto envelope = std::sin(std::min(1.0f, t * 12.0f) * pi * 0.5f)
                            * std::sin(std::min(1.0f, (1.0f - t) * 10.0f) * pi * 0.5f);
        w.clickPhase += 2.0f * pi * hz / (float) sampleRate_;
        result = softClip((std::sin(w.clickPhase) * 0.95f + hp * 0.35f) * (w.clickAmplitude * 2.4f)) * envelope;
    }
    else
    {
        const auto envelope = std::exp(-t * 14.0f);
        w.clickPhase += 2.0f * pi * w.clickFrequency / (float) sampleRate_;
        result = softClip((std::sin(w.clickPhase) * 0.85f + hp) * (w.clickAmplitude * 3.2f)) * envelope;
    }
    --w.clickRemaining;
    return result;
}

void TransmissionProcessor::process(float* const* channels, std::size_t channelCount,
                                    std::size_t sampleCount, const TransmissionParameters& raw) noexcept
{
    if (channels == nullptr || preparedChannels_ == 0 || sampleCount == 0) return;
    const auto count = std::min({ channelCount, preparedChannels_, maxChannels });
    if (count == 0) return;
    auto p = raw;
    p.passes = std::clamp(p.passes, 1, (int) maxPasses);
    p.inputGain = std::clamp(p.inputGain, 0.0f, 4.0f);
    p.outputGain = std::clamp(p.outputGain, 0.0f, 1.5f);
    p.mix = clamp01(p.mix);
    p.ceiling = std::clamp(p.ceiling, 0.2f, 1.0f);
    updateFilters(p);

    carrierDisplacementMs_ *= 0.92f;
    dropoutProgress_ = 0.0f;
    compressionReduction_ *= 0.92f;
    noiseActivity_ *= 0.92f;
    crackleActivity_ *= 0.92f;
    limiterActivity_ *= 0.92f;
    dropoutActive_ = false;
    const auto manualStrength = pendingDropoutStrength_.exchange(0.0f, std::memory_order_acq_rel);
    if (manualStrength > 0.0f)
    {
        const auto duration = pendingDropoutDuration_.exchange(0.0f, std::memory_order_relaxed);
        const auto total = std::max(1, (int) std::lround(duration * (float) sampleRate_));
        for (std::size_t channel = 0; channel < count; ++channel)
            for (int pass = 0; pass < p.passes; ++pass)
            {
                auto& stage = stages_[channel][(std::size_t) pass];
                stage.dropoutRemaining = stage.dropoutTotal = total;
                stage.dropoutDepth = std::clamp(1.0f - manualStrength * 0.95f, 0.02f, 1.0f);
            }
    }

    for (std::size_t sample = 0; sample < sampleCount; ++sample)
    {
        float mono = 0.0f;
        for (std::size_t channel = 0; channel < count; ++channel)
            mono += channels[channel][sample] * p.inputGain;
        mono /= (float) count;
        const auto walkie = processWalkie(mono, p);
        for (std::size_t channel = 0; channel < count; ++channel)
        {
            const auto input = channels[channel][sample] * p.inputGain;
            auto& dry = dryDelay_[channel];
            dry.samples[dry.index] = input;
            const auto drySample = dry.samples[(dry.index + 1) % dry.samples.size()];
            dry.index = (dry.index + 1) % dry.samples.size();
            auto value = std::clamp(input + walkie, -1.0f, 1.0f);
            for (std::size_t pass = 0; pass < maxPasses; ++pass)
                value = processStage(stages_[channel][pass], value, (int) pass < p.passes, p.passes, p);
            auto output = (drySample * (1.0f - p.mix) + value * p.mix) * p.outputGain;
            if (std::abs(output) > p.ceiling)
                limiterActivity_ = std::max(limiterActivity_, 1.0f - p.ceiling / (std::abs(output) + 1.0e-6f));
            channels[channel][sample] = std::clamp(output, -p.ceiling, p.ceiling);
        }
    }
    dropoutActive_ = false;
    dropoutProgress_ = 0.0f;
    for (std::size_t channel = 0; channel < count; ++channel)
        for (int pass = 0; pass < p.passes; ++pass)
        {
            const auto& stage = stages_[channel][(std::size_t) pass];
            if (stage.dropoutRemaining > 0 && stage.dropoutTotal > 0)
            {
                dropoutActive_ = true;
                dropoutProgress_ = std::max(dropoutProgress_,
                    1.0f - (float) stage.dropoutRemaining / (float) stage.dropoutTotal);
            }
        }
}
}
