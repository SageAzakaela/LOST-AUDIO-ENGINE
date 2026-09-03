#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace lost_audio::core
{
enum class CartridgeCodec : int { pcm = 0, dpcm, adpcm, brr, muLaw };
enum class CartridgeSpeaker : int { direct = 0, handheld, television, cabinet, pcSpeaker };
enum class BleepWave : int { alternate = 0, pulse, saw, triangle, noise };
enum class BleepTrigger : int { transient = 0, clock, hybrid };
enum class BleepScale : int { pentatonic = 0, minor, major, chromatic };

struct CartridgeParameters
{
    CartridgeCodec codecMode = CartridgeCodec::adpcm;
    CartridgeSpeaker speakerModel = CartridgeSpeaker::handheld;
    int bits = 10;
    float converterRateHz = 24000, jitter = .05f;
    bool dither = true, noiseShaping = false;
    float lowPassHz = 9000, highPassHz = 70, preEmphasis = .2f, companding = .25f, blockMs = 8;
    float saturation = .25f, edge = .25f, dcDrift = .15f, hum = .08f, whine = .15f, noise = .15f, noiseTracking = .6f;
    float speaker = .45f, microDelayMs = 8, microDelayMix = .06f, room = .08f, roomMs = 45;
    float addressWear = 0.0f, busDepth = 0.35f;
    bool bleepsEnabled = false;
    float bleepMix = .12f, bleepRate = 3, bleepVibrato = .12f, bleepPitch = .55f;
    BleepWave bleepWave = BleepWave::pulse; BleepTrigger bleepTrigger = BleepTrigger::transient; BleepScale bleepScale = BleepScale::minor;
    float limiter = .35f, ceiling = .92f, mix = 1, inputGain = 1, outputGain = .95f;
};

struct CartridgeMacroTargets
{
    int bits = 10; float converterRateHz = 24000, jitter = .05f, lowPassHz = 9000, highPassHz = 70, preEmphasis = .2f, companding = .25f, blockMs = 8;
    float saturation = .25f, edge = .25f, dcDrift = .15f, hum = .08f, whine = .15f, noise = .15f, noiseTracking = .6f;
    float speaker = .45f, microDelayMs = 8, microDelayMix = .06f, room = .08f, roomMs = 45, limiter = .35f, ceiling = .92f, outputGain = .95f;
};
[[nodiscard]] CartridgeMacroTargets mapCartridgeMacros(CartridgeCodec, CartridgeSpeaker, float quality, float codec, float grit, float noise) noexcept;

class CartridgeProcessor
{
public:
    static constexpr std::size_t maxChannels = 2;
    void prepare(double sampleRate, std::size_t channelCount);
    void reset(std::uint32_t seed = 0xfeedc0deu) noexcept;
    void setBleepClockSamples(int samplesUntilNext) noexcept;
    void triggerBleep() noexcept;
    void triggerRomStall(float strength, int durationSamples, int repeatSamples) noexcept;
    void triggerBankFault(float strength, int durationSamples) noexcept;
    void process(float* const* channels, std::size_t channelCount, std::size_t sampleCount, const CartridgeParameters&) noexcept;
    [[nodiscard]] float inputPeak(std::size_t ch) const noexcept { return inputPeak_[ch < 2 ? ch : 0]; }
    [[nodiscard]] float outputPeak(std::size_t ch) const noexcept { return outputPeak_[ch < 2 ? ch : 0]; }
    [[nodiscard]] bool bleepActive() const noexcept { return bleepRemaining_ > 0; }
    [[nodiscard]] bool stallActive() const noexcept { return stallRemaining_ > 0; }
    [[nodiscard]] bool bankFaultActive() const noexcept { return bankRemaining_ > 0; }
    [[nodiscard]] float stallProgress() const noexcept { return stallTotal_ > 0 ? (float)stallRemaining_ / (float)stallTotal_ : 0.0f; }
    [[nodiscard]] float bankFaultProgress() const noexcept { return bankTotal_ > 0 ? (float)bankRemaining_ / (float)bankTotal_ : 0.0f; }

private:
    struct Biquad { float b0=1,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0; void reset() noexcept {z1=z2=0;} float process(float) noexcept; void setHighPass(double,float,float) noexcept; void setLowPass(double,float,float) noexcept; void setPeak(double,float,float,float) noexcept; };
    struct ChannelState
    {
        std::array<Biquad, 6> filters {};
        std::vector<float> delay, roomA, roomB;
        std::vector<float> romHistory;
        std::size_t delayIndex=0, roomAIndex=0, roomBIndex=0;
        std::size_t romWriteIndex=0, stallReadIndex=0;
        float preZ=0, deZ=0, previous1=0, previous2=0, adpcmStep=.02f, noiseError=0, hold=0, dc=0, envelope=0, limiterEnvelope=0, roomLpA=0, roomLpB=0;
        int holdCount=0, blockRemaining=0;
    };
    void updateFilters(const CartridgeParameters&) noexcept;
    float read(const std::vector<float>&, std::size_t, float) const noexcept;
    static std::uint32_t nextU32(std::uint32_t&) noexcept; static float nextFloat(std::uint32_t&) noexcept; static float nextSigned(std::uint32_t&) noexcept;
    float codecSample(ChannelState&, float, const CartridgeParameters&, std::size_t channel) noexcept;
    float bleepSample(float sourceMagnitude, const CartridgeParameters&) noexcept;

    std::array<ChannelState, maxChannels> channel_ {};
    double sampleRate_ = 48000; std::size_t preparedChannels_ = 0; std::uint32_t seed_ = 0xfeedc0deu;
    std::array<std::uint32_t, maxChannels> codecRandom_ {1,2}, textureRandom_ {3,4}; std::uint32_t bleepRandom_ = 5;
    float humPhase_=0, whinePhase_=0, inputEnvelope_=0;
    int bleepRemaining_=0, bleepTotal_=1, bleepCooldown_=0, bleepClock_=0, bleepStep_=0; float bleepPhase_=0, bleepVibratoPhase_=0, bleepFrequency_=440, bleepAmplitude_=0, bleepDuty_=.25f;
    bool manualBleep_ = false;
    int stallRemaining_=0, stallTotal_=1, stallRepeatSamples_=1; float stallStrength_=0;
    int bankRemaining_=0, bankTotal_=1, bankSegment_=0; float bankStrength_=0;
    BleepWave activeWave_ = BleepWave::pulse;
    std::array<float, maxChannels> inputPeak_ {}, outputPeak_ {};
};
}
