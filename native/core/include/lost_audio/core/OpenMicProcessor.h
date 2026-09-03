#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace lost_audio::core
{
enum class OpenMicModel : int
{
    dynamicHandheld = 0,
    vocalCondenser,
    karaoke,
    podium,
    vintageRibbon
};

enum class OpenMicVenue : int
{
    cornerClub = 0,
    diveBar,
    rehearsalRoom,
    warehouse,
    rooftop,
    communityHall
};

enum class OpenMicPA : int
{
    compact = 0,
    column,
    horn,
    tiredCombo,
    blownStack
};

enum class OpenMicCrowdEvent : int { cheer = 0, applause, chatter, heckle };

struct OpenMicParameters
{
    OpenMicModel mic = OpenMicModel::dynamicHandheld;
    OpenMicVenue venue = OpenMicVenue::cornerClub;
    OpenMicPA pa = OpenMicPA::compact;

    float proximity = 0.45f;
    float micDrive = 0.24f;
    float paDrive = 0.20f;
    float monitorLevel = 0.42f;
    float stageBleed = 0.10f;
    float crowdLevel = 0.0f;
    float crowdMood = 0.45f;
    float electricalNoise = 0.015f;
    bool venueBedEnabled = true;
    float venueBedLevel = 0.42f;

    bool feedbackArmed = false;
    float feedbackAmount = 0.45f;
    float feedbackFrequency = 1800.0f;
    float feedbackQ = 16.0f;
    float feedbackDelayMs = 24.0f;
    float feedbackTone = 0.55f;
    float feedbackBuildMs = 420.0f;
    float feedbackReleaseMs = 180.0f;

    float roomAmount = 0.55f;
    float wallAbsorption = 0.28f;
    float stereoWidth = 0.82f;

    float inputGain = 1.0f;
    float mix = 1.0f;
    float limiterAmount = 0.72f;
    float ceiling = 0.90f;
    float outputGain = 0.95f;
};

struct OpenMicMacroTargets
{
    float proximity = 0.45f;
    float micDrive = 0.24f;
    float paDrive = 0.20f;
    float monitorLevel = 0.42f;
    float stageBleed = 0.10f;
    float roomAmount = 0.55f;
    float wallAbsorption = 0.28f;
    float electricalNoise = 0.015f;
};

[[nodiscard]] OpenMicMacroTargets mapOpenMicMacros(OpenMicModel mic,
                                                   OpenMicVenue venue,
                                                   OpenMicPA pa,
                                                   float hotMic,
                                                   float distance,
                                                   float room) noexcept;

class OpenMicProcessor
{
public:
    static constexpr std::size_t maxChannels = 2;

    void prepare(double sampleRate, std::size_t channels);
    void reset(std::uint32_t seed = 0x4f4d4e54u) noexcept;
    void triggerCrowdEvent(OpenMicCrowdEvent type, float strength, int durationSamples) noexcept;
    void process(float* const* channels,
                 std::size_t numChannels,
                 std::size_t numSamples,
                 const OpenMicParameters& parameters,
                 const float* const* externalAudienceAudio = nullptr) noexcept;

    [[nodiscard]] float inputPeak(std::size_t channel) const noexcept;
    [[nodiscard]] float outputPeak(std::size_t channel) const noexcept;
    [[nodiscard]] float feedbackActivity() const noexcept { return feedbackActivity_; }
    [[nodiscard]] float crowdActivity() const noexcept { return crowdActivity_; }
    [[nodiscard]] float roomActivity() const noexcept { return roomActivity_; }
    [[nodiscard]] float limiterActivity() const noexcept { return limiterActivity_; }
    [[nodiscard]] bool crowdEventActive() const noexcept { return crowdEventRemaining_ > 0; }
    [[nodiscard]] float crowdEventProgress() const noexcept { return crowdEventTotal_ > 0 ? (float)crowdEventRemaining_/(float)crowdEventTotal_ : 0.0f; }
    [[nodiscard]] bool safetyEngaged() const noexcept { return safetyEngaged_; }

private:
    struct Biquad
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
        float z1 = 0.0f, z2 = 0.0f;
        void reset() noexcept { z1 = z2 = 0.0f; }
        float process(float x) noexcept;
        void setHighPass(double sampleRate, float hz, float q) noexcept;
        void setLowPass(double sampleRate, float hz, float q) noexcept;
        void setPeak(double sampleRate, float hz, float q, float db) noexcept;
        void setBandPass(double sampleRate, float hz, float q) noexcept;
    };

    struct Channel
    {
        Biquad micHp, micPresence, micAir;
        Biquad paHp, paBody, paHorn, paLp;
        Biquad feedbackBand, feedbackTone;
        Biquad crowdBand, crowdAir;
        std::vector<float> feedbackDelay;
        std::vector<float> roomDelay;
        std::size_t feedbackWrite = 0;
        std::size_t roomWrite = 0;
        float crowdEnvelope = 0.0f;
        float crowdTarget = 0.0f;
        float crowdTransient = 0.0f;
        float crowdVoicePhase = 0.0f;
        int crowdCountdown = 0;
        float humPhase = 0.0f;
        float limiterEnvelope = 0.0f;
    };

    static std::uint32_t nextRandom(std::uint32_t& state) noexcept;
    static float randomSigned(std::uint32_t& state) noexcept;
    static float clamp(float value, float lo, float hi) noexcept;
    static float softClip(float value) noexcept;
    float readDelay(const std::vector<float>& delay, std::size_t write, float samples) const noexcept;
    void updateFilters(const OpenMicParameters& parameters) noexcept;

    std::array<Channel, maxChannels> channels_ {};
    std::array<std::uint32_t, maxChannels> random_ { 1u, 2u };
    std::array<float, maxChannels> inputPeak_ {};
    std::array<float, maxChannels> outputPeak_ {};
    double sampleRate_ = 48000.0;
    std::size_t preparedChannels_ = 0;
    float feedbackEnvelope_ = 0.0f;
    float feedbackActivity_ = 0.0f;
    float crowdActivity_ = 0.0f, roomActivity_ = 0.0f, limiterActivity_ = 0.0f;
    int crowdEventRemaining_ = 0, crowdEventTotal_ = 1;
    float crowdEventStrength_ = 0.0f;
    OpenMicCrowdEvent crowdEventType_ = OpenMicCrowdEvent::cheer;
    bool safetyEngaged_ = false;
};
} // namespace lost_audio::core
