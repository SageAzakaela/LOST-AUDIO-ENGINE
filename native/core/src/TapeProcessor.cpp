#include <lost_audio/core/TapeProcessor.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace lost_audio::core
{
namespace
{
constexpr float pi = 3.14159265358979323846f;

float clamp(float value, float minimum, float maximum) noexcept
{
    return std::clamp(value, minimum, maximum);
}

float clamp01(float value) noexcept
{
    return clamp(value, 0.0f, 1.0f);
}
}

TapeMacroTargets mapTapeMacros(float quality, float age, float wow, float glitch) noexcept
{
    const auto q = std::pow(1.0f - clamp01(quality), 1.4f);
    const auto a = std::pow(clamp01(age), 1.25f);
    const auto w = std::pow(clamp01(wow), 1.3f);
    const auto g = std::pow(clamp01(glitch), 1.35f);

    TapeMacroTargets targets;
    targets.lowPassHz = std::round(17500.0f - q * 14500.0f);
    targets.highPassHz = std::round(25.0f + q * 90.0f);
    targets.hiss = clamp01(0.03f + q * 0.22f);
    targets.hum = clamp01(0.01f + q * 0.06f);
    targets.drive = clamp01(0.08f + a * 0.85f);
    targets.compression = clamp01(0.12f + a * 0.5f);
    targets.headBumpDb = std::round((1.4f + a * 5.8f) * 20.0f) / 20.0f;
    targets.headBumpHz = std::round(70.0f + a * 45.0f);
    targets.wowDepthMs = std::round((1.2f + w * 12.5f) * 10.0f) / 10.0f;
    targets.flutterDepthMs = std::round((0.4f + w * 4.8f) * 10.0f) / 10.0f;
    targets.dropout = clamp01(g * (0.7f + 0.35f * a));
    targets.dropoutMs = std::round(18.0f + g * 140.0f);
    targets.speed = std::round((1.0f - w * 0.05f) * 1000.0f) / 1000.0f;
    targets.ceiling = 0.92f - a * 0.06f;
    targets.outputGain = std::round((0.96f + a * 0.18f) * 100.0f) / 100.0f;
    return targets;
}

void TapeProcessor::prepare(double sampleRate, std::size_t channelCount)
{
    sampleRate_ = std::max(8000.0, sampleRate);
    preparedChannels_ = std::clamp(channelCount, std::size_t { 1 }, maxChannels);
    const auto delayLength = std::max<std::size_t>(128, static_cast<std::size_t>(std::ceil(sampleRate_ * 0.06)));
    for (auto& channel : channels_)
        channel.delay.assign(delayLength, 0.0f);
    reset(seed_);
}

void TapeProcessor::reset(std::uint32_t seed) noexcept
{
    seed_ = seed == 0 ? 0x12345678u : seed;
    for (std::size_t index = 0; index < channels_.size(); ++index)
    {
        auto& channel = channels_[index];
        std::fill(channel.delay.begin(), channel.delay.end(), 0.0f);
        channel.writeIndex = 0;
        channel.randomState = index == 0 ? seed_ : seed_ ^ static_cast<std::uint32_t>(0x9e3779b9u * index);
        channel.wowPhase = nextFloat(channel.randomState);
        channel.flutterPhase = nextFloat(channel.randomState);
        channel.drift = 0.0f;
        channel.envelope = 0.0f;
        channel.limiterEnvelope = 0.0f;
        channel.humPhase = 0.0f;
        channel.hissPrevious = 0.0f;
        channel.dropoutRemaining = 0;
        channel.dropoutTotal = 0;
        channel.dropoutBlock = 0;
        channel.dropoutGain = 1.0f;
        channel.dropoutDepth = 1.0f;
        channel.dropoutInitialized = false;
    }
    pendingDropoutStrength_.store(0.0f);
    pendingDropoutDuration_.store(0.0f);
    modulationDisplacementMs_ = dropoutProgress_ = compressionReduction_ = 0.0f;
    saturationActivity_ = noiseActivity_ = limiterActivity_ = 0.0f;
    dropoutActive_ = false;
}

void TapeProcessor::triggerDropout(float strength, float durationSeconds) noexcept
{
    pendingDropoutDuration_.store(std::max(0.0f, durationSeconds), std::memory_order_relaxed);
    pendingDropoutStrength_.store(clamp01(strength), std::memory_order_release);
}

int TapeProcessor::latencySamples() const noexcept
{
    return static_cast<int>(std::llround(sampleRate_ * latencySeconds));
}

std::uint32_t TapeProcessor::nextU32(std::uint32_t& state) noexcept
{
    auto value = state;
    value ^= value << 13u;
    value ^= value >> 17u;
    value ^= value << 5u;
    state = value;
    return value;
}

float TapeProcessor::nextFloat(std::uint32_t& state) noexcept
{
    return static_cast<float>(static_cast<double>(nextU32(state)) / static_cast<double>(std::numeric_limits<std::uint32_t>::max()));
}

float TapeProcessor::nextSigned(std::uint32_t& state) noexcept
{
    return nextFloat(state) * 2.0f - 1.0f;
}

float TapeProcessor::readDelay(const ChannelState& state, float delaySamples) noexcept
{
    if (state.delay.size() < 2)
        return 0.0f;
    const auto length = static_cast<int>(state.delay.size());
    const auto read = static_cast<float>(state.writeIndex) - delaySamples;
    // Preserve the browser worklet's integer coercion for negative read
    // positions. JavaScript's `read | 0` truncates toward zero here.
    auto first = static_cast<int>(read);
    first = ((first % length) + length) % length;
    const auto second = (first + 1) % length;
    const auto fraction = read - std::floor(read);
    return state.delay[static_cast<std::size_t>(first)] * (1.0f - fraction)
         + state.delay[static_cast<std::size_t>(second)] * fraction;
}

void TapeProcessor::process(float* const* channels, std::size_t channelCount, std::size_t sampleCount, const TapeParameters& raw) noexcept
{
    const auto activeChannels = std::min({ channelCount, preparedChannels_, maxChannels });
    if (channels == nullptr || activeChannels == 0 || sampleCount == 0)
        return;

    const auto speed = clamp(raw.speed, 0.5f, 2.0f);
    const auto wowDepthMs = clamp(raw.wowDepthMs, 0.0f, 20.0f);
    const auto flutterDepthMs = clamp(raw.flutterDepthMs, 0.0f, 10.0f);
    const auto wowAmount = clamp01(raw.wowAmount);
    const auto drive = clamp01(raw.drive);
    const auto compression = clamp01(raw.compression);
    const auto hiss = clamp01(raw.hiss);
    const auto hum = clamp01(raw.hum);
    const auto dropout = clamp01(raw.dropout);
    const auto dropoutMs = clamp(raw.dropoutMs, 1.0f, 400.0f);
    const auto ceiling = clamp(raw.ceiling, 0.2f, 1.0f);
    const auto outputGain = clamp(raw.outputGain, 0.0f, 1.5f);
    const auto sampleRate = static_cast<float>(sampleRate_);

    const auto wowHz = raw.wowRateHz > 0.0f ? clamp(raw.wowRateHz, 0.01f, 40.0f) : 0.22f + wowAmount * 0.55f;
    const auto flutterHz = raw.flutterRateHz > 0.0f ? clamp(raw.flutterRateHz, 0.01f, 80.0f) : 4.8f + wowAmount * 6.5f;
    const auto wowDepthSeconds = (wowDepthMs / 1000.0f) * (0.25f + 0.75f * wowAmount);
    const auto flutterDepthSeconds = (flutterDepthMs / 1000.0f) * (0.25f + 0.75f * wowAmount);
    const auto totalDepthSeconds = clamp(wowDepthSeconds + flutterDepthSeconds, 0.0f, 0.03f);

    const auto envelopeAttack = std::exp(-1.0f / (0.006f * sampleRate));
    const auto envelopeRelease = std::exp(-1.0f / (0.12f * sampleRate));
    const auto compressionPower = compression * 0.72f;
    const auto dropoutBlockSamples = std::max(8, static_cast<int>(std::round((dropoutMs / 1000.0f) * sampleRate)));
    const auto humDepth = hum * hum * 0.02f;
    const auto hissDepth = hiss * hiss * 0.03f;
    // Keep useful oxide colour in the lower half of the control. The transfer
    // remains gain-normalised, so more harmonics do not masquerade as loudness.
    const auto saturationAmount = 1.0f + drive * 8.5f;
    const auto asymmetry = drive * 0.065f;
    const auto saturationZero = std::tanh(asymmetry * saturationAmount);
    const auto saturationSlope = std::max(0.2f, saturationAmount * (1.0f - saturationZero * saturationZero));
    const auto limiterAttack = std::exp(-1.0f / (0.002f * sampleRate));
    const auto limiterRelease = std::exp(-1.0f / (0.06f * sampleRate));
    const auto manualStrength = pendingDropoutStrength_.exchange(0.0f, std::memory_order_acq_rel);
    if (manualStrength > 0.0f)
    {
        const auto seconds = pendingDropoutDuration_.exchange(0.0f, std::memory_order_relaxed);
        const auto length = std::max(1, static_cast<int>(std::round(
            (seconds > 0.0f ? seconds : dropoutMs * 0.001f) * sampleRate)));
        for (std::size_t index = 0; index < activeChannels; ++index)
        {
            auto& state = channels_[index];
            state.dropoutRemaining = state.dropoutTotal = length;
            state.dropoutBlock = std::max(state.dropoutBlock, length);
            state.dropoutDepth = manualStrength;
        }
    }

    for (std::size_t channelIndex = 0; channelIndex < activeChannels; ++channelIndex)
    {
        auto* samples = channels[channelIndex];
        if (samples == nullptr)
            continue;
        auto& state = channels_[channelIndex];
        // Prime the first window once. Re-priming whenever a host block begins
        // at zero would skip a stochastic decision and make damage depend on
        // the DAW buffer size.
        if (!state.dropoutInitialized)
        {
            state.dropoutBlock = dropoutBlockSamples;
            state.dropoutInitialized = true;
        }

        for (std::size_t sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
        {
            state.delay[state.writeIndex] = samples[sampleIndex];
            const auto driftStep = wowAmount * wowAmount * 2.2e-6f;
            if (driftStep > 0.0f)
                state.drift = clamp(state.drift + nextSigned(state.randomState) * driftStep, -0.0018f, 0.0018f);

            const auto wowSignal = std::sin(state.wowPhase * 2.0f * pi);
            const auto flutterSignal = std::sin(state.flutterPhase * 2.0f * pi);
            const auto modulation = (wowSignal * wowDepthSeconds + flutterSignal * flutterDepthSeconds + state.drift) * (0.6f + 0.4f * wowAmount);
            modulationDisplacementMs_=modulation*1000.0f;
            const auto delaySeconds = clamp(static_cast<float>(latencySeconds) + modulation, 0.001f,
                                            static_cast<float>(latencySeconds) + totalDepthSeconds + 0.01f);
            auto value = readDelay(state, delaySeconds * sampleRate);

            state.wowPhase += (wowHz * speed) / sampleRate;
            state.flutterPhase += (flutterHz * speed) / sampleRate;
            if (state.wowPhase >= 1.0f) state.wowPhase -= 1.0f;
            if (state.flutterPhase >= 1.0f) state.flutterPhase -= 1.0f;

            const auto amplitude = std::abs(value);
            const auto envelopeCoefficient = amplitude > state.envelope ? envelopeAttack : envelopeRelease;
            state.envelope = amplitude + envelopeCoefficient * (state.envelope - amplitude);
            const auto over = std::max(1.0f, (state.envelope + 1.0e-6f) / 0.2f);
            const auto compressionGain = compression > 0.0001f ? std::pow(over, -compressionPower) : 1.0f;
            compressionReduction_ = std::max(1.0f - compressionGain, compressionReduction_ * 0.999f);
            value *= std::max(0.45f, compressionGain) * (1.0f + compression * 0.12f);

            const auto saturated = (std::tanh((value + asymmetry) * saturationAmount) - saturationZero) / saturationSlope;
            saturationActivity_ = std::max(std::abs(saturated - value) * drive, saturationActivity_ * 0.999f);
            const auto saturationBlend = drive <= 0.0f ? 0.0f : std::min(1.0f, 0.18f + drive * 0.92f);
            value = value * (1.0f - saturationBlend) + saturated * saturationBlend;

            if (state.dropoutBlock <= 0)
            {
                state.dropoutBlock = dropoutBlockSamples;
                state.dropoutRemaining = nextFloat(state.randomState) < dropout * dropout ? dropoutBlockSamples : 0;
                state.dropoutTotal = state.dropoutRemaining;
                state.dropoutDepth = 1.0f;
            }
            --state.dropoutBlock;
            const auto dropoutTarget = state.dropoutRemaining > 0 ? 1.0f-state.dropoutDepth : 1.0f;
            state.dropoutGain = clamp(state.dropoutGain + (dropoutTarget - state.dropoutGain) / 48.0f, 0.0f, 1.0f);
            if (state.dropoutRemaining > 0)
                --state.dropoutRemaining;
            dropoutActive_ = dropoutActive_ || state.dropoutRemaining > 0;
            if (state.dropoutRemaining > 0 && state.dropoutTotal > 0)
                dropoutProgress_ = std::max(dropoutProgress_, 1.0f - static_cast<float>(state.dropoutRemaining) / static_cast<float>(state.dropoutTotal));
            value *= state.dropoutGain;

            state.humPhase += (2.0f * pi * 60.0f) / sampleRate;
            if (state.humPhase > 2.0f * pi)
                state.humPhase -= 2.0f * pi;
            const auto white = nextSigned(state.randomState);
            const auto highPassedNoise = white - state.hissPrevious;
            state.hissPrevious = white;
            const auto noise = std::sin(state.humPhase) * humDepth + highPassedNoise * hissDepth;
            noiseActivity_ = std::max(std::abs(noise), noiseActivity_ * 0.999f);
            value += noise;

            auto output = value * outputGain;
            const auto outputAmplitude = std::abs(output);
            const auto limiterCoefficient = outputAmplitude > state.limiterEnvelope ? limiterAttack : limiterRelease;
            state.limiterEnvelope = outputAmplitude + limiterCoefficient * (state.limiterEnvelope - outputAmplitude);
            limiterActivity_ *= 0.999f;
            if (state.limiterEnvelope > ceiling)
            {
                limiterActivity_ = std::max(1.0f - ceiling / (state.limiterEnvelope + 1.0e-6f), limiterActivity_);
                output *= ceiling / (state.limiterEnvelope + 1.0e-6f);
            }
            samples[sampleIndex] = clamp(output, -ceiling, ceiling);

            state.writeIndex = (state.writeIndex + 1) % state.delay.size();
        }
    }
    dropoutActive_ = false;
    dropoutProgress_ = 0.0f;
    for (std::size_t index = 0; index < activeChannels; ++index)
    {
        const auto& state = channels_[index];
        dropoutActive_ = dropoutActive_ || state.dropoutRemaining > 0;
        if (state.dropoutRemaining > 0 && state.dropoutTotal > 0)
            dropoutProgress_ = std::max(dropoutProgress_, 1.0f - static_cast<float>(state.dropoutRemaining) / static_cast<float>(state.dropoutTotal));
    }
}
}
