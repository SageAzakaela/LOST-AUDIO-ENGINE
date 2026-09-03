#include <lost_audio/core/CamcorderProcessor.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

namespace
{
using lost_audio::core::CamcorderFormat;
using lost_audio::core::CamcorderParameters;
using lost_audio::core::CamcorderProcessor;

bool require(bool condition, const char* message) { if (!condition) std::cerr << "FAIL: " << message << '\n'; return condition; }
struct Render { std::vector<float> left, right; int latency = 0; };
Render render(std::size_t blockSize, std::uint32_t seed, const CamcorderParameters& parameters)
{
    constexpr double sampleRate = 48000; constexpr std::size_t count = 96000;
    CamcorderProcessor processor; processor.prepare(sampleRate, 2); processor.reset(seed);
    Render result { std::vector<float>(count), std::vector<float>(count), processor.latencySamples() };
    for (std::size_t i = 0; i < count; ++i)
    {
        const auto t = (float) i / 48000.0f;
        result.left[i] = .16f * std::sin(2 * 3.14159265358979323846f * 223 * t) + .07f * std::sin(2 * 3.14159265358979323846f * 1830 * t);
        result.right[i] = .13f * std::sin(2 * 3.14159265358979323846f * 359 * t) + .06f * std::sin(2 * 3.14159265358979323846f * 2670 * t);
    }
    for (std::size_t offset = 0; offset < count; offset += blockSize)
    {
        const auto n = std::min(blockSize, count - offset); float* channels[] { result.left.data() + offset, result.right.data() + offset };
        processor.process(channels, 2, n, parameters);
    }
    return result;
}
float difference(const std::vector<float>& a, const std::vector<float>& b)
{ float sum = 0; for (std::size_t i = 0; i < a.size(); ++i) sum += std::abs(a[i] - b[i]); return sum / (float) a.size(); }
}

int main()
{
    bool ok = true;
    CamcorderParameters dry; dry.mix = 0;
    const auto dryRender = render(257, 123u, dry);
    ok &= require(dryRender.latency == 144, "Camcorder must report a fixed 3 ms capture latency at 48 kHz");
    float error = 0;
    for (std::size_t i = (std::size_t) dryRender.latency; i < dryRender.left.size(); ++i)
    {
        const auto source = i - (std::size_t) dryRender.latency;
        const auto t = (float) source / 48000.0f;
        const auto reference = .16f * std::sin(2 * 3.14159265358979323846f * 223 * t) + .07f * std::sin(2 * 3.14159265358979323846f * 1830 * t);
        error += std::abs(dryRender.left[i] - reference);
    }
    ok &= require(error / (float) (dryRender.left.size() - (std::size_t) dryRender.latency) < 1e-6f,
                  "zero mix must be a latency-aligned transparent dry path");

    CamcorderParameters damaged; damaged.format = CamcorderFormat::vhsc; damaged.coverage = .62f; damaged.movement = .72f; damaged.corruption = .78f;
    damaged.agcDrive = .64f; damaged.windEnabled = true; damaged.windLevel = .65f; damaged.flutter = .72f; damaged.crush = .68f; damaged.bits = 7;
    damaged.converterRateHz = 10000; damaged.dropout = .64f; damaged.chirp = .48f; damaged.handling = .68f; damaged.rub = .52f; damaged.hiss = .28f;
    const auto blockA = render(64, 0x5eedu, damaged), blockB = render(997, 0x5eedu, damaged), other = render(64, 0x7eedu, damaged);
    ok &= require(blockA.left == blockB.left && blockA.right == blockB.right, "Camcorder output must be independent of host block size");
    ok &= require(difference(blockA.left, other.left) > 1e-4f, "different seeds must change capture artifacts");
    ok &= require(difference(blockA.left, blockA.right) > .01f, "linked events must preserve independent stereo content");
    ok &= require(std::all_of(blockA.left.begin(), blockA.left.end(), [](float value) { return std::isfinite(value) && std::abs(value) <= 1.00001f; }),
                  "Camcorder output must remain finite and bounded");

    CamcorderParameters windOff; windOff.windEnabled = false; windOff.windLevel = 0; const auto noWind = render(128, 19u, windOff);
    windOff.windLevel = 1.5f; const auto stillNoWind = render(128, 19u, windOff);
    ok &= require(noWind.left == stillNoWind.left, "wind level must do nothing while the explicit Wind switch is off");

    CamcorderProcessor windA, windB; windA.prepare(48000, 1); windB.prepare(48000, 1); windA.reset(44u); windB.reset(44u);
    CamcorderParameters calm, corrupt; calm.windEnabled = corrupt.windEnabled = true; calm.movement = corrupt.movement = .92f; calm.windLevel = corrupt.windLevel = .7f;
    calm.corruption = 0; corrupt.corruption = 1; calm.dropout = corrupt.dropout = 0; calm.chirp = corrupt.chirp = 0; calm.hiss = corrupt.hiss = 0;
    bool sameWindSchedule = true; float zeroA = 0, zeroB = 0;
    for (int i = 0; i < 120000; ++i)
    {
        float* a[] { &zeroA }; float* b[] { &zeroB }; windA.process(a, 1, 1, calm); windB.process(b, 1, 1, corrupt);
        if (windA.windActive() != windB.windActive()) { sameWindSchedule = false; break; }
        zeroA = zeroB = 0;
    }
    ok &= require(sameWindSchedule, "corruption must not alter Wind scheduling or switch state");

    const auto subtle = lost_audio::core::mapCamcorderMacros(CamcorderFormat::miniDV, lost_audio::core::CameraMic::stereoCapsule, .15f, .08f, .05f, .2f);
    const auto found = lost_audio::core::mapCamcorderMacros(CamcorderFormat::vhsc, lost_audio::core::CameraMic::cheapMono, .75f, .7f, .86f, .62f);
    ok &= require(found.dropout > subtle.dropout * 5 && found.bits < subtle.bits && found.lowPassHz < subtle.lowPassHz,
                  "protected macros must span subtle capture through found-footage damage");
    CamcorderProcessor conducted; conducted.prepare(48000, 2); conducted.reset(88u); CamcorderParameters eventParams; eventParams.dropout = 0; eventParams.chirp = 0; eventParams.handling = 0; eventParams.windEnabled = false;
    std::vector<float> eventLeft(512, .2f), eventRight(512, -.15f); float* eventChannels[] { eventLeft.data(), eventRight.data() };
    conducted.triggerDropout(.12f); conducted.triggerCodecFault(.8f, .15f); conducted.triggerHandling(.7f); conducted.process(eventChannels, 2, eventLeft.size(), eventParams);
    ok &= require(conducted.dropoutActive() && conducted.dropoutProgress() > 0, "manual capture loss must start and report progress");
    ok &= require(conducted.corruptionActive() && conducted.corruptionProgress() > 0, "manual codec fault must start and report progress");
    ok &= require(conducted.handlingActive() && conducted.handlingProgress() > 0, "manual body impact must start and report progress");
    if (!ok) return 1;
    std::cout << "Camcorder core passed: aligned latency, deterministic capture, explicit wind ownership, conducted events, bounded output\n";
    return 0;
}
