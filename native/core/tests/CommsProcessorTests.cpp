#include <lost_audio/core/CommsProcessor.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

namespace
{
using lost_audio::core::CommsMode;
using lost_audio::core::CommsParameters;
using lost_audio::core::CommsProcessor;

bool require(bool condition, const char* message)
{
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}

std::vector<float> render(std::size_t blockSize, std::uint32_t seed, const CommsParameters& parameters)
{
    constexpr double sampleRate = 48000.0;
    constexpr std::size_t sampleCount = 144000;
    CommsProcessor processor;
    processor.prepare(sampleRate, 1);
    processor.reset(seed);
    std::vector<float> output(sampleCount, 0.0f);
    for (std::size_t i = 0; i < sampleCount; ++i)
    {
        const auto t = (float) i / (float) sampleRate;
        output[i] = 0.16f * std::sin(2.0f * 3.14159265358979323846f * 190.0f * t)
                  + 0.10f * std::sin(2.0f * 3.14159265358979323846f * 1860.0f * t);
        if (i % 9000 < 16) output[i] += 0.32f * (1.0f - (float) (i % 9000) / 16.0f);
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
    CommsParameters drySettings;
    drySettings.mix = 0.0f;
    const auto dry = render(257, 0x12345678u, drySettings);
    std::vector<float> reference(dry.size(), 0.0f);
    for (std::size_t i = 0; i < reference.size(); ++i)
    {
        const auto t = (float) i / 48000.0f;
        reference[i] = 0.16f * std::sin(2.0f * 3.14159265358979323846f * 190.0f * t)
                     + 0.10f * std::sin(2.0f * 3.14159265358979323846f * 1860.0f * t);
        if (i % 9000 < 16) reference[i] += 0.32f * (1.0f - (float) (i % 9000) / 16.0f);
    }
    ok &= require(difference(dry, reference) < 1.0e-7f, "zero mix must be a transparent dry path");

    CommsParameters cellular;
    cellular.mode = CommsMode::cellular;
    cellular.highPassHz = 480.0f;
    cellular.lowPassHz = 3100.0f;
    cellular.drive = 0.48f;
    cellular.compression = 0.72f;
    cellular.bits = 7;
    cellular.converterRateHz = 9000.0f;
    cellular.packetLoss = 0.62f;
    cellular.packetLengthMs = 74.0f;
    cellular.hiss = 0.24f;
    cellular.lineAge = 0.18f;
    cellular.duplex = 0.48f;
    cellular.transducer = 0.28f;
    cellular.distance = 0.08f;
    cellular.roomMix = 0.04f;
    cellular.echoMix = 0.03f;
    const auto blockA = render(64, 0x5eed1234u, cellular);
    const auto blockB = render(997, 0x5eed1234u, cellular);
    const auto otherSeed = render(64, 0x5eed5678u, cellular);
    ok &= require(blockA == blockB, "Comms output must be independent of host block size");
    ok &= require(difference(blockA, otherSeed) > 1.0e-5f, "different seeds must change stochastic line behavior");
    ok &= require(std::all_of(blockA.begin(), blockA.end(), [](float value)
    {
        return std::isfinite(value) && std::abs(value) <= 1.00001f;
    }), "Comms output must remain finite and bounded");

    const auto landline = lost_audio::core::mapCommsMacros(CommsMode::landline, 0.42f, 0.5f, 0.2f, 0.3f, 0.7f, 0.1f);
    const auto cell = lost_audio::core::mapCommsMacros(CommsMode::cellular, 0.42f, 0.5f, 0.8f, 0.3f, 0.2f, 0.1f);
    const auto intercom = lost_audio::core::mapCommsMacros(CommsMode::intercom, 0.42f, 0.5f, 0.2f, 0.3f, 0.8f, 0.3f);
    const auto pa = lost_audio::core::mapCommsMacros(CommsMode::publicAddress, 0.42f, 0.5f, 0.2f, 0.3f, 0.8f, 0.6f);
    ok &= require(cell.packetLoss > landline.packetLoss, "cellular mode must own stronger packet failure behavior");
    ok &= require(intercom.duplex > landline.duplex, "intercom mode must own stronger half-duplex gating");
    ok &= require(pa.lowPassHz > intercom.lowPassHz, "PA mode must retain a wider horn bandwidth than intercom");
    ok &= require(pa.distance > landline.distance, "distance macro must pass through the protected model");

    CommsParameters landlineSound;
    landlineSound.mode = CommsMode::landline;
    landlineSound.lineAge = 0.75f;
    landlineSound.transducer = 0.75f;
    landlineSound.drive = 0.42f;
    landlineSound.roomMix = 0.03f;
    CommsParameters paSound = landlineSound;
    paSound.mode = CommsMode::publicAddress;
    paSound.highPassHz = 140.0f;
    paSound.lowPassHz = 7800.0f;
    paSound.speakerRattle = 0.88f;
    paSound.distance = 0.6f;
    paSound.roomMix = 0.3f;
    paSound.roomMs = 760.0f;
    const auto landlineRender = render(128, 9u, landlineSound);
    const auto paRender = render(128, 9u, paSound);
    ok &= require(difference(landlineRender, paRender) > 0.02f,
                  "landline and PA modes must produce materially distinct hardware paths");

    if (!ok) return 1;
    std::cout << "Comms core passed: transparent dry mix, block-invariant seeded failures, bounded output, distinct hardware modes\n";
    return 0;
}
