#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace lost_audio::core
{
enum class OcclusionMaterial : int { drywall = 0, brick, wood, curtain, door, glass, metal, concrete };
enum class OcclusionConstruction : int { solid = 0, stud, hollow, panel, loose };

struct OcclusionParameters
{
    OcclusionMaterial material = OcclusionMaterial::drywall;
    OcclusionConstruction construction = OcclusionConstruction::stud;
    float distance = .35f, wall = .45f;
    float hpHz = 64.0f, lpHz = 8285.0f, dipHz = 1528.0f, dipDb = -3.8f, dipQ = 1.1f;
    float bumpHz = 352.0f, bumpDb = 2.8f, bumpQ = .95f;
    float resonance = .613f, cavity = .443f, rattle = .060f, looseness = .34f, smear = .396f;
    float leak = .111f, leakTone = .564f;
    float sourceRoom = .35f, listenerRoom = .45f, roomMix = .413f, predelayMs = 12.0f;
    float roomSize = .412f, damp = .671f;
    float stereoMotion = 0.0f, motionPhase = 0.0f;
    float inputGain = 1.0f, mix = 1.0f, limiter = .4f, ceiling = .94f, outputGain = .9f;
};

struct OcclusionMacroTargets
{
    float hpHz = 58, lpHz = 5200, dipHz = 1550, dipDb = -2.4f, bumpHz = 350, bumpDb = 1.4f;
    float resonance = .48f, cavity = .52f, rattle = .08f, looseness = .34f, smear = .38f;
    float leak = .08f, leakTone = .52f, roomMix = .22f, predelayMs = 8, roomSize = .5f, damp = .63f, outputGain = 1;
};

[[nodiscard]] OcclusionMacroTargets mapOcclusionMacros(OcclusionMaterial, OcclusionConstruction,
                                                        float distance, float wall,
                                                        float sourceRoom, float listenerRoom) noexcept;

class OcclusionProcessor
{
public:
    static constexpr std::size_t maxChannels = 2;
    void prepare(double, std::size_t);
    void reset(std::uint32_t seed = 0x6f626675u) noexcept;
    void process(float* const*, std::size_t, std::size_t, const OcclusionParameters&) noexcept;
    void triggerBoundaryExcitation(float strength, int durationSamples) noexcept;
    void triggerRattleStrike(float strength) noexcept;

    float inputPeak(std::size_t c) const noexcept { return inputPeak_[c < 2 ? c : 0]; }
    float outputPeak(std::size_t c) const noexcept { return outputPeak_[c < 2 ? c : 0]; }
    float bodyActivity() const noexcept { return bodyActivity_; }
    float roomActivity() const noexcept { return roomActivity_; }
    float leakActivity() const noexcept { return leakActivity_; }
    float rattleActivity() const noexcept { return rattleEnvelope_; }
    float limiterActivity() const noexcept { return limiterActivity_; }
    bool rattleActive() const noexcept { return rattleEnvelope_ > .008f; }
    bool excitationActive() const noexcept { return excitationRemaining_ > 0; }
    float excitationProgress() const noexcept;

private:
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void reset() noexcept { z1 = z2 = 0; }
        float process(float) noexcept;
        void setHP(double, float, float) noexcept;
        void setLP(double, float, float) noexcept;
        void setPeak(double, float, float, float) noexcept;
        void setBP(double, float, float) noexcept;
    };
    struct Channel
    {
        std::array<Biquad, 12> f {};
        std::vector<float> delay, inputDelay;
        std::size_t write = 0, inputWrite = 0;
        float env = 0, prevAbs = 0, prev = 0, burst = 0, limEnv = 0, roomTone = 0, feedbackTone = 0;
        int cooldown = 0;
    };
    static std::uint32_t next(std::uint32_t&) noexcept;
    static float random(std::uint32_t&) noexcept;
    float read(const Channel&, float) const noexcept;
    float readInput(const Channel&, float) const noexcept;
    void filters(const OcclusionParameters&) noexcept;

    std::array<Channel, 2> ch_ {};
    std::array<std::uint32_t, 2> rng_ { 1, 2 };
    double sr_ = 48000;
    std::size_t channels_ = 0;
    std::array<float, 2> inputPeak_ {}, outputPeak_ {};
    float rattleEnvelope_ = 0, bodyActivity_ = 0, roomActivity_ = 0, leakActivity_ = 0, limiterActivity_ = 0;
    float excitationStrength_ = 0, strikeStrength_ = 0;
    int excitationRemaining_ = 0, excitationTotal_ = 1;
};
}
