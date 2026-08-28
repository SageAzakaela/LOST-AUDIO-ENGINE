#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace lost_audio::core
{
enum class CommsMode : int
{
    landline = 0,
    cellular,
    intercom,
    publicAddress,
    alarmPanel
};

struct CommsParameters
{
    CommsMode mode = CommsMode::landline;
    float highPassHz = 280.0f;
    float lowPassHz = 3400.0f;
    float midHumpDb = 3.5f;
    float midFrequencyHz = 1850.0f;
    float drive = 0.35f;
    float compression = 0.45f;
    int bits = 12;
    float converterRateHz = 24000.0f;
    float packetLoss = 0.2f;
    float packetLengthMs = 28.0f;
    float hum = 0.25f;
    float hiss = 0.22f;
    float toneMix = 0.35f;
    float transducer = 0.45f;
    float lineAge = 0.2f;
    float duplex = 0.08f;
    float speakerRattle = 0.12f;
    float distance = 0.15f;
    bool alarmTone = false;
    float echoMix = 0.0f;
    float echoMs = 180.0f;
    float echoFeedback = 0.28f;
    float echoTone = 0.55f;
    float roomMix = 0.0f;
    float roomMs = 240.0f;
    float roomDamping = 0.45f;
    float inputGain = 1.0f;
    float outputGain = 0.95f;
    float mix = 1.0f;
    float ceiling = 0.92f;
};

struct CommsMacroTargets
{
    float highPassHz = 280.0f;
    float lowPassHz = 3400.0f;
    float midHumpDb = 3.5f;
    float midFrequencyHz = 1850.0f;
    float compression = 0.45f;
    int bits = 12;
    float converterRateHz = 24000.0f;
    float packetLoss = 0.2f;
    float packetLengthMs = 28.0f;
    float hum = 0.25f;
    float hiss = 0.22f;
    float toneMix = 0.35f;
    float transducer = 0.45f;
    float lineAge = 0.2f;
    float duplex = 0.08f;
    float speakerRattle = 0.12f;
    float distance = 0.15f;
    float echoMix = 0.0f;
    float echoMs = 180.0f;
    float echoFeedback = 0.28f;
    float echoTone = 0.55f;
    float roomMix = 0.0f;
    float roomMs = 240.0f;
    float roomDamping = 0.45f;
    float outputGain = 0.95f;
    float ceiling = 0.92f;
};

[[nodiscard]] CommsMacroTargets mapCommsMacros(
    CommsMode mode, float bandwidth, float drive, float glitch,
    float noise, float character, float distance) noexcept;

class CommsProcessor
{
public:
    static constexpr std::size_t maxChannels = 2;

    void prepare(double sampleRate, std::size_t channelCount);
    void reset(std::uint32_t seed = 0x636f6d6du) noexcept;
    void process(float* const* channels, std::size_t channelCount, std::size_t sampleCount,
                 const CommsParameters& parameters) noexcept;

    [[nodiscard]] double sampleRate() const noexcept { return sampleRate_; }
    [[nodiscard]] float outputPeak() const noexcept { return outputPeak_; }

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

    struct FilterChain
    {
        Biquad hp1, hp2, dip, hump, lp1, lp2;
        Biquad bodyLow, bodyHigh, bodyNotch;
        void reset() noexcept;
    };

    struct DelayLine
    {
        std::vector<float> samples;
        std::size_t writeIndex = 0;
        float dampingState = 0.0f;
    };

    void updateFilters(const CommsParameters& parameters) noexcept;
    [[nodiscard]] float readDelay(const DelayLine& line, float delaySamples) const noexcept;
    [[nodiscard]] static std::uint32_t nextU32(std::uint32_t& state) noexcept;
    [[nodiscard]] static float nextFloat(std::uint32_t& state) noexcept;
    [[nodiscard]] static float nextSigned(std::uint32_t& state) noexcept;

    FilterChain filters_ {};
    DelayLine echo_ {};
    std::array<DelayLine, 4> room_ {};
    double sampleRate_ = 48000.0;
    std::size_t preparedChannels_ = 0;
    std::uint32_t seed_ = 0x636f6d6du;
    std::uint32_t randomState_ = 0x636f6d6du;

    float envelope_ = 0.0f;
    float limiterEnvelope_ = 0.0f;
    float hold_ = 0.0f;
    int holdCount_ = 0;
    int holdPeriod_ = 1;
    int dropoutRemaining_ = 0;
    int packetRemaining_ = 0;
    float dropoutGain_ = 1.0f;
    float humPhase_ = 0.0f;
    float lineNoise_ = 0.0f;
    float carbonNoise_ = 0.0f;
    float duplexGain_ = 1.0f;
    int duplexHold_ = 0;
    float speakerLow_ = 0.0f;
    float speakerBand_ = 0.0f;
    float previousSignal_ = 0.0f;
    float codecPreviousInput_ = 0.0f;
    float codecPreviousOutput_ = 0.0f;
    float echoToneState_ = 0.0f;
    float distanceLowpass_ = 0.0f;
    float tonePhaseA_ = 0.0f;
    float tonePhaseB_ = 0.0f;
    float warblePhase_ = 0.0f;
    float outputPeak_ = 0.0f;
};
}
