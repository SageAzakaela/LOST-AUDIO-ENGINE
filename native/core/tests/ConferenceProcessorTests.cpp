#include <lost_audio/core/ConferenceProcessor.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

namespace
{
using lost_audio::core::ConferenceMode;
using lost_audio::core::ConferenceParameters;
using lost_audio::core::ConferenceProcessor;

bool require(bool condition, const char* message)
{
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}

struct Render { std::vector<float> left, right; int latency = 0; };

Render render(std::size_t blockSize, std::uint32_t seed, const ConferenceParameters& parameters)
{
    constexpr double sampleRate = 48000.0;
    constexpr std::size_t sampleCount = 96000;
    ConferenceProcessor processor; processor.prepare(sampleRate, 2); processor.reset(seed);
    Render result { std::vector<float>(sampleCount), std::vector<float>(sampleCount), processor.latencySamples() };
    for (std::size_t i = 0; i < sampleCount; ++i)
    {
        const auto t = (float) i / (float) sampleRate;
        result.left[i] = 0.18f * std::sin(2.0f * 3.14159265358979323846f * 211.0f * t)
                       + 0.08f * std::sin(2.0f * 3.14159265358979323846f * 1733.0f * t);
        result.right[i] = 0.13f * std::sin(2.0f * 3.14159265358979323846f * 347.0f * t)
                        + 0.07f * std::sin(2.0f * 3.14159265358979323846f * 2411.0f * t);
    }
    for (std::size_t offset = 0; offset < sampleCount; offset += blockSize)
    {
        const auto count = std::min(blockSize, sampleCount - offset);
        float* channels[] { result.left.data() + offset, result.right.data() + offset };
        processor.process(channels, 2, count, parameters);
    }
    return result;
}

float difference(const std::vector<float>& a, const std::vector<float>& b)
{
    float sum = 0.0f; for (std::size_t i = 0; i < a.size(); ++i) sum += std::abs(a[i] - b[i]); return sum / (float) a.size();
}

float largestStep(const std::vector<float>& samples, std::size_t begin)
{
    float result = 0.0f;
    for (std::size_t i = std::max<std::size_t>(begin, 1); i < samples.size(); ++i)
        result = std::max(result, std::abs(samples[i] - samples[i - 1]));
    return result;
}
}

