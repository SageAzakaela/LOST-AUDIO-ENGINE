#include <lost_audio/core/CartridgeProcessor.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace
{
using namespace lost_audio::core;
bool require(bool c,const char* m){if(!c)std::cerr<<"FAIL: "<<m<<'\n';return c;}
std::vector<float> render(std::size_t block,std::uint32_t seed,const CartridgeParameters& p,int channel=0)
{
    constexpr std::size_t n=96000;CartridgeProcessor processor;processor.prepare(48000,2);processor.reset(seed);std::vector<float> l(n),r(n);
    for(std::size_t i=0;i<n;++i){const auto t=(float)i/48000;l[i]=.16f*std::sin(2*3.14159265358979323846f*211*t)+.08f*std::sin(2*3.14159265358979323846f*1700*t);r[i]=.13f*std::sin(2*3.14159265358979323846f*347*t)+.06f*std::sin(2*3.14159265358979323846f*2600*t);}
    for(std::size_t o=0;o<n;o+=block){const auto c=std::min(block,n-o);float* ch[]{l.data()+o,r.data()+o};processor.process(ch,2,c,p);}return channel==0?l:r;
}
float difference(const std::vector<float>& a,const std::vector<float>& b){float sum=0;for(std::size_t i=0;i<a.size();++i)sum+=std::abs(a[i]-b[i]);return sum/(float)a.size();}
}
int main()
{
    bool ok=true;CartridgeParameters dry;dry.mix=0;const auto dryOut=render(257,4,dry);std::vector<float> reference(dryOut.size());for(std::size_t i=0;i<reference.size();++i){const auto t=(float)i/48000;reference[i]=.16f*std::sin(2*3.14159265358979323846f*211*t)+.08f*std::sin(2*3.14159265358979323846f*1700*t);}ok&=require(difference(dryOut,reference)<1e-7f,"zero mix must be transparent");
    CartridgeParameters damaged;damaged.codecMode=CartridgeCodec::adpcm;damaged.speakerModel=CartridgeSpeaker::handheld;damaged.bits=6;damaged.converterRateHz=9000;damaged.jitter=.6f;damaged.companding=.7f;damaged.saturation=.65f;damaged.edge=.62f;damaged.noise=.3f;damaged.speaker=.9f;damaged.room=.18f;
    const auto a=render(64,0x55eedu,damaged),b=render(997,0x55eedu,damaged),other=render(64,0x77eedu,damaged),right=render(64,0x55eedu,damaged,1);ok&=require(a==b,"output must be independent of host block size");ok&=require(difference(a,other)>1e-5f,"seed must affect converter texture");ok&=require(difference(a,right)>.01f,"stereo channels must remain distinct");ok&=require(std::all_of(a.begin(),a.end(),[](float v){return std::isfinite(v)&&std::abs(v)<=1.00001f;}),"output must be finite and bounded");
    CartridgeParameters silentBleep;silentBleep.bleepsEnabled=false;silentBleep.bleepMix=1;const auto noBleep=render(128,8,silentBleep);silentBleep.bleepMix=0;const auto stillNoBleep=render(128,8,silentBleep);ok&=require(noBleep==stillNoBleep,"bleep controls must be inert while explicitly disabled");
    CartridgeProcessor eventCore,controlCore;eventCore.prepare(48000,2);controlCore.prepare(48000,2);eventCore.reset(91);controlCore.reset(91);CartridgeParameters eventParams;eventParams.noise=eventParams.hum=eventParams.whine=0;eventParams.addressWear=0;std::vector<float> eventL(4096),eventR(4096),controlL(4096),controlR(4096);for(std::size_t i=0;i<4096;++i){const auto x=.18f*std::sin((float)i*.047f);eventL[i]=controlL[i]=x;eventR[i]=controlR[i]=-x*.7f;}float*eventChannels[]{eventL.data(),eventR.data()},*controlChannels[]{controlL.data(),controlR.data()};eventCore.process(eventChannels,2,2048,eventParams);controlCore.process(controlChannels,2,2048,eventParams);eventCore.triggerRomStall(.85f,5000,320);eventCore.triggerBankFault(.7f,4200);float*eventTail[]{eventL.data()+2048,eventR.data()+2048},*controlTail[]{controlL.data()+2048,controlR.data()+2048};eventCore.process(eventTail,2,2048,eventParams);controlCore.process(controlTail,2,2048,eventParams);ok&=require(eventCore.stallActive()&&eventCore.bankFaultActive()&&eventCore.stallProgress()>0&&eventCore.bankFaultProgress()>0,"conducted ROM and bank failures must expose progress");ok&=require(difference(eventL,controlL)>1e-4f,"ROM stall and bank faults must alter decoded playback");
    const auto clean=mapCartridgeMacros(CartridgeCodec::pcm,CartridgeSpeaker::direct,.9f,.05f,.05f,.02f),rough=mapCartridgeMacros(CartridgeCodec::dpcm,CartridgeSpeaker::pcSpeaker,.12f,.8f,.75f,.4f);ok&=require(rough.bits<clean.bits&&rough.lowPassHz<clean.lowPassHz&&rough.edge>clean.edge,"macros must span clean memory playback to damaged hardware");
    if(!ok)return 1;std::cout<<"Cartridge core passed: transparent mix, block invariance, stereo codec paths, explicit bleep ownership, conducted ROM/bank failures, bounded output\n";return 0;
}
