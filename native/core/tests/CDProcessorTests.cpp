#include <lost_audio/core/CDProcessor.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

namespace
{
using lost_audio::core::CDConcealment;
using lost_audio::core::CDDamageShape;
using lost_audio::core::CDParameters;
using lost_audio::core::CDProcessor;

constexpr double sampleRate = 48000.0;
constexpr std::size_t sampleCount = 144000;
constexpr float pi = 3.14159265358979323846f;

struct StereoRender
{
    std::vector<float> left;
    std::vector<float> right;
};

bool require(bool condition, const char* message)
{
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}

float inputSample(std::size_t index, bool right)
{
    const auto t = (float) index / (float) sampleRate;
    auto value = right
        ? 0.13f * std::sin(2.0f * pi * 1703.0f * t) + 0.07f * std::sin(2.0f * pi * 421.0f * t)
        : 0.17f * std::sin(2.0f * pi * 193.0f * t) + 0.08f * std::sin(2.0f * pi * 811.0f * t);
    if (index % 11000 < 18)
        value += (right ? -0.24f : 0.31f) * (1.0f - (float) (index % 11000) / 18.0f);
    return value;
}

StereoRender render(std::size_t blockSize, std::uint32_t seed, const CDParameters& parameters)
{
    CDProcessor processor;
    processor.prepare(sampleRate, 2);
    processor.reset(seed);
    StereoRender output { std::vector<float>(sampleCount), std::vector<float>(sampleCount) };
    for (std::size_t i = 0; i < sampleCount; ++i)
    {
        output.left[i] = inputSample(i, false);
        output.right[i] = inputSample(i, true);
    }
    for (std::size_t offset = 0; offset < sampleCount; offset += blockSize)
    {
        const auto count = std::min(blockSize, sampleCount - offset);
        float* channels[] { output.left.data() + offset, output.right.data() + offset };
        processor.process(channels, 2, count, parameters);
    }
    return output;
}

float difference(const std::vector<float>& a, const std::vector<float>& b)
{
    float sum = 0.0f;
    for (std::size_t i = 0; i < a.size(); ++i) sum += std::abs(a[i] - b[i]);
    return sum / (float) a.size();
}

float renderDifference(const StereoRender& a, const StereoRender& b)
{
    return 0.5f * (difference(a.left, b.left) + difference(a.right, b.right));
}
}

