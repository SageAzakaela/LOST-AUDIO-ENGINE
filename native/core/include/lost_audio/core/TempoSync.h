#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <cstdint>

namespace lost_audio::core
{
struct TempoEvent
{
    int sampleOffset = 0;
    std::int64_t stepIndex = 0;
};

struct TempoEventSchedule
{
    static constexpr std::size_t capacity = 32;
    std::array<TempoEvent, capacity> events {};
    std::size_t size = 0;
};

inline constexpr std::array<float, 11> tempoDivisionBeats {
    4.0f,        // 1 bar in 4/4
    2.0f,        // 1/2
    1.0f,        // 1/4
    0.5f,        // 1/8
    0.25f,       // 1/16
    0.125f,      // 1/32
    2.0f / 3.0f, // 1/4 triplet
    1.0f / 3.0f, // 1/8 triplet
    1.0f / 6.0f, // 1/16 triplet
    0.75f,       // dotted 1/8
    0.375f       // dotted 1/16
};

[[nodiscard]] inline float tempoDivisionInBeats(int divisionIndex) noexcept
{
    return tempoDivisionBeats[static_cast<std::size_t>(
        std::clamp(divisionIndex, 0, static_cast<int>(tempoDivisionBeats.size()) - 1))];
}

[[nodiscard]] inline float tempoDivisionMilliseconds(double bpm, int divisionIndex) noexcept
{
    const auto safeBpm = std::clamp(bpm, 20.0, 400.0);
    return static_cast<float>((60000.0 / safeBpm) * tempoDivisionInBeats(divisionIndex));
}

[[nodiscard]] inline float tempoDivisionRateHz(double bpm, int divisionIndex) noexcept
{
    return 1000.0f / tempoDivisionMilliseconds(bpm, divisionIndex);
}

// Stable per-grid-step probability. This makes performer patterns repeatable
// across host block sizes, offline renders, and every engine using the clock.
[[nodiscard]] inline bool tempoEventDecision(std::int64_t stepIndex, float probability,
                                              std::uint64_t seed = 0) noexcept
{
    if (probability >= 1.0f) return true;
    if (probability <= 0.0f) return false;
    auto value = static_cast<std::uint64_t>(stepIndex) + 0x9e3779b97f4a7c15ull + seed;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ull;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebull;
    value ^= value >> 31;
    const auto unit = static_cast<float>((value >> 40) * (1.0 / 16777216.0));
    return unit < probability;
}

[[nodiscard]] inline std::int64_t tempoStepIndex(double ppqPosition, int divisionIndex) noexcept
{
    const auto beats = static_cast<double>(tempoDivisionInBeats(divisionIndex));
    return static_cast<std::int64_t>(std::floor((ppqPosition + 1.0e-9) / beats));
}

[[nodiscard]] inline int samplesUntilNextTempoStep(double ppqPosition, double bpm,
                                                    int divisionIndex, double sampleRate) noexcept
{
    const auto beats = static_cast<double>(tempoDivisionInBeats(divisionIndex));
    auto phase = std::fmod(ppqPosition, beats);
    if (phase < 0.0) phase += beats;
    const auto remainingBeats = phase < 1.0e-7 ? 0.0 : beats - phase;
    return std::max(0, static_cast<int>(std::llround(
        remainingBeats * 60.0 / std::clamp(bpm, 20.0, 400.0) * std::max(1.0, sampleRate))));
}

// Returns every musical boundary that occurs inside the current audio block.
// The end of the block is exclusive, so a boundary shared by adjacent blocks
// is emitted exactly once by the later block. Consumers can split their DSP at
// each offset instead of quantizing an event to the start of the host block.
[[nodiscard]] inline TempoEventSchedule tempoEventsInBlock(double ppqPosition, double bpm,
                                                            int divisionIndex, double sampleRate,
                                                            int sampleCount) noexcept
{
    TempoEventSchedule schedule;
    if (sampleCount <= 0 || !std::isfinite(ppqPosition)) return schedule;

    const auto safeBpm = std::clamp(bpm, 20.0, 400.0);
    const auto safeRate = std::max(1.0, sampleRate);
    const auto stepBeats = static_cast<double>(tempoDivisionInBeats(divisionIndex));
    const auto beatsPerSample = safeBpm / (60.0 * safeRate);
    const auto blockEndPpq = ppqPosition + static_cast<double>(sampleCount) * beatsPerSample;
    auto step = static_cast<std::int64_t>(std::ceil((ppqPosition - 1.0e-9) / stepBeats));

    while (schedule.size < TempoEventSchedule::capacity)
    {
        const auto eventPpq = static_cast<double>(step) * stepBeats;
        if (eventPpq >= blockEndPpq - 1.0e-12) break;
        if (eventPpq >= ppqPosition - 1.0e-9)
        {
            const auto offset = static_cast<int>(std::llround((eventPpq - ppqPosition) / beatsPerSample));
            if (offset >= 0 && offset < sampleCount)
                schedule.events[schedule.size++] = { offset, step };
        }
        ++step;
    }
    return schedule;
}
}
