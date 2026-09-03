#pragma once

#include <lost_audio/core/TempoSync.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace lost_audio::core
{
struct SequencerPosition
{
    std::int64_t absoluteStep = 0;
    int patternStep = 0;
    double phase = 0.0;
    int samplesUntilBoundary = 1;
};

[[nodiscard]] inline SequencerPosition sequencerPosition(double ppqPosition,
                                                          double bpm,
                                                          int divisionIndex,
                                                          float swing,
                                                          int patternLength,
                                                          double sampleRate) noexcept
{
    const auto stepBeats = static_cast<double>(tempoDivisionInBeats(divisionIndex));
    const auto pairBeats = stepBeats * 2.0;
    const auto safeSwing = static_cast<double>(std::clamp(swing, 0.0f, 0.5f));
    const auto firstDuration = stepBeats * (1.0 + safeSwing);
    const auto secondDuration = stepBeats * (1.0 - safeSwing);
    const auto pairIndex = static_cast<std::int64_t>(std::floor(ppqPosition / pairBeats));
    const auto pairStart = static_cast<double>(pairIndex) * pairBeats;
    const auto withinPair = std::clamp(ppqPosition - pairStart, 0.0, pairBeats);

    const auto oddStep = withinPair >= firstDuration;
    const auto duration = oddStep ? secondDuration : firstDuration;
    const auto withinStep = oddStep ? withinPair - firstDuration : withinPair;
    const auto absoluteStep = pairIndex * 2 + (oddStep ? 1 : 0);
    const auto length = std::clamp(patternLength, 1, 16);
    const auto wrapped = static_cast<int>((absoluteStep % length + length) % length);
    const auto remainingBeats = std::max(0.0, duration - withinStep);
    const auto safeBpm = std::clamp(bpm, 20.0, 400.0);
    const auto safeRate = std::max(1.0, sampleRate);
    const auto samples = std::max(1, static_cast<int>(std::ceil(remainingBeats * 60.0 / safeBpm * safeRate)));

    return { absoluteStep, wrapped, std::clamp(withinStep / duration, 0.0, 1.0), samples };
}

[[nodiscard]] inline bool sequencerProbabilityHit(std::int64_t absoluteStep,
                                                  std::uint32_t seed,
                                                  float probability) noexcept
{
    if (probability <= 0.0f) return false;
    if (probability >= 1.0f) return true;

    auto value = static_cast<std::uint64_t>(absoluteStep) ^
                 (static_cast<std::uint64_t>(seed) << 32u) ^ 0x9e3779b97f4a7c15ull;
    value ^= value >> 30u;
    value *= 0xbf58476d1ce4e5b9ull;
    value ^= value >> 27u;
    value *= 0x94d049bb133111ebull;
    value ^= value >> 31u;
    const auto normalized = static_cast<float>((value >> 40u) & 0xFFFFFFu) / 16777216.0f;
    return normalized < probability;
}
} // namespace lost_audio::core