int main()
{
    bool ok = true;
    CDProcessor latencyProbe;
    latencyProbe.prepare(sampleRate, 2);
    ok &= require(latencyProbe.latencySamples() == 120, "CD must report its fixed 2.5 ms latency");

    CDParameters drySettings;
    drySettings.mix = 0.0f;
    const auto dry = render(257, 0x12345678u, drySettings);
    const auto latency = (std::size_t) latencyProbe.latencySamples();
    ok &= require(std::all_of(dry.left.begin(), dry.left.begin() + (std::ptrdiff_t) latency,
                              [](float value) { return value == 0.0f; }),
                  "dry path must begin with exactly the reported latency");
    auto dryAligned = true;
    for (std::size_t i = latency; i < sampleCount; ++i)
    {
        dryAligned &= std::abs(dry.left[i] - inputSample(i - latency, false)) < 1.0e-7f;
        dryAligned &= std::abs(dry.right[i] - inputSample(i - latency, true)) < 1.0e-7f;
    }
    ok &= require(dryAligned, "zero mix must preserve each stereo channel after latency compensation");
    ok &= require(difference(dry.left, dry.right) > 0.12f, "dry stereo channels must never be summed together");

    CDParameters damaged;
    damaged.mode = CDConcealment::random;
    damaged.damageShape = CDDamageShape::randomPits;
    damaged.errorRate = 0.78f;
    damaged.burstMs = 86.0f;
    damaged.repeatMs = 52.0f;
    damaged.scratchRate = 0.84f;
    damaged.scratchAmount = 0.82f;
    damaged.correction = 0.22f;
    damaged.trackingRate = 0.61f;
    damaged.trackingMs = 420.0f;
    damaged.servoHunt = 0.74f;
    damaged.jitterMs = 0.24f;
    damaged.jitterRateHz = 92.0f;
    damaged.highFrequencyLoss = 0.14f;
    damaged.servoNoise = 0.48f;
    damaged.carCompression = 0.35f;
    damaged.stereoLink = 0.82f;
    damaged.stereoWidth = 1.12f;
    damaged.softClip = true;
    const auto blockA = render(64, 0x5eed1234u, damaged);
    const auto blockB = render(997, 0x5eed1234u, damaged);
    const auto otherSeed = render(64, 0x5eed5678u, damaged);
    ok &= require(blockA.left == blockB.left && blockA.right == blockB.right,
                  "CD output must be independent of host block size");
    ok &= require(renderDifference(blockA, otherSeed) > 1.0e-5f,
                  "different seeds must change sector damage and servo behavior");
    ok &= require(std::all_of(blockA.left.begin(), blockA.left.end(), [](float value)
    {
        return std::isfinite(value) && std::abs(value) <= 1.00001f;
    }) && std::all_of(blockA.right.begin(), blockA.right.end(), [](float value)
    {
        return std::isfinite(value) && std::abs(value) <= 1.00001f;
    }), "CD output must remain finite and bounded");
    ok &= require(difference(blockA.left, blockA.right) > 0.025f,
                  "shared disc failures must preserve independent left and right program audio");

    CDProcessor manualSkip;
    manualSkip.prepare(sampleRate, 2);
    manualSkip.reset(77u);
    manualSkip.triggerSkip(1.0f);
    CDParameters manualSettings;
    manualSettings.errorRate = 0.0f;
    manualSettings.scratchRate = 0.0f;
    manualSettings.trackingRate = 0.0f;
    manualSettings.servoNoise = 0.0f;
    manualSettings.repeatMs = 28.0f;
    manualSettings.trackingMs = 90.0f;
    std::vector<float> manualLeft(6000);
    std::vector<float> manualRight(6000);
    for (std::size_t i = 0; i < manualLeft.size(); ++i)
    {
        manualLeft[i] = inputSample(i, false);
        manualRight[i] = inputSample(i, true);
    }
    float* manualChannels[] { manualLeft.data(), manualRight.data() };
    manualSkip.process(manualChannels, 2, manualLeft.size(), manualSettings);
    ok &= require(manualSkip.skipActive(), "a skip triggered before history is ready must remain queued and then fire");
    ok &= require(difference(manualLeft, manualRight) > 0.05f,
                  "manual repeat skips must read separate channel histories");

    CDProcessor musicalSkip;
    musicalSkip.prepare(sampleRate, 2);
    musicalSkip.reset(101u);
    CDParameters musicalSettings;
    musicalSettings.errorRate = musicalSettings.scratchRate = musicalSettings.trackingRate = 0.0f;
    musicalSettings.servoNoise = 0.0f;
    std::vector<float> warmLeft(4096), warmRight(4096);
    for (std::size_t i = 0; i < warmLeft.size(); ++i) { warmLeft[i]=inputSample(i,false); warmRight[i]=inputSample(i,true); }
    float* warmChannels[] { warmLeft.data(), warmRight.data() };
    musicalSkip.process(warmChannels,2,warmLeft.size(),musicalSettings);
    musicalSkip.triggerMusicalSkip(.8f,240,960,false);
    std::vector<float> firstLeft(300),firstRight(300);for(std::size_t i=0;i<firstLeft.size();++i){firstLeft[i]=inputSample(i+4096,false);firstRight[i]=inputSample(i+4096,true);}float* firstChannels[]{firstLeft.data(),firstRight.data()};musicalSkip.process(firstChannels,2,firstLeft.size(),musicalSettings);
    ok &= require(musicalSkip.skipActive(), "musical skip must use its explicit bounded hold length");
    musicalSkip.triggerMusicalSkip(.8f,240,960,false);
    std::vector<float> finishLeft(700),finishRight(700);for(std::size_t i=0;i<finishLeft.size();++i){finishLeft[i]=inputSample(i+4396,false);finishRight[i]=inputSample(i+4396,true);}float* finishChannels[]{finishLeft.data(),finishRight.data()};musicalSkip.process(finishChannels,2,finishLeft.size(),musicalSettings);
    ok &= require(!musicalSkip.skipActive(), "ignored retriggers must not queue another musical skip");

    const auto clean = lost_audio::core::mapCDMacros(0.95f, 0.02f, 0.01f, 0.02f);
    const auto ruined = lost_audio::core::mapCDMacros(0.12f, 0.88f, 0.82f, 0.76f);
    ok &= require(clean.errorRate < ruined.errorRate && clean.scratchAmount < ruined.scratchAmount,
                  "damage macros must increase error and scratch severity");
    ok &= require(clean.correction > ruined.correction,
                  "damage macros must reduce decoder correction ability");
    ok &= require(clean.trackingRate < ruined.trackingRate && clean.jitterMs < ruined.jitterMs,
                  "tracking and jitter macros must reach their dedicated mechanisms");

    if (!ok) return 1;
    std::cout << "CD core passed: stereo-preserving latency, deterministic sector damage, queued initial skips, bounded musical skips, safe retriggers, bounded output\n";
    return 0;
}
