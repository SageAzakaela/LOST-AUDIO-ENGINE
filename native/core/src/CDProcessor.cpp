#include <lost_audio/core/CDProcessor.h>

#include <algorithm>
#include <cmath>

namespace lost_audio::core
{
namespace
{
constexpr float pi = 3.14159265358979323846f;
float clamp01(float value) noexcept { return std::clamp(value, 0.0f, 1.0f); }
}

CDMacroTargets mapCDMacros(float clarity, float damage, float tracking, float jitter) noexcept
{
    const auto c = std::pow(1.0f - clamp01(clarity), 1.35f);
    const auto d = std::pow(clamp01(damage), 1.25f);
    const auto t = std::pow(clamp01(tracking), 1.35f);
    const auto j = std::pow(clamp01(jitter), 1.25f);
    CDMacroTargets result;
    result.errorRate = clamp01(0.006f + c * 0.18f + d * 0.62f);
    result.burstMs = std::round(4.0f + c * 38.0f + d * 112.0f + t * 68.0f);
    result.repeatMs = std::round(18.0f + t * 92.0f + d * 28.0f);
    result.scratchRate = clamp01(0.008f + d * 0.96f);
    result.scratchAmount = clamp01(0.04f + d * 0.9f);
    result.correction = clamp01(0.995f - c * 0.42f - d * 0.66f - t * 0.16f);
    result.interpolationMs = std::round((3.0f + c * 9.0f) * 10.0f) / 10.0f;
    result.rotationHz = std::round((5.2f + j * 0.8f) * 10.0f) / 10.0f;
    result.trackingRate = clamp01(0.004f + t * 0.92f);
    result.trackingMs = std::round(45.0f + t * 820.0f);
    result.servoHunt = clamp01(0.03f + t * 0.78f + d * 0.24f);
    result.jitterMs = std::round((0.001f + j * 0.32f) * 1000.0f) / 1000.0f;
    result.jitterRateHz = std::round(24.0f + j * 90.0f);
    result.highFrequencyLoss = clamp01(0.002f + c * 0.045f + d * 0.12f);
    result.servoNoise = clamp01(0.015f + t * 0.18f + d * 0.12f);
    result.ceiling = std::round((0.97f - d * 0.11f) * 1000.0f) / 1000.0f;
    result.outputGain = std::round((0.98f - d * 0.06f) * 100.0f) / 100.0f;
    return result;
}

void CDProcessor::prepare(double sampleRate, std::size_t channelCount)
{
    sampleRate_ = std::clamp(sampleRate, 8000.0, 384000.0);
    preparedChannels_ = std::clamp(channelCount, std::size_t { 1 }, maxChannels);
    latencySamples_ = std::max(1, (int) std::llround(sampleRate_ * fixedLatencySeconds));
    const auto delaySize = (std::size_t) std::ceil(sampleRate_ * 0.006) + (std::size_t) latencySamples_ + 8;
    const auto historySize = (std::size_t) std::ceil(sampleRate_ * 2.1);
    for (auto& state : channel_)
    {
        state.delay.assign(delaySize, 0.0f);
        state.dryDelay.assign((std::size_t) latencySamples_ + 2, 0.0f);
        state.history.assign(historySize, 0.0f);
    }
    reset(seed_);
}

void CDProcessor::reset(std::uint32_t seed) noexcept
{
    seed_ = seed == 0 ? 0x4344454eu : seed;
    randomState_ = seed_;
    for (auto& state : channel_)
    {
        std::fill(state.delay.begin(), state.delay.end(), 0.0f);
        std::fill(state.dryDelay.begin(), state.dryDelay.end(), 0.0f);
        std::fill(state.history.begin(), state.history.end(), 0.0f);
        state.lastGood = state.lastGoodDelta = state.hfState = 0.0f;
        state.carLow = state.carHighLow = 0.0f;
        state.carEnvelope = {};
    }
    delayIndex_ = dryDelayIndex_ = historyIndex_ = 0;
    historyFilled_ = 0;
    pendingDamage_.store(0.0f, std::memory_order_relaxed);
    pendingSkip_.store(0.0f, std::memory_order_relaxed);
    pendingSkipLoopSamples_.store(0, std::memory_order_relaxed);
    pendingSkipDurationSamples_.store(0, std::memory_order_relaxed);
    pendingSkipRestart_.store(false, std::memory_order_relaxed);
    jitterPhase_ = nextFloat(randomState_);
    jitterNoise_ = 0.0f;
    discPhase_ = nextFloat(randomState_);
    scratchPhaseA_ = nextFloat(randomState_);
    scratchPhaseB_ = std::fmod(scratchPhaseA_ + 0.17f + nextFloat(randomState_) * 0.46f, 1.0f);
    damageBucket_ = -1;
    damageRandomValue_ = nextFloat(randomState_);
    errorRemaining_ = errorTotal_ = 0;
    activeMode_ = CDConcealment::interpolate;
    defectScale_ = { 1.0f, 1.0f };
    repeatStart_ = repeatPosition_ = 0; repeatLength_ = 1; trackingRemaining_ = trackingTotal_ = trackingFadeSamples_ = 0;
    servoEnvelope_ = servoSweep_ = 0.0f;
    servoPhaseA_ = nextFloat(randomState_); servoPhaseB_ = nextFloat(randomState_);
    limiterEnvelope_ = outputPeak_ = 0.0f;
}

void CDProcessor::triggerDamage(float strength) noexcept
{
    auto current = pendingDamage_.load(std::memory_order_relaxed);
    const auto wanted = std::clamp(strength, 0.05f, 1.0f);
    while (current < wanted && !pendingDamage_.compare_exchange_weak(current, wanted, std::memory_order_relaxed)) {}
}

void CDProcessor::triggerSkip(float strength) noexcept
{
    pendingSkipLoopSamples_.store(0, std::memory_order_relaxed);
    pendingSkipDurationSamples_.store(0, std::memory_order_relaxed);
    pendingSkipRestart_.store(false, std::memory_order_relaxed);
    auto current = pendingSkip_.load(std::memory_order_relaxed);
    const auto wanted = std::clamp(strength, 0.05f, 1.0f);
    while (current < wanted && !pendingSkip_.compare_exchange_weak(current, wanted, std::memory_order_relaxed)) {}
}

void CDProcessor::triggerMusicalSkip(float strength, int loopSamples, int durationSamples,
                                     bool restartActive) noexcept
{
    pendingSkipLoopSamples_.store(std::max(1, loopSamples), std::memory_order_relaxed);
    pendingSkipDurationSamples_.store(std::max(1, durationSamples), std::memory_order_relaxed);
    pendingSkipRestart_.store(restartActive, std::memory_order_relaxed);
    auto current = pendingSkip_.load(std::memory_order_relaxed);
    const auto wanted = std::clamp(strength, 0.05f, 1.0f);
    while (current < wanted && !pendingSkip_.compare_exchange_weak(current, wanted, std::memory_order_release)) {}
}

std::uint32_t CDProcessor::nextU32(std::uint32_t& state) noexcept
{
    auto x = state == 0 ? 0x12345678u : state;
    x ^= x << 13u; x ^= x >> 17u; x ^= x << 5u; state = x; return x;
}

float CDProcessor::nextFloat(std::uint32_t& state) noexcept
{
    return (float) ((double) nextU32(state) / 4294967295.0);
}

float CDProcessor::nextSigned(std::uint32_t& state) noexcept
{
    return nextFloat(state) * 2.0f - 1.0f;
}

float CDProcessor::readLinear(const std::vector<float>& buffer, std::size_t writeIndex, float delaySamples) const noexcept
{
    if (buffer.size() < 2) return 0.0f;
    auto read = (float) writeIndex - std::clamp(delaySamples, 0.0f, (float) buffer.size() - 2.0f);
    while (read < 0.0f) read += (float) buffer.size();
    const auto base = std::floor(read);
    const auto first = (std::size_t) base % buffer.size();
    const auto second = (first + 1) % buffer.size();
    const auto fraction = read - base;
    return buffer[first] + (buffer[second] - buffer[first]) * fraction;
}

bool CDProcessor::crossedPhase(float previous, float next, float target) noexcept
{
    return next >= previous ? target > previous && target <= next : target > previous || target <= next;
}

CDConcealment CDProcessor::resolveMode(CDConcealment mode) noexcept
{
    if (mode != CDConcealment::random) return mode;
    const auto pick = nextFloat(randomState_);
    if (pick < 0.22f) return CDConcealment::hold;
    if (pick < 0.36f) return CDConcealment::mute;
    if (pick < 0.62f) return CDConcealment::interpolate;
    return CDConcealment::repeat;
}

float CDProcessor::damageWave(CDDamageShape shape, float phase) noexcept
{
    switch (shape)
    {
        case CDDamageShape::sine: return 0.5f + std::sin(phase * 2.0f * pi) * 0.5f;
        case CDDamageShape::triangle: return 1.0f - std::abs(phase * 2.0f - 1.0f);
        case CDDamageShape::square: return phase < 0.3f ? 1.0f : 0.035f;
        case CDDamageShape::saw: return phase;
        case CDDamageShape::randomPits:
        {
            const auto bucket = (int) std::floor(phase * 12.0f);
            if (bucket != damageBucket_) { damageBucket_ = bucket; damageRandomValue_ = nextFloat(randomState_); }
            return damageRandomValue_;
        }
        case CDDamageShape::radial: default: return 0.0f;
    }
}

void CDProcessor::prepareRepeat(int offsetSamples, int loopSamples) noexcept
{
    if (channel_[0].history.empty()) return;
    const auto available = std::max(2, std::min((int) channel_[0].history.size() - 2, historyFilled_));
    const auto safeOffset = std::min(available, std::max(2, offsetSamples));
    repeatStart_ = ((int) historyIndex_ - safeOffset + (int) channel_[0].history.size()) % (int) channel_[0].history.size();
    repeatPosition_ = 0;
    repeatLength_ = std::max(1, std::min(loopSamples, safeOffset - 1));
}

void CDProcessor::beginRepeat(int offsetSamples, int loopSamples, int durationSamples) noexcept
{
    prepareRepeat(offsetSamples, loopSamples);
    trackingRemaining_ = trackingTotal_ = std::max(repeatLength_, durationSamples);
}

void CDProcessor::process(float* const* channels, std::size_t channelCount, std::size_t sampleCount,
                          const CDParameters& raw) noexcept
{
    const auto count = std::min({ channelCount, preparedChannels_, maxChannels });
    if (count == 0 || sampleCount == 0 || channels == nullptr) return;
    const auto sr = (float) sampleRate_;
    auto p = raw;
    p.errorRate = clamp01(p.errorRate); p.scratchRate = clamp01(p.scratchRate); p.scratchAmount = clamp01(p.scratchAmount);
    p.correction = clamp01(p.correction); p.trackingRate = clamp01(p.trackingRate); p.servoHunt = clamp01(p.servoHunt);
    p.highFrequencyLoss = clamp01(p.highFrequencyLoss); p.servoNoise = clamp01(p.servoNoise);
    p.carCompression = clamp01(p.carCompression); p.stereoLink = clamp01(p.stereoLink);
    p.stereoWidth = std::clamp(p.stereoWidth, 0.0f, 2.0f); p.mix = clamp01(p.mix);
    p.ceiling = std::clamp(p.ceiling, 0.2f, 1.0f);

    const auto burstSamples = std::max(1, (int) std::round(std::clamp(p.burstMs, 1.0f, 600.0f) * 0.001f * sr));
    const auto interpolationSamples = std::max(1, (int) std::round(std::clamp(p.interpolationMs, 0.25f, 30.0f) * 0.001f * sr));
    const auto repeatSamples = std::max(1, (int) std::round(std::clamp(p.repeatMs, 1.0f, 400.0f) * 0.001f * sr));
    const auto trackingOffset = std::max(repeatSamples + 1, (int) std::round(std::clamp(p.trackingMs, 10.0f, 1800.0f) * 0.001f * sr));
    const auto jitterDepth = std::min(2.0f, std::max(0.0f, p.jitterMs)) * 0.001f * sr;
    const auto jitterHz = 7.0f + std::clamp(p.jitterRateHz, 1.0f, 200.0f);
    const auto randomErrorProbability = p.errorRate * p.errorRate * 0.000025f;
    const auto trackingProbability = p.trackingRate * p.trackingRate * (0.000004f + (1.0f - p.correction) * 0.000016f);
    const auto servoDecay = std::exp(-1.0f / ((0.055f + p.servoHunt * 0.3f) * sr));
    const auto hfCut = 3200.0f + (1.0f - p.highFrequencyLoss) * 17400.0f;
    const auto hfAlpha = std::exp(-2.0f * pi * std::min(hfCut, sr * 0.45f) / sr);
    const auto limiterAttack = std::exp(-1.0f / (0.0015f * sr));
    const auto limiterRelease = std::exp(-1.0f / (0.065f * sr));
    const auto inputGain = std::clamp(p.inputGain, 0.0f, 15.85f);
    const auto outputGain = std::clamp(p.outputGain, 0.0f, 1.5f);
    const auto rotationHz = std::clamp(p.rotationHz, 2.0f, 10.0f);
    outputPeak_ = 0.0f;

    for (std::size_t sample = 0; sample < sampleCount; ++sample)
    {
        std::array<float, maxChannels> dry {};
        std::array<float, maxChannels> dryAligned {};
        std::array<float, maxChannels> y {};
        const auto readRepeatedSample = [&](const ChannelState& state) noexcept
        {
            const auto position = repeatPosition_ % repeatLength_;
            const auto tailIndex = (repeatStart_ + position) % (int) state.history.size();
            const auto tail = state.history[(std::size_t) tailIndex];
            const auto crossfadeSamples = std::max(1, std::min(repeatLength_ / 4, (int) std::round(sr * .005f)));
            const auto crossfadeStart = repeatLength_ - crossfadeSamples;
            if (position < crossfadeStart) return tail;

            const auto headPosition = position - crossfadeStart;
            const auto headIndex = (repeatStart_ + headPosition) % (int) state.history.size();
            const auto blend = (float) headPosition / (float) crossfadeSamples;
            return tail + (state.history[(std::size_t) headIndex] - tail) * blend;
        };

        for (std::size_t channel = 0; channel < count; ++channel)
        {
            dry[channel] = channels[channel][sample];
            auto& state = channel_[channel];
            state.dryDelay[dryDelayIndex_] = dry[channel];
            state.delay[delayIndex_] = dry[channel] * inputGain;
            dryAligned[channel] = readLinear(state.dryDelay, dryDelayIndex_, (float) latencySamples_);
        }

        jitterNoise_ = jitterNoise_ * 0.997f + nextSigned(randomState_) * 0.003f;
        jitterPhase_ = std::fmod(jitterPhase_ + jitterHz / sr, 1.0f);
        const auto jitterMod = std::clamp(std::sin(jitterPhase_ * 2.0f * pi) * 0.72f + jitterNoise_ * 0.8f, -1.0f, 1.0f);
        for (std::size_t channel = 0; channel < count; ++channel)
        {
            const auto stereoOffset = channel == 0 ? 0.0f : (1.0f - p.stereoLink) * jitterDepth * 0.08f;
            const auto delaySamples = (float) latencySamples_ + jitterDepth * jitterMod * 0.5f + stereoOffset;
            y[channel] = readLinear(channel_[channel].delay, delayIndex_, std::max(1.0f, delaySamples));
            channel_[channel].history[historyIndex_] = y[channel];
        }
        delayIndex_ = (delayIndex_ + 1) % channel_[0].delay.size();
        dryDelayIndex_ = (dryDelayIndex_ + 1) % channel_[0].dryDelay.size();
        historyIndex_ = (historyIndex_ + 1) % channel_[0].history.size();
        historyFilled_ = std::min((int) channel_[0].history.size(), historyFilled_ + 1);

        const auto previousDiscPhase = discPhase_;
        discPhase_ = std::fmod(discPhase_ + rotationHz / sr, 1.0f);
        const auto passedA = crossedPhase(previousDiscPhase, discPhase_, scratchPhaseA_);
        const auto passedB = crossedPhase(previousDiscPhase, discPhase_, scratchPhaseB_);
        const auto shapedDamage = damageWave(p.damageShape, discPhase_);
        auto defectSeverity = 0.0f;
        if (errorRemaining_ > 0 || trackingRemaining_ > 0)
            pendingDamage_.exchange(0.0f, std::memory_order_relaxed);
        const auto manualStrength = errorRemaining_ <= 0 && trackingRemaining_ <= 0
            ? pendingDamage_.exchange(0.0f, std::memory_order_relaxed) : 0.0f;
        const auto manualDamage = manualStrength > 0.0f && errorRemaining_ <= 0 && trackingRemaining_ <= 0;
        if (manualDamage) defectSeverity = manualStrength;
        if (errorRemaining_ <= 0 && trackingRemaining_ <= 0 && nextFloat(randomState_) < randomErrorProbability)
            defectSeverity = std::max(defectSeverity, (0.16f + p.errorRate * 0.72f) * (0.55f + nextFloat(randomState_) * 0.45f));
        if (p.damageShape == CDDamageShape::radial && errorRemaining_ <= 0 && trackingRemaining_ <= 0 && (passedA || passedB))
        {
            const auto markScale = passedA ? 1.0f : 0.68f;
            if (nextFloat(randomState_) < p.scratchRate * markScale)
                defectSeverity = std::max(defectSeverity, p.scratchAmount * markScale * (0.72f + nextFloat(randomState_) * 0.28f));
        }
        if (p.damageShape != CDDamageShape::radial && errorRemaining_ <= 0 && trackingRemaining_ <= 0)
        {
            const auto probability = p.scratchRate * p.scratchRate * 0.00004f * (0.025f + shapedDamage * shapedDamage * 1.7f);
            if (nextFloat(randomState_) < probability)
                defectSeverity = std::max(defectSeverity, p.scratchAmount * (0.42f + shapedDamage * 0.58f)
                                                         * (0.72f + nextFloat(randomState_) * 0.28f));
        }

        if (defectSeverity > 0.0f)
        {
            const auto recoveryDemand = defectSeverity * (0.78f + nextFloat(randomState_) * 0.32f);
            servoEnvelope_ = std::max(servoEnvelope_, defectSeverity * (0.22f + p.servoHunt * 0.78f));
            servoSweep_ = std::max(servoSweep_, defectSeverity);
            if (manualDamage || recoveryDemand > p.correction)
            {
                const auto overload = std::clamp((recoveryDemand - p.correction) / std::max(0.08f, 1.0f - p.correction), 0.0f, 1.0f);
                const auto length = std::max(1, (int) std::round(burstSamples * (manualDamage ? 1.35f : 0.25f + overload * 1.4f)));
                errorRemaining_ = errorTotal_ = length;
                activeMode_ = resolveMode(p.mode);
                defectScale_[0] = 1.0f;
                defectScale_[1] = p.stereoLink + (1.0f - p.stereoLink) * (0.48f + nextFloat(randomState_) * 0.42f);
                if (activeMode_ == CDConcealment::repeat)
                    prepareRepeat(repeatSamples, repeatSamples);
            }
        }

        auto requestedLoop = pendingSkipLoopSamples_.load(std::memory_order_relaxed);
        auto requestedDuration = pendingSkipDurationSamples_.load(std::memory_order_relaxed);
        const auto musicalSkip = requestedLoop > 0 && requestedDuration > 0;
        const auto requestedHistory = musicalSkip ? requestedLoop + 2
            : std::max(repeatSamples * 2, std::min(trackingOffset, (int) std::round(sr * 0.08f)));
        auto skipStrength = 0.0f;
        auto restartSkip = false;
        if (pendingSkip_.load(std::memory_order_acquire) > 0.0f)
        {
            restartSkip = pendingSkipRestart_.load(std::memory_order_relaxed);
            const auto eventBusy = trackingRemaining_ > 0 || errorRemaining_ > 0;
            if (eventBusy && !restartSkip)
            {
                // Performer clock events never form a hidden queue. A skip is
                // either the event happening now or it is deliberately ignored.
                pendingSkip_.exchange(0.0f, std::memory_order_acq_rel);
                pendingSkipLoopSamples_.store(0, std::memory_order_relaxed);
                pendingSkipDurationSamples_.store(0, std::memory_order_relaxed);
            }
            else if ((!eventBusy || restartSkip) && historyFilled_ >= requestedHistory)
            {
                skipStrength = pendingSkip_.exchange(0.0f, std::memory_order_acq_rel);
                requestedLoop = pendingSkipLoopSamples_.exchange(0, std::memory_order_relaxed);
                requestedDuration = pendingSkipDurationSamples_.exchange(0, std::memory_order_relaxed);
                pendingSkipRestart_.store(false, std::memory_order_relaxed);
            }
            else if (musicalSkip && historyFilled_ < requestedHistory)
            {
                // A quantized event that cannot capture its complete slice is
                // dropped. Letting it fire later would move it off the grid.
                pendingSkip_.exchange(0.0f, std::memory_order_acq_rel);
                pendingSkipLoopSamples_.store(0, std::memory_order_relaxed);
                pendingSkipDurationSamples_.store(0, std::memory_order_relaxed);
                pendingSkipRestart_.store(false, std::memory_order_relaxed);
            }
        }
        if (skipStrength > 0.0f && ((trackingRemaining_ <= 0 && errorRemaining_ <= 0) || restartSkip))
        {
            const auto resolvedLoop = requestedLoop > 0 ? requestedLoop : repeatSamples;
            const auto resolvedOffset = requestedLoop > 0 ? resolvedLoop + 1 : trackingOffset;
            const auto resolvedDuration = requestedDuration > 0 ? requestedDuration
                : (int) std::round(sr * (0.22f + skipStrength * 1.45f));
            beginRepeat(resolvedOffset, resolvedLoop, resolvedDuration);
            trackingFadeSamples_ = std::max(1, std::min(resolvedLoop / 4, (int) std::round(sr * .005f)));
            defectScale_ = { 1.0f, 1.0f };
            servoEnvelope_ = std::max(servoEnvelope_, 0.45f + skipStrength * 0.55f);
            servoSweep_ = std::max(servoSweep_, 0.55f + skipStrength * 0.45f);
            errorRemaining_ = 0;
        }
        if (trackingRemaining_ <= 0 && nextFloat(randomState_) < trackingProbability)
        {
            beginRepeat(trackingOffset, repeatSamples, (int) std::round(sr * (0.16f + p.trackingRate * 1.75f)));
            trackingFadeSamples_ = std::max(1, std::min(repeatSamples / 4, (int) std::round(sr * .005f)));
            defectScale_ = { 1.0f, 1.0f };
            servoEnvelope_ = std::max(servoEnvelope_, 0.35f + p.trackingRate * 0.65f);
            servoSweep_ = std::max(servoSweep_, 0.5f + p.servoHunt * 0.5f);
            errorRemaining_ = 0;
        }

        for (std::size_t channel = 0; channel < count; ++channel)
        {
            auto& state = channel_[channel];
            const auto unaffected = y[channel];
            auto concealed = unaffected;
            if (trackingRemaining_ > 0)
            {
                concealed = readRepeatedSample(state);
            }
            else if (errorRemaining_ > 0)
            {
                const auto progress = 1.0f - (float) errorRemaining_ / (float) std::max(1, errorTotal_);
                const auto elapsed = errorTotal_ - errorRemaining_;
                if (activeMode_ == CDConcealment::interpolate)
                {
                    const auto repairSamples = std::min(interpolationSamples, std::max(1, errorTotal_));
                    const auto predicted = state.lastGood + state.lastGoodDelta * (float) std::min(20, elapsed + 1);
                    const auto repairProgress = std::clamp((float) elapsed / (float) std::max(1, repairSamples), 0.0f, 1.0f);
                    concealed = predicted * (1.0f - repairProgress) + unaffected * repairProgress;
                }
                else if (activeMode_ == CDConcealment::mute)
                {
                    const auto edge = std::min({ 1.0f, progress * 12.0f, ((float) errorRemaining_ / (float) std::max(1, errorTotal_)) * 12.0f });
                    concealed = unaffected * (1.0f - edge);
                }
                else if (activeMode_ == CDConcealment::repeat)
                {
                    concealed = readRepeatedSample(state);
                }
                else concealed = state.lastGood;
            }
            else
            {
                const auto delta = unaffected - state.lastGood;
                state.lastGoodDelta = state.lastGoodDelta * 0.82f + delta * 0.18f;
                state.lastGood = unaffected;
            }
            auto eventBlend = 1.0f;
            if (trackingRemaining_ > 0 && trackingFadeSamples_ > 0)
            {
                const auto elapsed = trackingTotal_ - trackingRemaining_;
                const auto fadeIn = std::clamp((float) elapsed / (float) trackingFadeSamples_, 0.0f, 1.0f);
                const auto fadeOut = std::clamp((float) trackingRemaining_ / (float) trackingFadeSamples_, 0.0f, 1.0f);
                eventBlend = std::min(fadeIn, fadeOut);
            }
            else if (errorRemaining_ > 0)
            {
                // Damage is allowed to be abrupt in character, but entering or
                // leaving the concealment path on a single sample produces the
                // unmistakable click of a broken audio stream.  A very short
                // edge crossfade keeps the defect intact without manufacturing
                // an unrelated full-scale impulse.
                const auto elapsed = errorTotal_ - errorRemaining_;
                const auto fadeSamples = std::max(1, std::min(errorTotal_ / 4, (int) std::round(sr * .003f)));
                const auto fadeIn = std::clamp((float) elapsed / (float) fadeSamples, 0.0f, 1.0f);
                const auto fadeOut = std::clamp((float) errorRemaining_ / (float) fadeSamples, 0.0f, 1.0f);
                eventBlend = std::min(fadeIn, fadeOut);
            }
            y[channel] = unaffected + (concealed - unaffected) * defectScale_[channel] * eventBlend;
        }
        if (trackingRemaining_ > 0) { ++repeatPosition_; --trackingRemaining_; }
        else if (errorRemaining_ > 0) { if (activeMode_ == CDConcealment::repeat) ++repeatPosition_; --errorRemaining_; }

        servoEnvelope_ *= servoDecay;
        servoSweep_ *= 0.9996f;
        auto sharedServo = 0.0f;
        if (p.servoNoise > 0.0001f && servoEnvelope_ > 0.00005f)
        {
            const auto seek = p.servoHunt * servoSweep_;
            servoPhaseA_ = std::fmod(servoPhaseA_ + (82.0f + rotationHz * 10.0f + seek * 170.0f) / sr, 1.0f);
            servoPhaseB_ = std::fmod(servoPhaseB_ + (540.0f + seek * 1250.0f) / sr, 1.0f);
            sharedServo = (std::sin(servoPhaseA_ * 2.0f * pi) * 0.62f
                         + std::sin(servoPhaseB_ * 2.0f * pi + std::sin(servoPhaseA_ * 2.0f * pi) * 0.7f) * 0.38f)
                        * servoEnvelope_ * p.servoNoise * 0.018f;
        }

        for (std::size_t channel = 0; channel < count; ++channel)
        {
            auto& state = channel_[channel];
            const auto chatter = nextSigned(randomState_) * (1.0f - p.stereoLink) * servoEnvelope_ * p.servoNoise * 0.0025f;
            y[channel] += sharedServo + chatter;
            state.hfState = (1.0f - hfAlpha) * y[channel] + hfAlpha * state.hfState;
            y[channel] = state.hfState;

            if (p.carCompression > 0.0001f)
            {
                const auto lowAlpha = std::exp(-2.0f * pi * 220.0f / sr);
                const auto highAlpha = std::exp(-2.0f * pi * 3200.0f / sr);
                state.carLow = (1.0f - lowAlpha) * y[channel] + lowAlpha * state.carLow;
                state.carHighLow = (1.0f - highAlpha) * y[channel] + highAlpha * state.carHighLow;
                const std::array<float, 3> bands { state.carLow, state.carHighLow - state.carLow, y[channel] - state.carHighLow };
                auto compressed = 0.0f;
                for (std::size_t band = 0; band < bands.size(); ++band)
                {
                    const auto magnitude = std::abs(bands[band]);
                    const auto coefficient = magnitude > state.carEnvelope[band] ? 0.18f : 0.002f;
                    state.carEnvelope[band] += (magnitude - state.carEnvelope[band]) * coefficient;
                    const auto threshold = band == 2 ? 0.035f : 0.06f;
                    const auto gain = state.carEnvelope[band] > threshold
                        ? std::pow(threshold / (state.carEnvelope[band] + 1.0e-6f), 0.58f) : 1.0f;
                    compressed += bands[band] * gain * (band == 0 ? 1.12f : band == 1 ? 1.08f : 1.02f);
                }
                y[channel] += (compressed - y[channel]) * p.carCompression;
            }
        }

        if (count == 2)
        {
            const auto mid = (y[0] + y[1]) * 0.5f;
            const auto side = (y[0] - y[1]) * 0.5f * p.stereoWidth;
            y[0] = mid + side; y[1] = mid - side;
        }
        auto peak = 0.0f;
        for (std::size_t channel = 0; channel < count; ++channel)
        {
            y[channel] *= outputGain;
            if (p.softClip) y[channel] = std::tanh(y[channel]);
            peak = std::max(peak, std::abs(y[channel]));
        }
        const auto limiterCoefficient = peak > limiterEnvelope_ ? limiterAttack : limiterRelease;
        limiterEnvelope_ = peak + limiterCoefficient * (limiterEnvelope_ - peak);
        const auto limiterGain = limiterEnvelope_ > p.ceiling ? p.ceiling / (limiterEnvelope_ + 1.0e-6f) : 1.0f;
        for (std::size_t channel = 0; channel < count; ++channel)
        {
            const auto processed = std::clamp(y[channel] * limiterGain, -p.ceiling, p.ceiling);
            const auto output = std::clamp(dryAligned[channel] * (1.0f - p.mix) + processed * p.mix, -1.0f, 1.0f);
            channels[channel][sample] = output;
            outputPeak_ = std::max(outputPeak_, std::abs(output));
        }
    }
}
}
