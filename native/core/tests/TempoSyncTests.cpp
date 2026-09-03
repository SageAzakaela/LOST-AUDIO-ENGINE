#include <lost_audio/core/TempoSync.h>

#include <cmath>
#include <iostream>

namespace
{
bool near(float actual, float expected, float tolerance = 0.001f)
{
    return std::abs(actual - expected) <= tolerance;
}
}

int main()
{
    using lost_audio::core::tempoDivisionInBeats;
    using lost_audio::core::tempoDivisionMilliseconds;
    using lost_audio::core::tempoDivisionRateHz;
    using lost_audio::core::tempoStepIndex;
    using lost_audio::core::tempoEventsInBlock;
    using lost_audio::core::tempoEventDecision;
    using lost_audio::core::samplesUntilNextTempoStep;

    const auto boundaryAtStart = tempoEventsInBlock(2.0, 120.0, 2, 48000.0, 512);
    const auto boundaryInside = tempoEventsInBlock(1.99, 120.0, 2, 48000.0, 512);
    const auto boundaryAtEnd = tempoEventsInBlock(1.978666666667, 120.0, 2, 48000.0, 512);
    const auto fastMultiple = tempoEventsInBlock(0.0, 400.0, 5, 48000.0, 4096);

    if (!tempoEventDecision(42, 1.0f) || tempoEventDecision(42, 0.0f)
        || tempoEventDecision(99, 0.42f, 7) != tempoEventDecision(99, 0.42f, 7)
        || !near(tempoDivisionMilliseconds(120.0, 0), 2000.0f)
        || !near(tempoDivisionMilliseconds(120.0, 2), 500.0f)
        || !near(tempoDivisionMilliseconds(120.0, 3), 250.0f)
        || !near(tempoDivisionMilliseconds(120.0, 7), 166.6667f, 0.01f)
        || !near(tempoDivisionMilliseconds(120.0, 9), 375.0f)
        || !near(tempoDivisionRateHz(120.0, 4), 8.0f)
        || !near(tempoDivisionInBeats(-100), 4.0f)
        || !near(tempoDivisionInBeats(100), 0.375f)
        || tempoStepIndex(1.01, 3) != 2
        || samplesUntilNextTempoStep(1.25, 120.0, 3, 48000.0) != 6000
        || samplesUntilNextTempoStep(2.0, 120.0, 2, 48000.0) != 0
        || boundaryAtStart.size != 1 || boundaryAtStart.events[0].sampleOffset != 0
        || boundaryAtStart.events[0].stepIndex != 2
        || boundaryInside.size != 1 || boundaryInside.events[0].sampleOffset != 240
        || boundaryAtEnd.size != 0
        || fastMultiple.size < 4)
    {
        std::cerr << "Tempo division conversion failed\n";
        return 1;
    }

    std::cout << "Tempo sync conversion passed for straight, triplet, dotted, and clamped divisions\n";
}
