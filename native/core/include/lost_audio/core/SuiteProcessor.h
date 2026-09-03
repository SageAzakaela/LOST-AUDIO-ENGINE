#pragma once

#include <lost_audio/core/CamcorderProcessor.h>
#include <lost_audio/core/CartridgeProcessor.h>
#include <lost_audio/core/CDProcessor.h>
#include <lost_audio/core/CommsProcessor.h>
#include <lost_audio/core/ConferenceProcessor.h>
#include <lost_audio/core/OcclusionProcessor.h>
#include <lost_audio/core/OpenMicProcessor.h>
#include <lost_audio/core/TapeProcessor.h>
#include <lost_audio/core/TelevisionProcessor.h>
#include <lost_audio/core/TransmissionProcessor.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace lost_audio::core
{
enum class SuiteEngine : int
{
    empty = 0,
    tape,
    transmission,
    comms,
    cd,
    conference,
    camcorder,
    cartridge,
    television,
    occlusion,
    openMicNight
};

struct SuiteSlotParameters
{
    SuiteEngine engine = SuiteEngine::empty;
    bool bypass = false;
    float mix = 1.0f;
    float macroA = 0.35f;
    float macroB = 0.20f;
    float model = 0.0f;
    // Six neutral-centred device controls. Their visible names and DSP meaning
    // follow the selected engine; 0.5 preserves the original macro mapping.
    std::array<float, 6> detail { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
    float global1ToA = 0.0f;
    float global1ToB = 0.0f;
    float global2ToA = 0.0f;
    float global2ToB = 0.0f;
    bool feedbackArmed = false;
};

struct SuiteParameters
{
    static constexpr std::size_t slotCount = 6;
    std::array<SuiteSlotParameters, slotCount> slots {};
    std::array<int, slotCount> order { 0, 1, 2, 3, 4, 5 };
    float globalMacro1 = 0.5f;
    float globalMacro2 = 0.5f;
    float inputGain = 1.0f;
    float outputGain = 1.0f;
    float mix = 1.0f;
    float limiter = 0.72f;
    float ceiling = 0.92f;
};

class SuiteProcessor
{
public:
    static constexpr std::size_t maxChannels = 2;
    static constexpr std::size_t slotCount = SuiteParameters::slotCount;

    void prepare(double sampleRate, std::size_t channels);
    void reset(std::uint32_t seed = 0x53554954u) noexcept;
    void process(float* const* channels, std::size_t channelCount,
                 std::size_t sampleCount, const SuiteParameters& parameters,
                 const float* televisionBed = nullptr) noexcept;

    [[nodiscard]] int latencySamples() const noexcept { return fixedLatencySamples_; }
    [[nodiscard]] float inputPeak(std::size_t channel) const noexcept;
    [[nodiscard]] float outputPeak(std::size_t channel) const noexcept;
    [[nodiscard]] float topologyActivity() const noexcept { return topologyGain_; }
    [[nodiscard]] bool safetyEngaged() const noexcept { return safetyEngaged_; }

private:
    static constexpr std::size_t chunkSize = 64;

    struct Delay
    {
        std::vector<float> samples;
        std::size_t write = 0;
        void prepare(std::size_t size);
        void reset() noexcept;
        float pushRead(float input, int delaySamples) noexcept;
    };

    struct SlotRuntime
    {
        TapeProcessor tape;
        TransmissionProcessor transmission;
        CommsProcessor comms;
        CDProcessor cd;
        ConferenceProcessor conference;
        CamcorderProcessor camcorder;
        CartridgeProcessor cartridge;
        TelevisionProcessor television;
        OcclusionProcessor occlusion;
        OpenMicProcessor openMic;
        std::array<Delay, maxChannels> dry;
    };

    struct Topology
    {
        std::array<SuiteEngine, slotCount> engines {};
        std::array<int, slotCount> order { 0, 1, 2, 3, 4, 5 };
        std::array<bool, slotCount> bypass {};
        bool operator==(const Topology&) const noexcept = default;
    };

    static float clamp(float value, float lo, float hi) noexcept;
    static int scaledIndex(float value, int maximum) noexcept;
    static Topology topologyFrom(const SuiteParameters&) noexcept;
    static std::array<int, slotCount> sanitizeOrder(const std::array<int, slotCount>&) noexcept;
    void prepareSlot(SlotRuntime&, std::size_t index);
    void resetSlot(SlotRuntime&, std::uint32_t seed) noexcept;
    int engineLatency(const SlotRuntime&, SuiteEngine) const noexcept;
    void processEngine(SlotRuntime&, SuiteEngine, float* const*, std::size_t,
                       std::size_t, const SuiteSlotParameters&, float, float,
                       const float*) noexcept;
    void processSlot(std::size_t slotIndex, SuiteEngine, float* const*, std::size_t,
                     std::size_t, const SuiteSlotParameters&, float, float,
                     const float*) noexcept;
    void beginTopologyChange(const Topology&) noexcept;
    void applyPendingTopology() noexcept;

    std::array<SlotRuntime, slotCount> slots_ {};
    std::array<Delay, maxChannels> masterDry_ {};
    std::array<Delay, maxChannels> wetPad_ {};
    std::array<std::array<float, chunkSize>, maxChannels> original_ {};
    std::array<std::array<float, chunkSize>, maxChannels> masterInput_ {};
    std::array<float, maxChannels> limiterEnvelope_ {};
    std::array<float, maxChannels> inputPeak_ {}, outputPeak_ {};
    std::array<bool, slotCount> feedbackEligible_ {};
    Topology activeTopology_ {}, pendingTopology_ {};
    bool topologyInitialized_ = false, topologyPending_ = false, fadingOut_ = false;
    float topologyGain_ = 1.0f;
    double sampleRate_ = 48000.0;
    std::size_t preparedChannels_ = 0;
    int fixedLatencySamples_ = 5760;
    std::uint32_t seed_ = 0x53554954u;
    bool safetyEngaged_ = false;
};
} // namespace lost_audio::core
