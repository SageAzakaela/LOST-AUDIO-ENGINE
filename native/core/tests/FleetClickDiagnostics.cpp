#include <lost_audio/core/CamcorderProcessor.h>
#include <lost_audio/core/CartridgeProcessor.h>
#include <lost_audio/core/CDProcessor.h>
#include <lost_audio/core/CommsProcessor.h>
#include <lost_audio/core/ConferenceProcessor.h>
#include <lost_audio/core/OcclusionProcessor.h>
#include <lost_audio/core/OpenMicProcessor.h>
#include <lost_audio/core/TapeProcessor.h>
#include <lost_audio/core/TelevisionProcessor.h>
#include <lost_audio/core/TransmissionProcessor.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace
{
constexpr double sampleRate=48000.0;constexpr int block=256;constexpr int samples=240000;
int unexpectedImpulseEngines=0;
struct Result{float maxDelta=0,maxImpulse=0,boundaryMax=0,rms=0;int jumps=0,impulses=0,boundaryJumps=0,firstImpulse=-1;};
Result analyse(const std::vector<float>&x){Result r;double energy=0;for(int i=48000;i<(int)x.size();++i){const auto delta=std::abs(x[(std::size_t)i]-x[(std::size_t)i-1]);const auto impulse=i>1?std::abs(x[(std::size_t)i]-2.0f*x[(std::size_t)i-1]+x[(std::size_t)i-2]):0.0f;r.maxDelta=std::max(r.maxDelta,delta);r.maxImpulse=std::max(r.maxImpulse,impulse);if(delta>.08f)++r.jumps;if(impulse>.08f){++r.impulses;if(r.firstImpulse<0)r.firstImpulse=i;}if(i%block==0){r.boundaryMax=std::max(r.boundaryMax,impulse);if(impulse>.08f)++r.boundaryJumps;}energy+=(double)x[(std::size_t)i]*x[(std::size_t)i];}r.rms=(float)std::sqrt(energy/(x.size()-48000));return r;}
template<class Process>Result render(Process&&process){std::vector<float>l(samples),r(samples);for(int i=0;i<samples;++i){const auto fade=std::min(1.0f,i/4096.0f),v=.14f*fade*std::sin(6.283185307f*997.0f*i/(float)sampleRate);l[(std::size_t)i]=r[(std::size_t)i]=v;}for(int offset=0;offset<samples;offset+=block){float*channels[]{l.data()+offset,r.data()+offset};process(channels,std::min(block,samples-offset));}return analyse(l);}
void print(const char*name,const Result&r){std::cout<<std::left<<std::setw(18)<<name<<" maxDelta="<<std::setw(10)<<r.maxDelta<<" impulses="<<std::setw(7)<<r.impulses<<" maxImpulse="<<std::setw(10)<<r.maxImpulse<<" first="<<std::setw(8)<<r.firstImpulse<<" boundaryMax="<<std::setw(10)<<r.boundaryMax<<" boundaryImpulses="<<std::setw(5)<<r.boundaryJumps<<" rms="<<r.rms<<'\n';if(r.impulses>0)++unexpectedImpulseEngines;}
}

int main()
{
    using namespace lost_audio::core;
    {TapeProcessor p;p.prepare(sampleRate,2);TapeParameters x;print("Tape",render([&](float**c,int n){p.process(c,2,(std::size_t)n,x);}));}
    {TransmissionProcessor p;p.prepare(sampleRate,2);TransmissionParameters x;print("Transmission",render([&](float**c,int n){p.process(c,2,(std::size_t)n,x);}));}
    {CDProcessor p;p.prepare(sampleRate,2);CDParameters x;print("CD",render([&](float**c,int n){p.process(c,2,(std::size_t)n,x);}));}
    {TelevisionProcessor p;p.prepare(sampleRate,2);TelevisionParameters x;print("Television",render([&](float**c,int n){p.process(c,2,(std::size_t)n,x);}));}
    {CartridgeProcessor p;p.prepare(sampleRate,2);CartridgeParameters x;print("Cartridge",render([&](float**c,int n){p.process(c,2,(std::size_t)n,x);}));}
    {CamcorderProcessor p;p.prepare(sampleRate,2);CamcorderParameters x;print("Camcorder",render([&](float**c,int n){p.process(c,2,(std::size_t)n,x);}));}
    {ConferenceProcessor p;p.prepare(sampleRate,2);ConferenceParameters x;print("Conference",render([&](float**c,int n){p.process(c,2,(std::size_t)n,x);}));}
    {CommsProcessor p;p.prepare(sampleRate,2);CommsParameters x;print("Comms",render([&](float**c,int n){p.process(c,2,(std::size_t)n,x);}));}
    {OcclusionProcessor p;p.prepare(sampleRate,2);OcclusionParameters x;print("Occlusion",render([&](float**c,int n){p.process(c,2,(std::size_t)n,x);}));}
    {OpenMicProcessor p;p.prepare(sampleRate,2);OpenMicParameters x;print("Open Mic",render([&](float**c,int n){p.process(c,2,(std::size_t)n,x);}));}
    return unexpectedImpulseEngines == 0 ? 0 : 1;
}
