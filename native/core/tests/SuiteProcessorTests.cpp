#include <lost_audio/core/SuiteProcessor.h>

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
    std::cerr << "Suite core failure: " << message << '\n';
    std::exit(1);
}

std::vector<float> signal(std::size_t count, float phase = 0.0f)
{
    std::vector<float> output(count);
    for (std::size_t i = 0; i < count; ++i)
        output[i] = .19f * std::sin(6.2831853f * 193.0f * static_cast<float>(i) / 48000.0f + phase)
                  + .07f * std::sin(6.2831853f * 1770.0f * static_cast<float>(i) / 48000.0f);
    return output;
}

void run(SuiteProcessor& processor, std::vector<float>& left, std::vector<float>& right,
         const SuiteParameters& parameters, std::size_t block)
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
    float sum = 0.0f;
    for (std::size_t i = 0; i < a.size(); ++i) sum += std::abs(a[i] - b[i]);
    return sum / static_cast<float>(a.size());
}
}

int main()
{
    {
        SuiteProcessor processor; processor.prepare(48000.0, 2);
        const auto latency = processor.latencySamples();
        std::vector<float> left(static_cast<std::size_t>(latency + 512), 0.0f), right(left.size(), 0.0f);
        left[0] = right[0] = .5f;
        SuiteParameters p; p.limiter = 0.0f; p.ceiling = .99f;
        run(processor, left, right, p, 127);
        if (std::abs(left[static_cast<std::size_t>(latency)] - .5f) > 1.0e-5f) fail("fixed host latency is not compensated");
        for (int i = 0; i < latency; ++i) if (std::abs(left[static_cast<std::size_t>(i)]) > 1.0e-6f) fail("signal escaped before reported latency");
    }
    {
        auto left = signal(72000), right = signal(72000, .67f), left2 = left, right2 = right;
        SuiteParameters p; p.slots[0].engine = SuiteEngine::tape; p.slots[0].macroA=.42f;p.slots[0].macroB=.31f;
        p.slots[1].engine = SuiteEngine::television;p.slots[1].macroA=.37f;p.slots[1].macroB=.18f;
        p.slots[2].engine = SuiteEngine::occlusion;p.slots[2].macroA=.58f;p.slots[2].macroB=.45f;
        SuiteProcessor a,b;a.prepare(48000,2);b.prepare(48000,2);a.reset(91);b.reset(91);
        run(a,left,right,p,64);run(b,left2,right2,p,509);
        if (difference(left,left2)>4.0e-5f || difference(right,right2)>4.0e-5f) fail("host block size changed the serial render");
        if (difference(left,right)<.002f) fail("stereo field collapsed");
    }
    {
        auto source=signal(64000), right=signal(64000,.43f), forward=source, forwardR=right, reverse=source, reverseR=right;
        SuiteParameters a;a.slots[0].engine=SuiteEngine::cartridge;a.slots[0].macroA=.67f;a.slots[0].macroB=.19f;
        a.slots[1].engine=SuiteEngine::occlusion;a.slots[1].macroA=.73f;a.slots[1].macroB=.52f;
        auto b=a;b.order={1,0,2,3,4,5};SuiteProcessor pa,pb;pa.prepare(48000,2);pb.prepare(48000,2);
        run(pa,forward,forwardR,a,128);run(pb,reverse,reverseR,b,128);
        if(difference(forward,reverse)<.0015f)fail("slot order did not alter nonlinear routing");
    }
    {
        auto once=signal(64000),onceR=signal(64000,.3f),twice=once,twiceR=onceR;SuiteParameters a;a.slots[0].engine=SuiteEngine::tape;a.slots[0].macroA=.48f;a.slots[0].macroB=.38f;
        auto b=a;b.slots[1]=b.slots[0];SuiteProcessor pa,pb;pa.prepare(48000,2);pb.prepare(48000,2);run(pa,once,onceR,a,96);run(pb,twice,twiceR,b,96);
        if(difference(once,twice)<.002f)fail("repeated engine slots did not retain independent stages");
    }
    {
        auto left=signal(48000),right=signal(48000,.8f);SuiteParameters a;a.slots[0].engine=SuiteEngine::transmission;a.slots[0].macroA=.35f;a.slots[0].macroB=.2f;
        SuiteProcessor p;p.prepare(48000,2);float* first[]{left.data(),right.data()};p.process(first,2,12000,a);a.slots[0].engine=SuiteEngine::conference;a.slots[0].macroB=.8f;
        float* second[]{left.data()+12000,right.data()+12000};p.process(second,2,left.size()-12000,a);
        if(p.topologyActivity()<.99f)fail("topology transition did not recover");
        for(auto value:left)if(!std::isfinite(value)||std::abs(value)>1.001f)fail("topology transition escaped output bounds");
    }
    {
        auto left=signal(48000),right=signal(48000,.4f);SuiteParameters p;p.outputGain=1.5f;p.ceiling=.76f;p.limiter=1.0f;
        for(auto& slot:p.slots){slot.engine=SuiteEngine::openMicNight;slot.macroA=1;slot.macroB=1;slot.feedbackArmed=true;}
        SuiteProcessor processor;processor.prepare(48000,2);run(processor,left,right,p,63);
        for(auto value:left)if(!std::isfinite(value)||std::abs(value)>.761f)fail("maximum chain escaped master ceiling");
    }
    {
        auto stale=signal(64000),staleR=signal(64000,.2f),safe=stale,safeR=staleR;SuiteParameters armed;armed.slots[0].engine=SuiteEngine::openMicNight;armed.slots[0].macroA=.9f;armed.slots[0].macroB=1;armed.slots[0].feedbackArmed=true;
        auto disarmed=armed;disarmed.slots[0].feedbackArmed=false;SuiteProcessor a,b;a.prepare(48000,2);b.prepare(48000,2);a.reset(31);b.reset(31);run(a,stale,staleR,armed,64);run(b,safe,safeR,disarmed,64);
        if(difference(stale,safe)>1.0e-6f)fail("stale feedback arm was honored without an observed off state");
        auto latched=signal(96000),latchedR=signal(96000,.2f),control=latched,controlR=latchedR;SuiteProcessor c,d;c.prepare(48000,2);d.prepare(48000,2);c.reset(44);d.reset(44);
        float* c0[]{latched.data(),latchedR.data()};float* d0[]{control.data(),controlR.data()};c.process(c0,2,48000,disarmed);d.process(d0,2,48000,disarmed);float* c1[]{latched.data()+48000,latchedR.data()+48000};float* d1[]{control.data()+48000,controlR.data()+48000};c.process(c1,2,48000,armed);d.process(d1,2,48000,disarmed);
        if(difference(latched,control)<1.0e-4f)fail("deliberate off-to-on feedback transition was not honored");
    }
    std::cout << "Suite core passed: fixed latency, block invariance, stereo serial order, repeated engines, click-safe topology, explicit feedback latch, bounded output\n";
}
