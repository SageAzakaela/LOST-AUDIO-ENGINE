#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace lost_audio::core
{
struct TapeParameters
{
    float speed = 1.0f;
    float wowDepthMs = 3.5f;
    float flutterDepthMs = 1.2f;
    float wowAmount = 0.25f;
    float drive = 0.35f;
    float compression = 0.28f;
    float hiss = 0.12f;
    float hum = 0.05f;
    float dropout = 0.18f;
    float dropoutMs = 38.0f;
    float ceiling = 0.92f;
    float outputGain = 0.98f;
};

struct TapeMacroTargets
{
    float highPassHz = 35.0f;
    float lowPassHz = 11000.0f;
    float headBumpDb = 2.2f;
    float headBumpHz = 85.0f;
    float speed = 1.0f;
    float wowDepthMs = 3.5f;
    float flutterDepthMs = 1.2f;
    float drive = 0.35f;
    float compression = 0.28f;
    float hiss = 0.12f;
    float hum = 0.05f;
    float dropout = 0.18f;
    float dropoutMs = 38.0f;
    float ceiling = 0.92f;
    float outputGain = 0.98f;
};

[[nodiscard]] TapeMacroTargets mapTapeMacros(float quality, float age, float wow, float glitch) noexcept;

class TapeProcessor
{
public:
    static constexpr std::size_t maxChannels = 2;
    static constexpr double latencySeconds = 0.012;

    void prepare(double sampleRate, std::size_t channelCount);
    void reset(std::uint32_t seed = 0x74617065u) noexcept;
    void process(float* const* channels, std::size_t channelCount, std::size_t sampleCount, const TapeParameters& parameters) noexcept;

    [[nodiscard]] int latencySamples() const noexcept;
    [[nodiscard]] double sampleRate() const noexcept { return sampleRate_; }

private:
    struct ChannelState
    {
        std::vector<float> delay;
        std::size_t writeIndex = 0;
        float wowPhase = 0.0f;
        float flutterPhase = 0.0f;
        float drift = 0.0f;
        float envelope = 0.0f;
        float limiterEnvelope = 0.0f;
        float humPhase = 0.0f;
        float hissPrevious = 0.0f;
        std::uint32_t randomState = 0x74617065u;
        int dropoutRemaining = 0;
        int dropoutBlock = 0;
        float dropoutGain = 1.0f;
        bool dropoutInitialized = false;
    };

    [[nodiscard]] static std::uint32_t nextU32(std::uint32_t& state) noexcept;
    [[nodiscard]] static float nextFloat(std::uint32_t& state) noexcept;
    [[nodiscard]] static float nextSigned(std::uint32_t& state) noexcept;
    [[nodiscard]] static float readDelay(const ChannelState& state, float delaySamples) noexcept;

    std::array<ChannelState, maxChannels> channels_ {};
    double sampleRate_ = 48000.0;
    std::size_t preparedChannels_ = 0;
    std::uint32_t seed_ = 0x74617065u;
};
}
