#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace lost_audio::core
{
enum class ConferenceMode : int { discord = 0, zoom, skype, cellular };
enum class ConferenceConcealment : int { hold = 0, mute, interpolate, repeat };

struct ConferenceParameters
{
    ConferenceMode mode = ConferenceMode::discord;
    ConferenceConcealment concealment = ConferenceConcealment::hold;
    float highPassHz = 260.0f;
    float lowPassHz = 4200.0f;
    float midHumpDb = 2.2f;
    float midFrequencyHz = 1750.0f;
    float packetLoss = 0.012f;
    float packetMs = 20.0f;
    float repeatMs = 38.0f;
    float jitterMs = 0.35f;
    float jitterRate = 18.0f;
    float gate = 0.12f;
    int bits = 12;
    float converterRateHz = 24000.0f;
    float robot = 0.02f;
    float noise = 0.04f;
    float burstiness = 0.28f;
    float suppression = 0.42f;
    float agc = 0.34f;
    float bufferSlip = 0.02f;
    float bandwidthSwitch = 0.03f;
    float comfortNoise = 0.10f;
    float inputGain = 1.0f;
    float outputGain = 0.98f;
    float mix = 1.0f;
    float ceiling = 0.92f;
};

struct ConferenceMacroTargets
{
    float highPassHz = 260.0f, lowPassHz = 4200.0f, midHumpDb = 2.2f, midFrequencyHz = 1750.0f;
    float packetLoss = 0.045f, packetMs = 20.0f, repeatMs = 38.0f;
    float jitterMs = 0.35f, jitterRate = 18.0f, gate = 0.12f;
    int bits = 12;
    float converterRateHz = 24000.0f;
    float robot = 0.12f, noise = 0.12f, burstiness = 0.56f, suppression = 0.42f;
    float agc = 0.34f, bufferSlip = 0.08f, bandwidthSwitch = 0.12f, comfortNoise = 0.22f;
    float outputGain = 0.98f, ceiling = 0.92f;
};

[[nodiscard]] ConferenceMacroTargets mapConferenceMacros(
    ConferenceMode mode, float bandwidth, float codec, float dropouts,
    float jitter, float robot, float noise) noexcept;

class ConferenceProcessor
{
public:
    static constexpr std::size_t maxChannels = 2;

    void prepare(double sampleRate, std::size_t channelCount);
    void reset(std::uint32_t seed = 0x636f6e66u) noexcept;
    void process(float* const* channels, std::size_t channelCount, std::size_t sampleCount,
                 const ConferenceParameters& parameters) noexcept;
    void triggerPacketLoss(float depth, float durationSeconds) noexcept;
    void triggerRobot(float strength, float durationSeconds, float grainMilliseconds) noexcept;

    [[nodiscard]] int latencySamples() const noexcept { return latencySamples_; }
    [[nodiscard]] float inputPeak(std::size_t channel) const noexcept { return inputPeak_[channel < maxChannels ? channel : 0]; }
    [[nodiscard]] float outputPeak(std::size_t channel) const noexcept { return outputPeak_[channel < maxChannels ? channel : 0]; }
    [[nodiscard]] bool packetLost() const noexcept { return packetLost_ || manualLossRemaining_ > 0; }
    [[nodiscard]] bool robotActive() const noexcept { return robotRemaining_ > 0; }
    [[nodiscard]] bool bufferSlipActive() const noexcept { return slipIndicator_ > 0; }
    [[nodiscard]] bool bandwidthCollapsed() const noexcept { return narrowFrames_ > 0; }
    [[nodiscard]] float packetLossProgress() const noexcept { return lossProgress_; }
    [[nodiscard]] float robotProgress() const noexcept { return robotProgress_; }
    [[nodiscard]] float jitterActivity() const noexcept { return jitterActivity_; }
    [[nodiscard]] float suppressionActivity() const noexcept { return suppressionActivity_; }
    [[nodiscard]] float agcActivity() const noexcept { return agcActivity_; }
    [[nodiscard]] float comfortNoiseActivity() const noexcept { return comfortNoiseActivity_; }
    [[nodiscard]] float limiterActivity() const noexcept { return limiterActivity_; }

private:
    struct Biquad
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f, z1 = 0.0f, z2 = 0.0f;
        void reset() noexcept { z1 = z2 = 0.0f; }
        float process(float input) noexcept;
        void setHighPass(double sampleRate, float frequency, float q) noexcept;
        void setLowPass(double sampleRate, float frequency, float q) noexcept;
        void setPeak(double sampleRate, float frequency, float q, float gainDb) noexcept;
    };

    struct ChannelState
    {
        std::vector<float> wetHistory, dryHistory, robotGrain;
        std::size_t writeIndex = 0;
        std::array<Biquad, 6> tone {};
        float envelope = 0.0f, gateGain = 1.0f, agcGain = 1.0f;
        float heldSample = 0.0f, codecPrevious = 0.0f, smear = 0.0f;
        float narrowSmear = 0.0f, narrowBlend = 0.0f;
        float lastGood = 0.0f, lossStart = 0.0f, comfortState = 0.0f;
        float lossBlend = 0.0f;
        int rateCounter = 0;
    };

    void updateFilters(const ConferenceParameters& parameters) noexcept;
    [[nodiscard]] float readHistory(const std::vector<float>& history, std::size_t writeIndex, float delaySamples) const noexcept;
    [[nodiscard]] static std::uint32_t nextU32(std::uint32_t& state) noexcept;
    [[nodiscard]] static float nextFloat(std::uint32_t& state) noexcept;
    [[nodiscard]] static float nextSigned(std::uint32_t& state) noexcept;

    std::array<ChannelState, maxChannels> channel_ {};
    double sampleRate_ = 48000.0;
    std::size_t preparedChannels_ = 0;
    int latencySamples_ = 96;
    std::uint32_t seed_ = 0x636f6e66u, randomState_ = 0x636f6e66u;
    int frameRemaining_ = 0, frameLength_ = 960;
    bool badBurst_ = false, packetLost_ = false;
    int manualLossRemaining_ = 0, manualLossTotal_ = 1;
    float manualLossDepth_ = 1.0f;
    bool previousLossState_ = false;
    int recoveryRemaining_ = 0, recoveryLength_ = 1;
    float jitterTarget_ = 0.0f, jitterCurrent_ = 0.0f;
    int slipIndicator_ = 0;
    int narrowFrames_ = 0;
    int robotRemaining_ = 0, robotTotal_ = 1, robotLength_ = 0, robotPosition_ = 0;
    float robotBlend_ = 1.0f, robotEnvelope_ = 0.0f;
    bool pendingRobotTrigger_ = false;
    int requestedRobotDuration_ = 0, requestedRobotGrain_ = 0;
    float requestedRobotStrength_ = 1.0f;
    float lossProgress_ = 0.0f, robotProgress_ = 0.0f, jitterActivity_ = 0.0f;
    float suppressionActivity_ = 0.0f, agcActivity_ = 0.0f, comfortNoiseActivity_ = 0.0f;
    float limiterActivity_ = 0.0f;
    std::array<float, maxChannels> inputPeak_ {}, outputPeak_ {};
};
}
