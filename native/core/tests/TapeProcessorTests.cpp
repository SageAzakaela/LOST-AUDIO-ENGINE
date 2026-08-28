#include <lost_audio/core/TapeProcessor.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

namespace
{
using lost_audio::core::TapeParameters;
using lost_audio::core::TapeProcessor;

bool require(bool condition, const char* message)
{
    if (!condition)
        std::cerr << "FAIL: " << message << '\n';
    return condition;
}

std::vector<float> inputSignal(std::size_t count, double sampleRate)
{
    std::vector<float> signal(count, 0.0f);
    for (std::size_t index = 0; index < count; ++index)
    {
        const auto time = static_cast<double>(index) / sampleRate;
        signal[index] = static_cast<float>(0.28 * std::sin(2.0 * 3.141592653589793 * 83.0 * time)
                                         + 0.16 * std::sin(2.0 * 3.141592653589793 * 1187.0 * time));
    }
    return signal;
}

std::vector<float> render(std::size_t blockSize, std::uint32_t seed, const TapeParameters& parameters)
{
    constexpr double sampleRate = 48000.0;
    auto signal = inputSignal(48000, sampleRate);
    TapeProcessor processor;
    processor.prepare(sampleRate, 1);
    processor.reset(seed);
    for (std::size_t offset = 0; offset < signal.size(); offset += blockSize)
    {
        const auto count = std::min(blockSize, signal.size() - offset);
        auto* channel = signal.data() + offset;
        processor.process(&channel, 1, count, parameters);
    }
    return signal;
}
}

int main()
{
    bool ok = true;

    TapeParameters neutral;
    neutral.wowDepthMs = 0.0f;
    neutral.flutterDepthMs = 0.0f;
    neutral.wowAmount = 0.0f;
    neutral.drive = 0.0f;
    neutral.compression = 0.0f;
    neutral.hiss = 0.0f;
    neutral.hum = 0.0f;
    neutral.dropout = 0.0f;
    neutral.ceiling = 1.0f;
    neutral.outputGain = 1.0f;

    std::vector<float> impulse(2048, 0.0f);
    impulse[0] = 0.5f;
    TapeProcessor latencyProcessor;
    latencyProcessor.prepare(48000.0, 1);
    latencyProcessor.reset(0x74617065u);
    auto* impulseChannel = impulse.data();
    latencyProcessor.process(&impulseChannel, 1, impulse.size(), neutral);
    const auto arrival = static_cast<int>(std::distance(impulse.begin(), std::find_if(impulse.begin(), impulse.end(), [](float value) { return std::abs(value) > 1.0e-6f; })));
    ok &= require(latencyProcessor.latencySamples() == 576, "48 kHz latency must be 576 samples");
    ok &= require(arrival == latencyProcessor.latencySamples(), "neutral impulse must arrive at reported latency");

    TapeParameters damaged;
    damaged.wowAmount = 0.81f;
    damaged.wowDepthMs = 11.0f;
    damaged.flutterDepthMs = 4.0f;
    damaged.drive = 0.76f;
    damaged.compression = 0.54f;
    damaged.hiss = 0.3f;
    damaged.hum = 0.12f;
    damaged.dropout = 0.48f;
    damaged.dropoutMs = 73.0f;
    damaged.ceiling = 0.86f;
    damaged.outputGain = 1.08f;

    const auto first = render(64, 0x51a7eu, damaged);
    const auto repeat = render(64, 0x51a7eu, damaged);
    const auto otherSeed = render(64, 0x51a7fu, damaged);
    const auto otherBlocks = render(257, 0x51a7eu, damaged);
    ok &= require(first == repeat, "same seed and state must render identically");
    ok &= require(first == otherBlocks, "render must be invariant to host block size");
    ok &= require(first != otherSeed, "different seeds must alter stochastic tape behavior");

    float peak = 0.0f;
    for (const auto sample : first)
    {
        ok &= require(std::isfinite(sample), "processed samples must remain finite");
        peak = std::max(peak, std::abs(sample));
    }
    ok &= require(peak <= damaged.ceiling + 1.0e-6f, "portable core must enforce its output ceiling");

    const auto subtle = lost_audio::core::mapTapeMacros(0.72f, 0.18f, 0.14f, 0.06f);
    const auto destroyed = lost_audio::core::mapTapeMacros(0.08f, 0.82f, 0.92f, 0.62f);
    ok &= require(subtle.lowPassHz > destroyed.lowPassHz, "quality macro must change bandwidth");
    ok &= require(subtle.drive < destroyed.drive, "age macro must change saturation");
    ok &= require(subtle.wowDepthMs < destroyed.wowDepthMs, "wow macro must change modulation depth");
    ok &= require(subtle.dropout < destroyed.dropout, "glitch macro must change dropout severity");

    if (!ok)
        return 1;
    std::cout << "Tape core passed: latency=" << latencyProcessor.latencySamples()
              << " peak=" << peak << " deterministic=1 blockInvariant=1\n";
    return 0;
}
