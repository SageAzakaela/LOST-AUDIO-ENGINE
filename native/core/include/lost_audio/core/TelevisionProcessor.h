#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace lost_audio::core
{
enum class TelevisionModel : int { portable = 0, console, broadcastMonitor, kitchen, motel };
enum class TelevisionReception : int { baseband = 0, antenna, cable, detuned };

struct TelevisionParameters
{
    TelevisionModel model = TelevisionModel::console;
    TelevisionReception reception = TelevisionReception::antenna;
    float highPassHz = 70.0f, lowPassHz = 9000.0f, midHumpDb = 1.2f, midFrequencyHz = 1800.0f;
    float drive = .45f, compression = .22f, staticAmount = .12f, noiseHiss = .55f, noiseCrackle = .08f;
    float hum = .18f, whine = .08f, tunerDrift = .08f, syncInstability = .04f, powerSag = .08f;
    float cabinet = .55f, cabinetRattle = .06f;
    float inputGain = 1.0f, mix = 1.0f, limiter = .45f, ceiling = .94f, outputGain = 1.0f, auxiliaryGain = 1.0f;
};

struct TelevisionMacroTargets
{
    float highPassHz = 70.0f, lowPassHz = 9000.0f, midHumpDb = 1.2f, midFrequencyHz = 1800.0f;
    float drive = .45f, compression = .22f, noiseHiss = .55f, noiseCrackle = .08f;
    float tunerDrift = .08f, syncInstability = .04f, powerSag = .08f, cabinet = .55f, cabinetRattle = .06f;
    float limiter = .45f, ceiling = .94f, outputGain = 1.0f;
};

[[nodiscard]] TelevisionMacroTargets mapTelevisionMacros(TelevisionModel, TelevisionReception, float vibe, float speaker, float agc, float staticAmount) noexcept;

class TelevisionProcessor
{
public:
    static constexpr std::size_t maxChannels = 2;
    void prepare(double sampleRate, std::size_t channelCount);
    void reset(std::uint32_t seed = 0xdecafbadu) noexcept;
    void triggerSyncFault(float strength = 1.0f, float durationSeconds = 0.0f) noexcept;
    void process(float* const* channels, std::size_t channelCount, std::size_t sampleCount, const TelevisionParameters&, const float* auxiliaryMono = nullptr) noexcept;
    [[nodiscard]] float inputPeak(std::size_t channel) const noexcept { return inputPeak_[channel < 2 ? channel : 0]; }
    [[nodiscard]] float outputPeak(std::size_t channel) const noexcept { return outputPeak_[channel < 2 ? channel : 0]; }
    [[nodiscard]] bool staticActive() const noexcept { return staticEnvelope_ > .02f; }
    [[nodiscard]] bool syncFaultActive() const noexcept { return syncRemaining_ > 0; }
    [[nodiscard]] float staticLevel() const noexcept { return staticEnvelope_; }
    [[nodiscard]] float electricalLevel() const noexcept { return electricalEnvelope_; }
    [[nodiscard]] float rattleLevel() const noexcept { return rattleEnvelope_; }
    [[nodiscard]] float syncProgress() const noexcept;

private:
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void reset() noexcept { z1 = z2 = 0; }
        float process(float input) noexcept;
        void setHighPass(double, float, float) noexcept;
        void setLowPass(double, float, float) noexcept;
        void setPeak(double, float, float, float) noexcept;
    };
    struct ChannelState
    {
        std::array<Biquad, 6> filters {};
        float agcEnvelope = 0, agcGain = 1, noiseHigh = 0, previousNoise = 0, noiseLow = 0;
        float crackle = 0, rattle = 0, rattleVelocity = 0, limiterEnvelope = 0, driftLowPass = 0;
        int crackleRemaining = 0;
    };

    static std::uint32_t nextU32(std::uint32_t&) noexcept;
    static float nextFloat(std::uint32_t&) noexcept;
    static float nextSigned(std::uint32_t&) noexcept;
    void updateFilters(const TelevisionParameters&) noexcept;

    std::array<ChannelState, maxChannels> channel_ {};
    std::array<std::uint32_t, maxChannels> noiseRandom_ { 1, 2 }, textureRandom_ { 3, 4 };
    std::uint32_t eventRandom_ = 5;
    double sampleRate_ = 48000.0;
    std::size_t preparedChannels_ = 0;
    float humPhase_ = 0, flybackPhase_ = 0, flybackSubPhase_ = 0, driftPhase_ = 0;
    float staticEnvelope_ = 0, electricalEnvelope_ = 0, rattleEnvelope_ = 0, syncStrength_ = 1.0f;
    int syncCountdown_ = 0, syncRemaining_ = 0, syncLength_ = 1;
    std::atomic<float> pendingSyncStrength_ { 0.0f }, pendingSyncDurationSeconds_ { 0.0f };
    std::array<float, maxChannels> inputPeak_ {}, outputPeak_ {};
};
}
