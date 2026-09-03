#include <lost_audio/core/TransmissionProcessor.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

namespace
{
using lost_audio::core::TransmissionParameters;
using lost_audio::core::TransmissionProcessor;

bool require(bool condition, const char* message)
{
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}

std::vector<float> render(std::size_t blockSize, std::uint32_t seed, const TransmissionParameters& parameters)
{
    constexpr double sampleRate = 48000.0;
    constexpr std::size_t sampleCount = 144000;
    TransmissionProcessor processor;
    processor.prepare(sampleRate, 1);
    processor.reset(seed);
    std::vector<float> output(sampleCount, 0.0f);
    for (std::size_t i = 0; i < sampleCount; ++i)
    {
        const auto t = (float) i / (float) sampleRate;
        output[i] = 0.16f * std::sin(2.0f * 3.14159265358979323846f * 233.0f * t)
                  + 0.09f * std::sin(2.0f * 3.14159265358979323846f * 1703.0f * t);
        if (i % 12000 < 20) output[i] += 0.35f * (1.0f - (float) (i % 12000) / 20.0f);
    }
    for (std::size_t offset = 0; offset < sampleCount; offset += blockSize)
    {
        const auto count = std::min(blockSize, sampleCount - offset);
        float* channels[] { output.data() + offset };
        processor.process(channels, 1, count, parameters);
    }
    return output;
}

float difference(const std::vector<float>& a, const std::vector<float>& b)
{
    float sum = 0.0f;
    for (std::size_t i = 0; i < a.size(); ++i) sum += std::abs(a[i] - b[i]);
    return sum / (float) a.size();
}
}

int main()
{
    bool ok = true;
    TransmissionParameters neutral;
    neutral.mix = 0.0f;
    neutral.noise = neutral.hiss = neutral.crackle = neutral.dropoutRate = neutral.wowDepth = 0.0f;
    const auto dry = render(257, 0x12345678u, neutral);
    TransmissionProcessor latencyProbe;
    latencyProbe.prepare(48000.0, 1);
    ok &= require(latencyProbe.latencySamples() == 1728, "Transmission must report the fixed six-stage latency");
    ok &= require(std::all_of(dry.begin(), dry.begin() + latencyProbe.latencySamples(), [](float value) { return value == 0.0f; }),
                  "dry mix must be aligned to reported latency");

    TransmissionParameters damaged;
    damaged.highPassHz = 560.0f;
    damaged.lowPassHz = 2800.0f;
    damaged.midGainDb = 4.8f;
    damaged.midQ = 1.9f;
    damaged.drive = 0.65f;
    damaged.asymmetry = 0.3f;
    damaged.compression = 0.55f;
    damaged.crush = 0.28f;
    damaged.wowDepth = 0.62f;
    damaged.dropoutRate = 0.72f;
    damaged.dropoutDepth = 0.82f;
    damaged.crackle = 0.55f;
    damaged.noise = 0.42f;
    damaged.noiseColor = 0.9f;
    damaged.hiss = 0.58f;
    damaged.passes = 3;
    damaged.outputGain = 0.9f;
    const auto blockA = render(64, 0x5eed1234u, damaged);
    const auto blockB = render(997, 0x5eed1234u, damaged);
    const auto otherSeed = render(64, 0x5eed5678u, damaged);
    ok &= require(blockA == blockB, "Transmission output must be independent of host block size");
    ok &= require(difference(blockA, otherSeed) > 1.0e-4f, "different seeds must change stochastic damage");
    ok &= require(std::all_of(blockA.begin(), blockA.end(), [](float value)
    {
        return std::isfinite(value) && std::abs(value) <= 1.00001f;
    }), "Transmission output must remain finite and bounded");

    const auto subtle = lost_audio::core::mapTransmissionMacros(0.78f, 0.12f, 0.05f, 0.08f);
    const auto ruined = lost_audio::core::mapTransmissionMacros(0.12f, 0.78f, 0.86f, 0.72f);
    ok &= require(subtle.highPassHz < ruined.highPassHz && subtle.lowPassHz > ruined.lowPassHz,
                  "bandwidth macro must narrow both sides of the receiver band");
    ok &= require(subtle.compression < ruined.compression, "drive macro must change transmitter compression");
    ok &= require(subtle.dropoutRate < ruined.dropoutRate, "reception macro must change dropout rate");
    ok &= require(subtle.hiss < ruined.hiss, "noise macro must change hiss");

    TransmissionParameters clean;
    clean.highPassHz = 90.0f;
    clean.lowPassHz = 14500.0f;
    clean.drive = 0.02f;
    clean.compression = 0.05f;
    clean.noise = clean.hiss = clean.crackle = clean.dropoutRate = clean.wowDepth = 0.0f;
    const auto cleanRender = render(128, 9u, clean);
    ok &= require(difference(cleanRender, blockA) > 0.015f, "damaged settings must materially change the signal");

    TransmissionProcessor triggered;
    triggered.prepare(48000.0, 2);
    triggered.reset(0x5eed1234u);
    TransmissionParameters triggeredParameters = clean;
    triggeredParameters.mix = 1.0f;
    std::vector<float> left(12000, 0.25f), right(12000, -0.25f);
    float* linked[] { left.data(), right.data() };
    triggered.triggerDropout(0.8f, 0.05f);
    triggered.process(linked, 2, 1024, triggeredParameters);
    ok &= require(triggered.dropoutActive(), "manual carrier loss must become active");
    ok &= require(triggered.dropoutProgress() > 0.0f, "manual carrier loss must publish progress");
    triggered.process(linked, 2, 8000, triggeredParameters);
    ok &= require(!triggered.dropoutActive(), "manual carrier loss must recover within its requested duration");

    if (!ok) return 1;
    std::cout << "Transmission core passed: latency=" << latencyProbe.latencySamples()
              << " samples, block-invariant deterministic damage, bounded output, live macros\n";
    return 0;
}
