#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace lost_audio::core
{
struct TransmissionParameters
{
    float highPassHz = 380.0f;
    float lowPassHz = 5200.0f;
    float midGainDb = 0.0f;
    float midFrequencyHz = 1550.0f;
    float midQ = 1.2f;
    float boxDipDb = 0.0f;
    float drive = 0.35f;
    float asymmetry = 0.1f;
    float compression = 0.25f;
    float crush = 0.0f;
    float wowDepth = 0.25f;
    float dropoutRate = 0.25f;
    float dropoutDepth = 0.35f;
    float crackle = 0.25f;
    float lfoRateHz = 0.7f;
    float noise = 0.2f;
    float noiseColor = 0.0f;
    float hiss = 0.2f;
    float inputGain = 1.0f;
    float outputGain = 0.92f;
    float mix = 1.0f;
    float ceiling = 1.0f;
    int passes = 1;
    bool walkieEnabled = false;
    float walkieThresholdDb = -45.0f;
    float walkieMinSilenceMs = 220.0f;
    float walkieClickMs = 12.0f;
    float walkieClickLevel = 0.65f;
    bool walkieDispatchMode = false;
};

struct TransmissionMacroTargets
{
    float highPassHz = 380.0f;
    float lowPassHz = 5200.0f;
    float midGainDb = 0.0f;
    float midFrequencyHz = 1550.0f;
    float midQ = 1.2f;
    float boxDipDb = 0.0f;
    float asymmetry = 0.1f;
    float compression = 0.25f;
    float wowDepth = 0.25f;
    float dropoutRate = 0.25f;
    float dropoutDepth = 0.35f;
    float crackle = 0.25f;
    float lfoRateHz = 0.7f;
    float noiseColor = 0.0f;
    float hiss = 0.2f;
};

[[nodiscard]] TransmissionMacroTargets mapTransmissionMacros(
    float bandwidth, float drive, float badConnection, float noiseProfile) noexcept;

class TransmissionProcessor
{
public:
    static constexpr std::size_t maxChannels = 2;
    static constexpr std::size_t maxPasses = 6;
    static constexpr double stageLatencySeconds = 0.006;

    void prepare(double sampleRate, std::size_t channelCount);
    void reset(std::uint32_t seed = 0x7472616eu) noexcept;
    void triggerDropout(float strength = 1.0f, float durationSeconds = 0.08f) noexcept;
    void process(float* const* channels, std::size_t channelCount, std::size_t sampleCount,
                 const TransmissionParameters& parameters) noexcept;

    [[nodiscard]] int latencySamples() const noexcept;
    [[nodiscard]] double sampleRate() const noexcept { return sampleRate_; }
    [[nodiscard]] float carrierDisplacementMs() const noexcept { return carrierDisplacementMs_; }
    [[nodiscard]] bool dropoutActive() const noexcept { return dropoutActive_; }
    [[nodiscard]] float dropoutProgress() const noexcept { return dropoutProgress_; }
    [[nodiscard]] float compressionReduction() const noexcept { return compressionReduction_; }
    [[nodiscard]] float noiseActivity() const noexcept { return noiseActivity_; }
    [[nodiscard]] float crackleActivity() const noexcept { return crackleActivity_; }
    [[nodiscard]] float limiterActivity() const noexcept { return limiterActivity_; }
    [[nodiscard]] bool squelchClosed() const noexcept { return walkie_.inSilence; }
    [[nodiscard]] float squelchEventActivity() const noexcept
    {
        return walkie_.clickTotal > 0 ? static_cast<float>(walkie_.clickRemaining) / static_cast<float>(walkie_.clickTotal) : 0.0f;
    }

private:
    struct Biquad
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
        float z1 = 0.0f, z2 = 0.0f;
        void reset() noexcept { z1 = z2 = 0.0f; }
        float process(float input) noexcept;
        void setHighPass(double sampleRate, float frequency, float q) noexcept;
        void setLowPass(double sampleRate, float frequency, float q) noexcept;
        void setPeak(double sampleRate, float frequency, float q, float gainDb) noexcept;
    };

    struct PinkNoiseState
    {
        float p0 = 0.0f, p1 = 0.0f, p2 = 0.0f, p3 = 0.0f;
        float p4 = 0.0f, p5 = 0.0f, p6 = 0.0f;
    };

    struct StageState
    {
        Biquad hp1, hp2, lp1, lp2, dip, mid;
        std::vector<float> delay;
        std::size_t delayIndex = 0;
        float envelope = 0.0f;
        float crushHold = 0.0f;
        int crushPhase = 0;
        float lfoPhase = 0.0f;
        float driftNoise = 0.0f;
        PinkNoiseState pink;
        float previousNoise = 0.0f;
        std::uint32_t randomState = 1u;
        int dropoutRemaining = 0;
        int dropoutTotal = 0;
        float dropoutDepth = 1.0f;
        int crackleRemaining = 0;
        int crackleTotal = 0;
        float crackleState = 0.0f;
    };

    struct DryDelay
    {
        std::vector<float> samples;
        std::size_t index = 0;
    };

    struct WalkieState
    {
        std::vector<float> rmsRing;
        std::size_t ringIndex = 0;
        float sumSquares = 0.0f;
        int belowCount = 0;
        bool inSilence = false;
        int clickRemaining = 0;
        int clickTotal = 0;
        float clickAmplitude = 0.0f;
        float clickFrequency = 1800.0f;
        float clickPhase = 0.0f;
        float noiseHpState = 0.0f;
        std::uint32_t randomState = 1u;
    };

    void updateFilters(const TransmissionParameters& parameters) noexcept;
    [[nodiscard]] float processStage(StageState& state, float input, bool active,
                                     int activePasses, const TransmissionParameters& parameters) noexcept;
    [[nodiscard]] float processWalkie(float monoInput, const TransmissionParameters& parameters) noexcept;
    [[nodiscard]] float readDelay(const StageState& state, float delaySamples) const noexcept;
    [[nodiscard]] static std::uint32_t mixSeed(std::uint32_t base, std::uint32_t salt) noexcept;
    [[nodiscard]] static std::uint32_t nextU32(std::uint32_t& state) noexcept;
    [[nodiscard]] static float nextFloat(std::uint32_t& state) noexcept;
    [[nodiscard]] static float nextSigned(std::uint32_t& state) noexcept;
    [[nodiscard]] static float pinkFromWhite(PinkNoiseState& state, float white) noexcept;

    std::array<std::array<StageState, maxPasses>, maxChannels> stages_ {};
    std::array<DryDelay, maxChannels> dryDelay_ {};
    WalkieState walkie_ {};
    double sampleRate_ = 48000.0;
    std::size_t preparedChannels_ = 0;
    int stageDelaySamples_ = 288;
    std::uint32_t seed_ = 0x7472616eu;
    std::atomic<float> pendingDropoutStrength_ { 0.0f }, pendingDropoutDuration_ { 0.0f };
    float carrierDisplacementMs_ = 0.0f, dropoutProgress_ = 0.0f;
    float compressionReduction_ = 0.0f, noiseActivity_ = 0.0f;
    float crackleActivity_ = 0.0f, limiterActivity_ = 0.0f;
    bool dropoutActive_ = false;
};
}
