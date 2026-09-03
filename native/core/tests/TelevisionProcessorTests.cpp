#include <lost_audio/core/TelevisionProcessor.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace
{
using lost_audio::core::TelevisionModel;
using lost_audio::core::TelevisionParameters;
using lost_audio::core::TelevisionProcessor;
using lost_audio::core::TelevisionReception;

[[noreturn]] void fail(const char* message) { std::cerr << "Television core failure: " << message << '\n'; std::exit(1); }
std::vector<float> signal(std::size_t count, float phase = 0.0f)
{
    std::vector<float> out(count); for(std::size_t i=0;i<count;++i){const auto t=(float)i/48000.0f;out[i]=.24f*std::sin(6.2831853f*233.0f*t+phase)+.11f*std::sin(6.2831853f*1740.0f*t+phase*.3f);}return out;
}
void run(TelevisionProcessor& p, std::vector<float>& l, std::vector<float>& r, const TelevisionParameters& params, std::size_t block)
{
    for(std::size_t pos=0;pos<l.size();pos+=block){const auto n=std::min(block,l.size()-pos);float* channels[]{l.data()+pos,r.data()+pos};p.process(channels,2,n,params);}
}
float difference(const std::vector<float>& a,const std::vector<float>& b){float total=0;for(std::size_t i=0;i<a.size();++i)total+=std::abs(a[i]-b[i]);return total/(float)a.size();}
}

int main()
{
    constexpr std::size_t count=48000;
    {
        auto l=signal(count),r=signal(count,.7f),ol=l,orr=r;TelevisionProcessor p;p.prepare(48000,2);TelevisionParameters x;x.mix=0;x.outputGain=1;x.ceiling=1;x.limiter=0;x.staticAmount=x.hum=x.whine=0;run(p,l,r,x,127);if(difference(l,ol)>1e-6f||difference(r,orr)>1e-6f)fail("zero mix is not transparent");
    }
    {
        auto aL=signal(count),aR=signal(count,.3f),bL=aL,bR=aR;TelevisionParameters x;x.reception=TelevisionReception::detuned;x.staticAmount=.55f;x.syncInstability=.7f;x.cabinetRattle=.4f;TelevisionProcessor a,b;a.prepare(48000,2);b.prepare(48000,2);a.reset(991);b.reset(991);run(a,aL,aR,x,64);run(b,bL,bR,x,511);if(difference(aL,bL)>2e-5f||difference(aR,bR)>2e-5f)fail("host block size changed deterministic output");
    }
    {
        auto l=signal(count),r=signal(count,1.2f);TelevisionParameters x;x.staticAmount=.16f;x.hum=.12f;x.whine=.08f;TelevisionProcessor p;p.prepare(48000,2);run(p,l,r,x,193);if(difference(l,r)<.01f)fail("stereo content collapsed");for(const auto v:l)if(!std::isfinite(v)||std::abs(v)>1.001f)fail("left output not finite/bounded");for(const auto v:r)if(!std::isfinite(v)||std::abs(v)>1.001f)fail("right output not finite/bounded");
    }
    {
        std::vector<float> l(count,0),r(count,0);TelevisionParameters x;x.staticAmount=x.hum=x.whine=0;x.cabinetRattle=0;TelevisionProcessor p;p.prepare(48000,2);run(p,l,r,x,256);float peak=0;for(const auto v:l)peak=std::max(peak,std::abs(v));if(peak>1e-7f)fail("disabled electrical texture produced sound");
    }
    {
        auto sourceL=signal(count),sourceR=signal(count,.4f),portableL=sourceL,portableR=sourceR,monitorL=sourceL,monitorR=sourceR;TelevisionParameters p1,p2;const auto a=lost_audio::core::mapTelevisionMacros(TelevisionModel::portable,TelevisionReception::detuned,.85f,.18f,.6f,.55f),b=lost_audio::core::mapTelevisionMacros(TelevisionModel::broadcastMonitor,TelevisionReception::baseband,.12f,.9f,.08f,.01f);p1.model=TelevisionModel::portable;p1.reception=TelevisionReception::detuned;p1.highPassHz=a.highPassHz;p1.lowPassHz=a.lowPassHz;p1.midHumpDb=a.midHumpDb;p1.midFrequencyHz=a.midFrequencyHz;p1.drive=a.drive;p1.compression=a.compression;p1.staticAmount=.55f;p1.noiseHiss=a.noiseHiss;p1.noiseCrackle=a.noiseCrackle;p1.tunerDrift=a.tunerDrift;p1.syncInstability=a.syncInstability;p1.cabinet=a.cabinet;p1.cabinetRattle=a.cabinetRattle;p2.model=TelevisionModel::broadcastMonitor;p2.reception=TelevisionReception::baseband;p2.highPassHz=b.highPassHz;p2.lowPassHz=b.lowPassHz;p2.midHumpDb=b.midHumpDb;p2.midFrequencyHz=b.midFrequencyHz;p2.drive=b.drive;p2.compression=b.compression;p2.staticAmount=.01f;p2.noiseHiss=b.noiseHiss;p2.noiseCrackle=b.noiseCrackle;p2.tunerDrift=b.tunerDrift;p2.syncInstability=b.syncInstability;p2.cabinet=b.cabinet;p2.cabinetRattle=b.cabinetRattle;TelevisionProcessor one,two;one.prepare(48000,2);two.prepare(48000,2);run(one,portableL,portableR,p1,128);run(two,monitorL,monitorR,p2,128);if(difference(portableL,monitorL)<.025f)fail("set and reception models are not materially distinct");
    }
    {
        auto l=signal(256),r=signal(256,.2f);TelevisionParameters x;x.syncInstability=0;x.staticAmount=x.hum=x.whine=0;TelevisionProcessor p;p.prepare(48000,2);p.triggerSyncFault(.8f,.04f);float* channels[]{l.data(),r.data()};p.process(channels,2,64,x);if(!p.syncFaultActive()||p.syncProgress()<=0)fail("manual sync fault did not enter the DSP timeline");
    }
    std::cout<<"Television core passed: transparent mix, block invariance, stereo sets, explicit texture ownership, model separation, manual sync fault, bounded output\n";return 0;
}
