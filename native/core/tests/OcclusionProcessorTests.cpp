#include <lost_audio/core/OcclusionProcessor.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace lost_audio::core;
namespace
{
[[noreturn]] void fail(const char* message){std::cerr<<"Occlusion core failure: "<<message<<'\n';std::exit(1);}
std::vector<float> signal(std::size_t n,float phase=0){std::vector<float>x(n);for(std::size_t i=0;i<n;++i)x[i]=.24f*std::sin(6.2831853f*220*i/48000+phase)+.1f*std::sin(6.2831853f*1800*i/48000);return x;}
void run(OcclusionProcessor&p,std::vector<float>&l,std::vector<float>&r,const OcclusionParameters&x,int block){for(std::size_t o=0;o<l.size();o+=(std::size_t)block){auto n=std::min<std::size_t>((std::size_t)block,l.size()-o);float*d[]{l.data()+o,r.data()+o};p.process(d,2,n,x);}}
float difference(const std::vector<float>&a,const std::vector<float>&b){double d=0;for(std::size_t i=0;i<a.size();++i)d+=std::abs(a[i]-b[i]);return(float)(d/a.size());}
float tailEnergy(const std::vector<float>&a,std::size_t start){double e=0;for(std::size_t i=start;i<a.size();++i)e+=std::abs(a[i]);return(float)(e/(a.size()-start));}
}

int main()
{
    constexpr int n=48000;
    {
        auto l=signal(n),r=signal(n,.7f),a=l,b=r;OcclusionProcessor p;p.prepare(48000,2);OcclusionParameters x;x.mix=0;x.outputGain=1;x.ceiling=1;x.limiter=0;run(p,l,r,x,127);if(difference(l,a)>1e-6f||difference(r,b)>1e-6f)fail("dry mix");
    }
    {
        auto l=signal(n),r=signal(n,.4f),a=l,b=r;OcclusionParameters x;x.material=OcclusionMaterial::metal;x.construction=OcclusionConstruction::loose;x.rattle=.7f;x.smear=.6f;OcclusionProcessor p,q;p.prepare(48000,2);q.prepare(48000,2);p.reset(77);q.reset(77);run(p,l,r,x,64);run(q,a,b,x,511);if(difference(l,a)>3e-5f)fail("block invariance");
    }
    {
        auto src=signal(n),r=signal(n,.9f),wood=src,wr=r,brick=src,br=r;auto a=mapOcclusionMacros(OcclusionMaterial::wood,OcclusionConstruction::hollow,.5f,.7f,.4f,.5f),b=mapOcclusionMacros(OcclusionMaterial::brick,OcclusionConstruction::solid,.5f,.7f,.4f,.5f);OcclusionParameters x,y;x.material=OcclusionMaterial::wood;x.construction=OcclusionConstruction::hollow;x.lpHz=a.lpHz;x.bumpHz=a.bumpHz;x.bumpDb=a.bumpDb;x.dipHz=a.dipHz;x.dipDb=a.dipDb;x.resonance=a.resonance;x.cavity=a.cavity;x.rattle=a.rattle;x.smear=a.smear;y=x;y.material=OcclusionMaterial::brick;y.construction=OcclusionConstruction::solid;y.lpHz=b.lpHz;y.bumpHz=b.bumpHz;y.bumpDb=b.bumpDb;y.dipHz=b.dipHz;y.dipDb=b.dipDb;y.resonance=b.resonance;y.cavity=b.cavity;y.rattle=b.rattle;y.smear=b.smear;OcclusionProcessor p,q;p.prepare(48000,2);q.prepare(48000,2);run(p,wood,wr,x,128);run(q,brick,br,y,128);if(difference(wood,brick)<.02f)fail("materials not distinct");if(difference(wood,wr)<.005f)fail("stereo collapsed");for(auto v:wood)if(!std::isfinite(v)||std::abs(v)>1.001f)fail("bounds");
    }
    {
        auto source=signal(n),right=signal(n,.5f),drySource=source,dryRight=right,sourceSpace=source,sourceRight=right,listenerSpace=source,listenerRight=right,sizeDark=source,sizeDarkRight=right;OcclusionParameters dry,src,lis,room;dry.roomMix=0;dry.sourceRoom=dry.listenerRoom=0;src=dry;src.roomMix=.85f;src.sourceRoom=1;lis=dry;lis.roomMix=.85f;lis.listenerRoom=1;lis.roomSize=1;lis.predelayMs=24;room=lis;room.roomSize=0;room.damp=1;OcclusionProcessor a,b,c,d;a.prepare(48000,2);b.prepare(48000,2);c.prepare(48000,2);d.prepare(48000,2);run(a,drySource,dryRight,dry,128);run(b,sourceSpace,sourceRight,src,128);run(c,listenerSpace,listenerRight,lis,128);run(d,sizeDark,sizeDarkRight,room,128);if(difference(drySource,sourceSpace)<.002f)fail("source space control is inaudible");if(difference(drySource,listenerSpace)<.002f)fail("listener space control is inaudible");if(difference(sourceSpace,listenerSpace)<.001f)fail("source and listener spaces are not distinct");if(difference(listenerSpace,sizeDark)<.001f)fail("room size and damping are inaudible");
    }
    {
        constexpr int ringSamples=4096;std::vector<float> dry(ringSamples,0),dryR(ringSamples,0),ring(ringSamples,0),ringR(ringSamples,0);dry[0]=dryR[0]=ring[0]=ringR[0]=.7f;OcclusionParameters low,high;low.construction=high.construction=OcclusionConstruction::hollow;low.cavity=low.resonance=0;high.cavity=high.resonance=1;OcclusionProcessor a,b;a.prepare(48000,2);b.prepare(48000,2);run(a,dry,dryR,low,128);run(b,ring,ringR,high,128);const auto lowTail=tailEnergy(dry,256),highTail=tailEnergy(ring,256);if(highTail<lowTail*1.2f||difference(dry,ring)<.00002f){std::cerr<<"tail low="<<lowTail<<" high="<<highTail<<" diff="<<difference(dry,ring)<<'\n';fail("cavity feedback is inaudible");}if(b.bodyActivity()<.00001f)fail("body telemetry is silent");
    }
    {
        constexpr int eventSamples=4096;std::vector<float> quiet(eventSamples,0),quietR(eventSamples,0),excited=quiet,excitedR=quietR;OcclusionParameters x;x.material=OcclusionMaterial::metal;x.construction=OcclusionConstruction::loose;x.resonance=.9f;x.cavity=.8f;x.rattle=.75f;OcclusionProcessor a,b;a.prepare(48000,2);b.prepare(48000,2);b.triggerBoundaryExcitation(.9f,eventSamples);b.triggerRattleStrike(.8f);run(a,quiet,quietR,x,128);run(b,excited,excitedR,x,128);if(difference(quiet,excited)<.0001f)fail("conducted excitation is silent");if(b.rattleActivity()<.0001f)fail("rattle strike telemetry is silent");for(auto v:excited)if(!std::isfinite(v)||std::abs(v)>1.001f)fail("excited output escaped bounds");
    }
    std::cout<<"Occlusion core passed: transparent mix, block invariance, stereo boundaries, material separation, distinct rooms, resonant cavity, conducted excitation, telemetry, bounded output\n";
}