int main()
{
    bool ok = true;
    ConferenceParameters drySettings; drySettings.mix = 0.0f;
    const auto dry = render(257, 0x12345678u, drySettings);
    ok &= require(dry.latency == 96, "Conference live path must remain at 2 ms transport latency at 48 kHz");
    float dryError = 0.0f;
    for (std::size_t i = (std::size_t) dry.latency; i < dry.left.size(); ++i)
    {
        const auto source = i - (std::size_t) dry.latency;
        const auto t = (float) source / 48000.0f;
        const auto reference = 0.18f * std::sin(2.0f * 3.14159265358979323846f * 211.0f * t)
                             + 0.08f * std::sin(2.0f * 3.14159265358979323846f * 1733.0f * t);
        dryError += std::abs(dry.left[i] - reference);
    }
    dryError /= (float) (dry.left.size() - (std::size_t) dry.latency);
    ok &= require(dryError < 1.0e-6f, "zero mix must be a latency-aligned transparent dry path");

    ConferenceParameters damaged;
    damaged.mode = ConferenceMode::cellular; damaged.packetLoss = 0.44f; damaged.packetMs = 28.0f;
    damaged.burstiness = 0.82f; damaged.jitterMs = 4.5f; damaged.jitterRate = 31.0f;
    damaged.bufferSlip = 0.56f; damaged.bandwidthSwitch = 0.42f; damaged.robot = 0.48f;
    damaged.bits = 7; damaged.converterRateHz = 9000.0f; damaged.noise = 0.22f;
    const auto blockA = render(64, 0x5eed1234u, damaged);
    const auto blockB = render(997, 0x5eed1234u, damaged);
    const auto otherSeed = render(64, 0x5eed5678u, damaged);
    ok &= require(blockA.left == blockB.left && blockA.right == blockB.right,
                  "Conference failures must be independent of host block size");
    ok &= require(difference(blockA.left, otherSeed.left) > 1.0e-4f,
                  "different seeds must change packet and buffer failures");
    ok &= require(std::all_of(blockA.left.begin(), blockA.left.end(), [](float value)
    {
        return std::isfinite(value) && std::abs(value) <= 1.00001f;
    }), "Conference output must remain finite and bounded");
    ok &= require(difference(blockA.left, blockA.right) > 0.01f,
                  "linked packet events must preserve independent stereo channel content");

    const auto cleanDiscord = lost_audio::core::mapConferenceMacros(ConferenceMode::discord, 0.85f, 0.08f, 0.02f, 0.03f, 0.01f, 0.02f);
    const auto badCell = lost_audio::core::mapConferenceMacros(ConferenceMode::cellular, 0.18f, 0.82f, 0.88f, 0.75f, 0.62f, 0.45f);
    ok &= require(badCell.packetLoss > cleanDiscord.packetLoss * 5.0f, "dropout macro must materially increase frame loss");
    ok &= require(badCell.lowPassHz < cleanDiscord.lowPassHz, "cellular damage must collapse bandwidth");
    ok &= require(badCell.bits < cleanDiscord.bits, "codec macro must lower codec resolution");

    ConferenceParameters clean; clean.mode = ConferenceMode::discord; clean.packetLoss = 0.0f; clean.robot = 0.0f; clean.noise = 0.0f;
    clean.jitterMs = 0.0f; clean.bufferSlip = 0.0f; clean.bandwidthSwitch = 0.0f; clean.bits = 16; clean.converterRateHz = 48000.0f;
    const auto cleanRender = render(128, 7u, clean);
    const auto failedRender = render(128, 7u, damaged);
    ok &= require(difference(cleanRender.left, failedRender.left) > 0.025f,
                  "healthy and failing conference paths must be materially distinct");
    const auto musicalDefault = render(128, 0x636f6e66u, ConferenceParameters {});
    ok &= require(largestStep(musicalDefault.left, 256) < 0.12f,
                  "Conference default must not create click-sized discontinuities");

    ConferenceProcessor conducted; conducted.prepare(48000.0, 2); conducted.reset(99u);
    ConferenceParameters conductedParameters; conductedParameters.packetLoss = 0.0f; conductedParameters.robot = 0.0f;
    conductedParameters.noise = 0.0f; conductedParameters.jitterMs = 0.0f;
    std::vector<float> conductedLeft(512, 0.2f), conductedRight(512, -0.15f);
    float* conductedChannels[] { conductedLeft.data(), conductedRight.data() };
    conducted.triggerPacketLoss(0.8f, 0.1f); conducted.triggerRobot(0.7f, 0.15f, 22.0f);
    conducted.process(conductedChannels, 2, conductedLeft.size(), conductedParameters);
    ok &= require(conducted.packetLost() && conducted.packetLossProgress() > 0.0f,
                  "manual packet failure must start immediately and report real progress");
    ok &= require(conducted.robotActive() && conducted.robotProgress() > 0.0f,
                  "manual robot capture must start immediately and report real progress");
    for (int block = 0; block < 24; ++block)
    {
        std::fill(conductedLeft.begin(), conductedLeft.end(), 0.1f); std::fill(conductedRight.begin(), conductedRight.end(), -0.1f);
        conducted.process(conductedChannels, 2, conductedLeft.size(), conductedParameters);
    }
    ok &= require(!conducted.packetLost() && !conducted.robotActive(),
                  "conducted packet and robot events must recover without hidden queues");

    if (!ok) return 1;
    std::cout << "Conference core passed: fixed latency, deterministic failures, conducted events, stereo preservation, bounded output\n";
    return 0;
}
