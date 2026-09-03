#include <lost_audio/core/OpenMicProcessor.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace lost_audio::core;

namespace
{
[[noreturn]] void fail(const char* message)
{
    std::cerr << "Open Mic core failure: " << message << '\n';
    std::exit(1);
}

std::vector<float> signal(std::size_t count, float phase = 0.0f)
{
    std::vector<float> result(count);
    for (std::size_t i = 0; i < count; ++i)
        result[i] = 0.21f * std::sin(6.2831853f * 173.0f * static_cast<float>(i) / 48000.0f + phase)
                  + 0.08f * std::sin(6.2831853f * 2130.0f * static_cast<float>(i) / 48000.0f);
    return result;
}

void run(OpenMicProcessor& processor, std::vector<float>& left, std::vector<float>& right,
         const OpenMicParameters& parameters, std::size_t block)
{
    for (std::size_t offset = 0; offset < left.size(); offset += block)
    {
        const auto count = std::min(block, left.size() - offset);
        float* channels[] { left.data() + offset, right.data() + offset };
        processor.process(channels, 2, count, parameters);
    }
}

float difference(const std::vector<float>& a, const std::vector<float>& b)
{
    float total = 0.0f;
    for (std::size_t i = 0; i < a.size(); ++i) total += std::abs(a[i] - b[i]);
    return total / static_cast<float>(a.size());
}
} // namespace

int main()
{
    constexpr std::size_t count = 48000;
    {
        auto left = signal(count), right = signal(count, 0.71f);
        const auto originalLeft = left, originalRight = right;
        OpenMicParameters p; p.mix = 0.0f; p.outputGain = 1.0f; p.limiterAmount = 0.0f;
        OpenMicProcessor processor; processor.prepare(48000.0, 2); run(processor, left, right, p, 127);
        if (difference(left, originalLeft) > 1.0e-6f || difference(right, originalRight) > 1.0e-6f) fail("dry mix is not transparent");
    }
    {
        auto left = signal(count), right = signal(count, 0.41f), left2 = left, right2 = right;
        OpenMicParameters p; p.mic = OpenMicModel::karaoke; p.venue = OpenMicVenue::diveBar; p.pa = OpenMicPA::tiredCombo;
        p.crowdLevel = 0.3f; p.stageBleed = 0.25f; p.feedbackArmed = true; p.feedbackAmount = 0.62f;
        OpenMicProcessor a, b; a.prepare(48000.0, 2); b.prepare(48000.0, 2); a.reset(77); b.reset(77);
        run(a, left, right, p, 64); run(b, left2, right2, p, 511);
        if (difference(left, left2) > 3.0e-5f || difference(right, right2) > 3.0e-5f) fail("block invariance");
    }
    {
        auto club = signal(count), clubR = signal(count, 0.8f), roof = club, roofR = clubR;
        OpenMicParameters a; a.venue = OpenMicVenue::cornerClub; a.mic = OpenMicModel::dynamicHandheld; a.pa = OpenMicPA::compact;
        OpenMicParameters b = a; b.venue = OpenMicVenue::rooftop; b.mic = OpenMicModel::vocalCondenser; b.pa = OpenMicPA::horn;
        OpenMicProcessor pa, pb; pa.prepare(48000.0, 2); pb.prepare(48000.0, 2); run(pa, club, clubR, a, 128); run(pb, roof, roofR, b, 128);
        if (difference(club, roof) < 0.012f) fail("venue/device models are not distinct");
        if (difference(club, clubR) < 0.004f) fail("stereo field collapsed");
    }
    {
        std::vector<float> left(count, 0.0f), right(count, 0.0f);
        OpenMicParameters p; p.feedbackArmed = false; p.feedbackAmount = 1.0f; p.crowdLevel = 0.0f; p.electricalNoise = 0.0f; p.venueBedEnabled = false;
        OpenMicProcessor processor; processor.prepare(48000.0, 2); run(processor, left, right, p, 128);
        if (processor.feedbackActivity() > 1.0e-6f || difference(left, right) > 1.0e-6f) fail("unarmed feedback produced a hidden event");
    }
    {
        std::vector<float> left(count,0),right(count,0);OpenMicParameters p;p.feedbackArmed=false;p.venueBedEnabled=false;p.venueBedLevel=0;p.crowdLevel=.8f;p.electricalNoise=0;p.roomAmount=0;OpenMicProcessor processor;processor.prepare(48000,2);run(processor,left,right,p,128);float magnitude=0;for(const auto x:left)magnitude=std::max(magnitude,std::abs(x));if(magnitude<1e-4f||processor.crowdActivity()<=0)fail("audience control was incorrectly gated by venue bed");
        processor.triggerCrowdEvent(OpenMicCrowdEvent::applause,.8f,96000);std::fill(left.begin(),left.end(),0.0f);std::fill(right.begin(),right.end(),0.0f);run(processor,left,right,p,128);if(!processor.crowdEventActive()||processor.crowdEventProgress()<=0||processor.crowdActivity()<=0)fail("conducted audience event or telemetry failed");
    }
    {
        auto left = signal(count), right = signal(count, 0.5f);
        OpenMicParameters p; p.feedbackArmed = true; p.feedbackAmount = 1.0f; p.monitorLevel = 1.0f; p.ceiling = 0.82f; p.limiterAmount = 1.0f;
        OpenMicProcessor processor; processor.prepare(48000.0, 2); run(processor, left, right, p, 63);
        for (const auto value : left) if (!std::isfinite(value) || std::abs(value) > 0.821f) fail("armed feedback escaped safety ceiling");
    }
    std::cout << "Open Mic core passed: transparent mix, block invariance, stereo venues, independent crowd, conducted audience events, explicit feedback arm, bounded output\n";
}
