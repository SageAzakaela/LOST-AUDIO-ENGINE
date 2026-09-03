#include <lost_audio/core/SequencerClock.h>

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << message << '\n';
        std::exit(1);
    }
}
}

int main()
{
    using lost_audio::core::sequencerPosition;
    using lost_audio::core::sequencerProbabilityHit;

    const auto start = sequencerPosition(0.0, 120.0, 4, 0.0f, 16, 48000.0);
    require(start.absoluteStep == 0 && start.patternStep == 0, "The pattern must begin on step one.");
    require(start.samplesUntilBoundary == 6000, "A sixteenth note at 120 BPM must last 6000 samples at 48 kHz.");

    const auto fifth = sequencerPosition(1.0, 120.0, 4, 0.0f, 16, 48000.0);
    require(fifth.absoluteStep == 4 && fifth.patternStep == 4, "Quarter-note PPQ must land on the fifth sixteenth step.");

    const auto wrapped = sequencerPosition(4.0, 120.0, 4, 0.0f, 16, 48000.0);
    require(wrapped.absoluteStep == 16 && wrapped.patternStep == 0, "A sixteen-step pattern must wrap after one 4/4 bar.");

    const auto swungFirst = sequencerPosition(0.30, 120.0, 4, 0.5f, 16, 48000.0);
    require(swungFirst.absoluteStep == 0, "Positive swing must lengthen the first step in each pair.");
    const auto swungSecond = sequencerPosition(0.40, 120.0, 4, 0.5f, 16, 48000.0);
    require(swungSecond.absoluteStep == 1, "The swung off-step must begin at the delayed boundary.");

    const auto negative = sequencerPosition(-0.01, 120.0, 4, 0.0f, 16, 48000.0);
    require(negative.patternStep == 15, "Negative pre-roll positions must wrap safely.");

    require(!sequencerProbabilityHit(7, 1234u, 0.0f), "Zero probability must never fire.");
    require(sequencerProbabilityHit(7, 1234u, 1.0f), "Full probability must always fire.");
    const auto firstRoll = sequencerProbabilityHit(104, 0xBEEFu, 0.5f);
    require(firstRoll == sequencerProbabilityHit(104, 0xBEEFu, 0.5f), "Probability must be stable for a host-grid step.");

    int hits = 0;
    for (int step = 0; step < 256; ++step)
        hits += sequencerProbabilityHit(step, 0xBEEFu, 0.5f) ? 1 : 0;
    require(hits > 90 && hits < 166, "Deterministic probability should remain plausibly distributed.");

    return 0;
}
