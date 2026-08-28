#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace lost_audio::core
{
enum class CDConcealment : int { hold = 0, mute, interpolate, repeat, random };
enum class CDDamageShape : int { radial = 0, sine, triangle, square, saw, randomPits };

struct CDParameters
{
    CDConcealment mode = CDConcealment::interpolate;
    CDDamageShape damageShape = CDDamageShape::radial;
    float errorRate = 0.12f;
    float burstMs = 18.0f;
    float repeatMs = 36.0f;
    float scratchRate = 0.14f;
    float scratchAmount = 0.2f;
    float correction = 0.88f;
    float interpolationMs = 5.0f;
    float rotationHz = 5.2f;
    float trackingRate = 0.08f;
    float trackingMs = 140.0f;
    float servoHunt = 0.18f;
    float jitterMs = 0.025f;
    float jitterRateHz = 34.0f;
    float highFrequencyLoss = 0.025f;
    float servoNoise = 0.08f;
    float carCompression = 0.0f;
    float stereoLink = 1.0f;
    float stereoWidth = 1.0f;
    float inputGain = 1.0f;
    float outputGain = 0.98f;
    float mix = 1.0f;
    float ceiling = 0.94f;
    bool softClip = false;
};

struct CDMacroTargets
{
    float errorRate = 0.12f;
    float burstMs = 18.0f;
    float repeatMs = 36.0f;
    float scratchRate = 0.14f;
    float scratchAmount = 0.2f;
    float correction = 0.88f;
    float interpolationMs = 5.0f;
    float rotationHz = 5.2f;
    float trackingRate = 0.08f;
    float trackingMs = 140.0f;
    float servoHunt = 0.18f;
    float jitterMs = 0.025f;
    float jitterRateHz = 34.0f;
    float highFrequencyLoss = 0.025f;
    float servoNoise = 0.08f;
    float outputGain = 0.98f;
    float ceiling = 0.94f;
};

[[nodiscard]] CDMacroTargets mapCDMacros(float clarity, float damage, float tracking, float jitter) noexcept;

class CDProcessor
{
public:
    static constexpr std::size_t maxChannels = 2;
    static constexpr double fixedLatencySeconds = 0.0025;

    void prepare(double sampleRate, std::size_t channelCount);
    void reset(std::uint32_t seed = 0x4344454eu) noexcept;
    void process(float* const* channels, std::size_t channelCount, std::size_t sampleCount,
                 const CDParameters& parameters) noexcept;
    void triggerDamage(float strength = 1.0f) noexcept;
    void triggerSkip(float strength = 1.0f) noexcept;

    [[nodiscard]] int latencySamples() const noexcept { return latencySamples_; }
    [[nodiscard]] float outputPeak() const noexcept { return outputPeak_; }
    [[nodiscard]] bool damageActive() const noexcept { return errorRemaining_ > 0; }
    [[nodiscard]] bool skipActive() const noexcept { return trackingRemaining_ > 0; }

private:
    struct ChannelState
    {
        std::vector<float> delay;
        std::vector<float> dryDelay;
        std::vector<float> history;
        float lastGood = 0.0f;
        float lastGoodDelta = 0.0f;
        float hfState = 0.0f;
        float carLow = 0.0f;
        float carHighLow = 0.0f;
        std::array<float, 3> carEnvelope {};
    };

    [[nodiscard]] float readLinear(const std::vector<float>& buffer, std::size_t writeIndex, float delaySamples) const noexcept;
    [[nodiscard]] CDConcealment resolveMode(CDConcealment mode) noexcept;
    [[nodiscard]] float damageWave(CDDamageShape shape, float phase) noexcept;
    void prepareRepeat(int offsetSamples, int loopSamples) noexcept;
    void beginRepeat(int offsetSamples, int loopSamples, int durationSamples) noexcept;
    [[nodiscard]] static bool crossedPhase(float previous, float next, float target) noexcept;
    [[nodiscard]] static std::uint32_t nextU32(std::uint32_t& state) noexcept;
    [[nodiscard]] static float nextFloat(std::uint32_t& state) noexcept;
    [[nodiscard]] static float nextSigned(std::uint32_t& state) noexcept;

    std::array<ChannelState, maxChannels> channel_ {};
    double sampleRate_ = 48000.0;
    std::size_t preparedChannels_ = 0;
    int latencySamples_ = 120;
    std::size_t delayIndex_ = 0;
    std::size_t dryDelayIndex_ = 0;
    std::size_t historyIndex_ = 0;
    int historyFilled_ = 0;

    std::uint32_t seed_ = 0x4344454eu;
    std::uint32_t randomState_ = 0x4344454eu;
    std::atomic<float> pendingDamage_ { 0.0f };
    std::atomic<float> pendingSkip_ { 0.0f };

    float jitterPhase_ = 0.0f;
    float jitterNoise_ = 0.0f;
    float discPhase_ = 0.0f;
    float scratchPhaseA_ = 0.0f;
    float scratchPhaseB_ = 0.0f;
    int damageBucket_ = -1;
    float damageRandomValue_ = 0.0f;

    int errorRemaining_ = 0;
    int errorTotal_ = 0;
    CDConcealment activeMode_ = CDConcealment::interpolate;
    std::array<float, maxChannels> defectScale_ { 1.0f, 1.0f };
    int repeatStart_ = 0;
    int repeatPosition_ = 0;
    int repeatLength_ = 1;
    int trackingRemaining_ = 0;

    float servoEnvelope_ = 0.0f;
    float servoSweep_ = 0.0f;
    float servoPhaseA_ = 0.0f;
    float servoPhaseB_ = 0.0f;
    float limiterEnvelope_ = 0.0f;
    float outputPeak_ = 0.0f;
};
}
