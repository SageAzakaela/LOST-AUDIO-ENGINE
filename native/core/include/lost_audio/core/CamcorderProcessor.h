#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace lost_audio::core
{
enum class CamcorderFormat : int { vhsc = 0, video8, miniDV, digicam, actionCam };
enum class CameraMic : int { electret = 0, cheapMono, stereoCapsule, waterproof, shotgun };
enum class CameraConcealment : int { hold = 0, mute, interpolate, repeat };

struct CamcorderParameters
{
    CamcorderFormat format = CamcorderFormat::miniDV;
    CameraMic microphone = CameraMic::electret;
    CameraConcealment concealment = CameraConcealment::hold;
    float coverage = 0.35f, movement = 0.25f, corruption = 0.18f, agcDrive = 0.35f;
    bool windEnabled = false;
    float windLevel = 0.80f;
    float highPassHz = 55.0f, lowPassHz = 9200.0f, bodyDb = 3.2f, bodyHz = 1650.0f;
    float agcAmount = 0.55f, agcSpeed = 0.45f, agcPump = 0.45f, clip = 0.25f;
    float crush = 0.12f, flutter = 0.12f, flutterRateHz = 0.0f;
    int bits = 12;
    float converterRateHz = 24000.0f;
    float dropout = 0.18f, dropoutMs = 28.0f, repeatMs = 48.0f, chirp = 0.15f;
    float handling = 0.22f, rub = 0.18f, hiss = 0.12f, motorBleed = 0.08f;
    float inputGain = 1.0f, outputGain = 0.98f, mix = 1.0f, ceiling = 0.92f;
};

struct CamcorderMacroTargets
{
    float highPassHz = 55.0f, lowPassHz = 9200.0f, bodyDb = 3.2f, bodyHz = 1650.0f;
    float agcAmount = 0.55f, agcSpeed = 0.45f, agcPump = 0.45f, clip = 0.25f;
    float crush = 0.12f, flutter = 0.12f;
    int bits = 12;
    float converterRateHz = 24000.0f;
    float dropout = 0.18f, dropoutMs = 28.0f, repeatMs = 48.0f, chirp = 0.15f;
    float handling = 0.22f, rub = 0.18f, hiss = 0.12f, motorBleed = 0.08f;
    float outputGain = 0.98f, ceiling = 0.92f;
};

[[nodiscard]] CamcorderMacroTargets mapCamcorderMacros(
    CamcorderFormat format, CameraMic microphone, float coverage,
    float movement, float corruption, float agcDrive) noexcept;

class CamcorderProcessor
{
public:
    static constexpr std::size_t maxChannels = 2;
    void prepare(double sampleRate, std::size_t channelCount);
    void reset(std::uint32_t seed = 0x43414d45u) noexcept;
    void process(float* const* channels, std::size_t channelCount, std::size_t sampleCount,
                 const CamcorderParameters& parameters, const float* auxiliaryMono = nullptr) noexcept;
    void triggerDropout(float durationSeconds) noexcept;
    void triggerCodecFault(float strength, float durationSeconds) noexcept;
    void triggerHandling(float strength) noexcept;
    [[nodiscard]] int latencySamples() const noexcept { return latencySamples_; }
    [[nodiscard]] float inputPeak(std::size_t channel) const noexcept { return inputPeak_[channel < maxChannels ? channel : 0]; }
    [[nodiscard]] float outputPeak(std::size_t channel) const noexcept { return outputPeak_[channel < maxChannels ? channel : 0]; }
    [[nodiscard]] bool windActive() const noexcept { return windRemaining_ > 0; }
    [[nodiscard]] bool handlingActive() const noexcept { return thumpRemaining_ > 0 || scrapeRemaining_ > 0; }
    [[nodiscard]] bool dropoutActive() const noexcept { return dropoutRemaining_ > 0; }
    [[nodiscard]] bool corruptionActive() const noexcept { return chirpRemaining_ > 0; }
    [[nodiscard]] float dropoutProgress() const noexcept { return dropoutProgress_; }
    [[nodiscard]] float corruptionProgress() const noexcept { return corruptionProgress_; }
    [[nodiscard]] float handlingProgress() const noexcept { return handlingProgress_; }
    [[nodiscard]] float windProgress() const noexcept { return windProgress_; }
    [[nodiscard]] float agcActivity() const noexcept { return agcActivity_; }
    [[nodiscard]] float flutterActivity() const noexcept { return flutterActivity_; }
    [[nodiscard]] float limiterActivity() const noexcept { return limiterActivity_; }

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
        std::vector<float> history, dryHistory;
        std::size_t writeIndex = 0;
        std::array<Biquad, 5> tone {};
        float micLow = 0, micBand = 0, envelope = 0, agcGain = 1, muffle = 0;
        float held = 0, lastGood = 0, dropStart = 0, dropoutBlend = 0, limiter = 0;
        float rubLow = 0, rubBand = 0, hissPrevious = 0, windLow = 0, windMid = 0;
        int holdCount = 0;
    };

    void updateFilters(const CamcorderParameters&) noexcept;
    [[nodiscard]] float read(const std::vector<float>&, std::size_t, float) const noexcept;
    [[nodiscard]] static std::uint32_t nextU32(std::uint32_t&) noexcept;
    [[nodiscard]] static float nextFloat(std::uint32_t&) noexcept;
    [[nodiscard]] static float nextSigned(std::uint32_t&) noexcept;

    std::array<ChannelState, maxChannels> channel_ {};
    double sampleRate_ = 48000.0;
    std::size_t preparedChannels_ = 0;
    int latencySamples_ = 96;
    std::uint32_t seed_ = 0x43414d45u;
    std::uint32_t transportRandom_ = 1, corruptionRandom_ = 2, handlingRandom_ = 3, windRandom_ = 4;
    std::array<std::uint32_t, maxChannels> codecRandom_ { 5, 6 }, textureRandom_ { 7, 8 }, windNoiseRandom_ { 9, 10 };
    float flutterPhaseA_ = 0, flutterPhaseB_ = 0, flutterNoise_ = 0;
    int dropoutRemaining_ = 0, dropoutTotal_ = 1;
    int chirpRemaining_ = 0, chirpTotal_ = 1; float chirpPhase_ = 0, chirpStartHz_ = 900, chirpEndHz_ = 4200, chirpAmplitude_ = 0;
    int thumpRemaining_ = 0, thumpTotal_ = 1; float thumpPhase_ = 0, thumpHz_ = 55, thumpAmplitude_ = 0;
    int scrapeRemaining_ = 0, scrapeTotal_ = 1; float scrapeEnvelope_ = 0;
    int windRemaining_ = 0, windTotal_ = 1; float windPhase_ = 0, windAmplitude_ = 0;
    float motorPhaseA_ = 0, motorPhaseB_ = 0;
    float dropoutProgress_ = 0, corruptionProgress_ = 0, handlingProgress_ = 0, windProgress_ = 0;
    float agcActivity_ = 0, flutterActivity_ = 0, limiterActivity_ = 0;
    std::array<float, maxChannels> inputPeak_ {}, outputPeak_ {};
};
}
