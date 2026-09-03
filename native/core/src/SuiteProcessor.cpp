#include <lost_audio/core/SuiteProcessor.h>

#include <algorithm>
#include <cmath>

namespace lost_audio::core
{
namespace
{
constexpr float pi = 3.14159265358979323846f;

float timeCoefficient(float milliseconds, double sampleRate) noexcept
{
    return std::exp(-1.0f / (std::max(0.1f, milliseconds) * 0.001f * static_cast<float>(sampleRate)));
}
}

float SuiteProcessor::clamp(float value, float lo, float hi) noexcept
{
    return std::clamp(value, lo, hi);
}

int SuiteProcessor::scaledIndex(float value, int maximum) noexcept
{
    return std::clamp(static_cast<int>(std::lround(clamp(value, 0.0f, 1.0f) * static_cast<float>(maximum))), 0, maximum);
}

void SuiteProcessor::Delay::prepare(std::size_t size)
{
    samples.assign(std::max<std::size_t>(size, 2), 0.0f);
    write = 0;
}

void SuiteProcessor::Delay::reset() noexcept
{
    std::fill(samples.begin(), samples.end(), 0.0f);
    write = 0;
}

float SuiteProcessor::Delay::pushRead(float input, int delaySamples) noexcept
{
    if (samples.empty()) return input;
    delaySamples = std::clamp(delaySamples, 0, static_cast<int>(samples.size()) - 1);
    samples[write] = input;
    const auto read = (write + samples.size() - static_cast<std::size_t>(delaySamples)) % samples.size();
    const auto output = samples[read];
    write = (write + 1u) % samples.size();
    return output;
}

std::array<int, SuiteProcessor::slotCount> SuiteProcessor::sanitizeOrder(const std::array<int, slotCount>& requested) noexcept
{
    std::array<int, slotCount> result {};
    std::array<bool, slotCount> used {};
    std::size_t position = 0;
    for (const auto value : requested)
        if (value >= 0 && value < static_cast<int>(slotCount) && !used[static_cast<std::size_t>(value)])
        {
            result[position++] = value;
            used[static_cast<std::size_t>(value)] = true;
        }
    for (std::size_t i = 0; i < slotCount; ++i)
        if (!used[i]) result[position++] = static_cast<int>(i);
    return result;
}

SuiteProcessor::Topology SuiteProcessor::topologyFrom(const SuiteParameters& parameters) noexcept
{
    Topology topology;
    topology.order = sanitizeOrder(parameters.order);
    for (std::size_t i = 0; i < slotCount; ++i)
    {
        topology.engines[i] = static_cast<SuiteEngine>(std::clamp(static_cast<int>(parameters.slots[i].engine), 0, 10));
        topology.bypass[i] = parameters.slots[i].bypass || topology.engines[i] == SuiteEngine::empty;
    }
    return topology;
}

void SuiteProcessor::prepareSlot(SlotRuntime& slot, std::size_t index)
{
    slot.tape.prepare(sampleRate_, preparedChannels_);
    slot.transmission.prepare(sampleRate_, preparedChannels_);
    slot.comms.prepare(sampleRate_, preparedChannels_);
    slot.cd.prepare(sampleRate_, preparedChannels_);
    slot.conference.prepare(sampleRate_, preparedChannels_);
    slot.camcorder.prepare(sampleRate_, preparedChannels_);
    slot.cartridge.prepare(sampleRate_, preparedChannels_);
    slot.television.prepare(sampleRate_, preparedChannels_);
    slot.occlusion.prepare(sampleRate_, preparedChannels_);
    slot.openMic.prepare(sampleRate_, preparedChannels_);
    const auto maximumEngineLatency = static_cast<std::size_t>(std::ceil(sampleRate_ * 0.022)) + 8u;
    for (auto& delay : slot.dry) delay.prepare(maximumEngineLatency);
    resetSlot(slot, seed_ ^ static_cast<std::uint32_t>((index + 1u) * 0x9e3779b9u));
}

void SuiteProcessor::resetSlot(SlotRuntime& slot, std::uint32_t seed) noexcept
{
    slot.tape.reset(seed ^ 0x01u); slot.transmission.reset(seed ^ 0x02u);
    slot.comms.reset(seed ^ 0x03u); slot.cd.reset(seed ^ 0x04u);
    slot.conference.reset(seed ^ 0x05u); slot.camcorder.reset(seed ^ 0x06u);
    slot.cartridge.reset(seed ^ 0x07u); slot.television.reset(seed ^ 0x08u);
    slot.occlusion.reset(seed ^ 0x09u); slot.openMic.reset(seed ^ 0x0au);
    for (auto& delay : slot.dry) delay.reset();
}

void SuiteProcessor::prepare(double sampleRate, std::size_t channels)
{
    sampleRate_ = std::clamp(sampleRate, 8000.0, 384000.0);
    preparedChannels_ = std::clamp<std::size_t>(channels, 1, maxChannels);
    fixedLatencySamples_ = static_cast<int>(std::ceil(sampleRate_ * 0.12));
    for (std::size_t i = 0; i < slotCount; ++i) prepareSlot(slots_[i], i);
    for (auto& delay : masterDry_) delay.prepare(static_cast<std::size_t>(fixedLatencySamples_) + 8u);
    for (auto& delay : wetPad_) delay.prepare(static_cast<std::size_t>(fixedLatencySamples_) + 8u);
    reset(seed_);
}

void SuiteProcessor::reset(std::uint32_t seed) noexcept
{
    seed_ = seed;
    for (std::size_t i = 0; i < slotCount; ++i)
        resetSlot(slots_[i], seed ^ static_cast<std::uint32_t>((i + 1u) * 0x9e3779b9u));
    for (auto& delay : masterDry_) delay.reset();
    for (auto& delay : wetPad_) delay.reset();
    limiterEnvelope_.fill(0.0f); inputPeak_.fill(0.0f); outputPeak_.fill(0.0f);
    feedbackEligible_.fill(false);
    activeTopology_ = {}; pendingTopology_ = {}; topologyInitialized_ = false;
    topologyPending_ = fadingOut_ = false; topologyGain_ = 1.0f; safetyEngaged_ = false;
}

int SuiteProcessor::engineLatency(const SlotRuntime& slot, SuiteEngine engine) const noexcept
{
    switch (engine)
    {
        case SuiteEngine::tape: return slot.tape.latencySamples();
        case SuiteEngine::transmission: return slot.transmission.latencySamples();
        case SuiteEngine::cd: return slot.cd.latencySamples();
        case SuiteEngine::conference: return slot.conference.latencySamples();
        case SuiteEngine::camcorder: return slot.camcorder.latencySamples();
        default: return 0;
    }
}

void SuiteProcessor::processEngine(SlotRuntime& slot, SuiteEngine engine, float* const* data,
                                   std::size_t channels, std::size_t samples,
                                   const SuiteSlotParameters& s, float macroA, float macroB,
                                   const float* televisionBed) noexcept
{
    const auto model = clamp(s.model, 0.0f, 1.0f);
    const auto adjusted = [&s](std::size_t index, float base, float span) noexcept
    {
        return std::clamp(base + (std::clamp(s.detail[index], 0.0f, 1.0f) - .5f) * span, 0.0f, 1.0f);
    };
    switch (engine)
    {
        case SuiteEngine::tape:
        {
            const auto t = mapTapeMacros(1.0f - macroA * 0.78f, macroA, macroB, macroB * 0.72f);
            TapeParameters p; p.speed = t.speed; p.wowDepthMs = t.wowDepthMs; p.flutterDepthMs = t.flutterDepthMs;
            p.speed = clamp(t.speed * std::pow(2.0f, (s.detail[0]-.5f)*1.4f), .25f, 2.0f);
            p.wowDepthMs=std::max(0.0f,t.wowDepthMs+(s.detail[1]-.5f)*10.0f);p.flutterDepthMs=std::max(0.0f,t.flutterDepthMs+(s.detail[2]-.5f)*4.0f);
            p.wowAmount = macroB; p.drive = t.drive; p.compression = t.compression; p.hiss = adjusted(3,t.hiss,.55f); p.hum = adjusted(4,t.hum,.35f);
            p.dropout = adjusted(5,t.dropout,.8f); p.dropoutMs = t.dropoutMs; p.ceiling = t.ceiling; p.outputGain = t.outputGain;
            slot.tape.process(data, channels, samples, p); break;
        }
        case SuiteEngine::transmission:
        {
            const auto t = mapTransmissionMacros(1.0f - macroA * 0.82f, macroA, macroB, clamp(macroA * 0.35f + macroB * 0.65f, 0.0f, 1.0f));
            TransmissionParameters p; p.highPassHz=t.highPassHz;p.lowPassHz=t.lowPassHz;p.midGainDb=t.midGainDb;p.midFrequencyHz=t.midFrequencyHz;p.midQ=t.midQ;p.boxDipDb=t.boxDipDb;
            p.drive=macroA;p.asymmetry=t.asymmetry;p.compression=t.compression;p.crush=macroA*.38f;p.wowDepth=t.wowDepth;p.dropoutRate=t.dropoutRate;p.dropoutDepth=t.dropoutDepth;
            p.lowPassHz=clamp(p.lowPassHz*std::pow(2.0f,(s.detail[0]-.5f)*2.0f),500.0f,18000.0f);p.drive=adjusted(1,p.drive,.9f);p.crush=adjusted(2,p.crush,.8f);
            p.crackle=t.crackle;p.lfoRateHz=t.lfoRateHz;p.noise=adjusted(3,macroA*.12f+macroB*.22f,.7f);p.noiseColor=t.noiseColor;p.hiss=t.hiss;p.dropoutRate=adjusted(4,p.dropoutRate,.8f);p.passes=std::clamp(1+scaledIndex(model,2)+(int)std::lround((s.detail[5]-.5f)*2.0f),1,3);p.outputGain=.92f;p.mix=1.0f;
            slot.transmission.process(data, channels, samples, p); break;
        }
        case SuiteEngine::comms:
        {
            CommsParameters p; p.mode=static_cast<CommsMode>(scaledIndex(model,4));p.drive=macroA;
            const auto t=mapCommsMacros(p.mode,1.0f-macroA*.78f,macroA,macroB,macroA*.28f+macroB*.2f,macroA,macroA*.46f);
            p.highPassHz=t.highPassHz;p.lowPassHz=t.lowPassHz;p.midHumpDb=t.midHumpDb;p.midFrequencyHz=t.midFrequencyHz;p.compression=t.compression;p.bits=t.bits;p.converterRateHz=t.converterRateHz;
            p.packetLoss=t.packetLoss;p.packetLengthMs=t.packetLengthMs;p.hum=t.hum;p.hiss=t.hiss;p.toneMix=t.toneMix;p.transducer=t.transducer;p.lineAge=t.lineAge;p.duplex=t.duplex;
            p.lowPassHz=clamp(p.lowPassHz*std::pow(2.0f,(s.detail[0]-.5f)*1.7f),900.0f,12000.0f);p.drive=adjusted(1,p.drive,.9f);p.distance=adjusted(2,t.distance,1.0f);p.roomMix=adjusted(3,t.roomMix,.9f);p.hum=adjusted(4,t.hum,.55f);p.hiss=adjusted(4,t.hiss,.55f);p.speakerRattle=adjusted(5,t.speakerRattle,.8f);p.echoMix=t.echoMix;p.echoMs=t.echoMs;p.echoFeedback=t.echoFeedback;p.echoTone=t.echoTone;p.roomMs=t.roomMs;p.roomDamping=t.roomDamping;p.outputGain=t.outputGain;p.ceiling=t.ceiling;p.mix=1.0f;
            slot.comms.process(data, channels, samples, p); break;
        }
        case SuiteEngine::cd:
        {
            CDParameters p;p.mode=static_cast<CDConcealment>(scaledIndex(model,4));p.damageShape=static_cast<CDDamageShape>(scaledIndex(model,5));
            const auto t=mapCDMacros(1.0f-macroA*.72f,macroB,macroB*.72f,macroA*.18f+macroB*.82f);
            p.errorRate=t.errorRate;p.burstMs=t.burstMs;p.repeatMs=t.repeatMs;p.scratchRate=t.scratchRate;p.scratchAmount=t.scratchAmount;p.correction=t.correction;p.interpolationMs=t.interpolationMs;
            p.errorRate=adjusted(0,t.errorRate,.9f);p.burstMs=std::max(2.0f,t.burstMs+(s.detail[1]-.5f)*90.0f);p.repeatMs=std::max(4.0f,t.repeatMs+(s.detail[2]-.5f)*180.0f);p.scratchAmount=adjusted(3,t.scratchAmount,.9f);
            p.rotationHz=t.rotationHz;p.trackingRate=adjusted(4,t.trackingRate,.8f);p.trackingMs=t.trackingMs;p.servoHunt=adjusted(5,t.servoHunt,.9f);p.jitterMs=t.jitterMs;p.jitterRateHz=t.jitterRateHz;p.highFrequencyLoss=t.highFrequencyLoss;p.servoNoise=t.servoNoise;p.outputGain=t.outputGain;p.ceiling=t.ceiling;p.mix=1.0f;
            slot.cd.process(data, channels, samples, p); break;
        }
        case SuiteEngine::conference:
        {
            ConferenceParameters p;p.mode=static_cast<ConferenceMode>(scaledIndex(model,3));p.concealment=static_cast<ConferenceConcealment>(scaledIndex(model,3));
            const auto t=mapConferenceMacros(p.mode,1.0f-macroA*.68f,macroA,macroB,macroB,macroB*.72f,macroA*.2f+macroB*.2f);
            p.highPassHz=t.highPassHz;p.lowPassHz=t.lowPassHz;p.midHumpDb=t.midHumpDb;p.midFrequencyHz=t.midFrequencyHz;p.packetLoss=t.packetLoss;p.packetMs=t.packetMs;p.repeatMs=t.repeatMs;
            p.jitterMs=t.jitterMs;p.jitterRate=t.jitterRate;p.gate=t.gate;p.bits=t.bits;p.converterRateHz=t.converterRateHz;p.robot=t.robot;p.noise=t.noise;p.burstiness=t.burstiness;
            p.lowPassHz=clamp(p.lowPassHz*std::pow(2.0f,(s.detail[0]-.5f)*1.7f),1000.0f,12000.0f);p.packetLoss=adjusted(1,t.packetLoss,.9f);p.jitterMs=std::max(0.0f,t.jitterMs+(s.detail[2]-.5f)*12.0f);p.robot=adjusted(3,t.robot,.9f);p.gate=adjusted(4,t.gate,.9f);p.comfortNoise=adjusted(5,t.comfortNoise,.8f);
            p.suppression=t.suppression;p.agc=t.agc;p.bufferSlip=t.bufferSlip;p.bandwidthSwitch=t.bandwidthSwitch;p.outputGain=t.outputGain;p.ceiling=t.ceiling;p.mix=1.0f;
            slot.conference.process(data, channels, samples, p); break;
        }
        case SuiteEngine::camcorder:
        {
            CamcorderParameters p;p.format=static_cast<CamcorderFormat>(scaledIndex(model,4));p.microphone=static_cast<CameraMic>(scaledIndex(model,4));p.concealment=static_cast<CameraConcealment>(scaledIndex(model,3));
            p.coverage=1.0f-macroA*.65f;p.movement=macroB;p.corruption=macroB;p.agcDrive=macroA;
            const auto t=mapCamcorderMacros(p.format,p.microphone,p.coverage,p.movement,p.corruption,p.agcDrive);
            p.highPassHz=t.highPassHz;p.lowPassHz=t.lowPassHz;p.bodyDb=t.bodyDb;p.bodyHz=t.bodyHz;p.agcAmount=t.agcAmount;p.agcSpeed=t.agcSpeed;p.agcPump=t.agcPump;p.clip=t.clip;
            p.crush=t.crush;p.bits=t.bits;p.converterRateHz=t.converterRateHz;p.flutter=t.flutter;p.dropout=t.dropout;p.dropoutMs=t.dropoutMs;p.repeatMs=t.repeatMs;p.chirp=t.chirp;
            p.coverage=adjusted(0,p.coverage,1.0f);p.movement=adjusted(1,p.movement,1.0f);p.handling=adjusted(3,t.handling,.9f);p.rub=t.rub;p.hiss=adjusted(5,t.hiss,.7f);p.motorBleed=adjusted(4,t.motorBleed,.7f);p.outputGain=t.outputGain;p.ceiling=t.ceiling;p.windEnabled=s.detail[2]>.52f;p.windLevel=adjusted(2,.5f,1.0f);p.mix=1.0f;
            slot.camcorder.process(data, channels, samples, p); break;
        }
        case SuiteEngine::cartridge:
        {
            CartridgeParameters p;p.codecMode=static_cast<CartridgeCodec>(scaledIndex(model,4));p.speakerModel=static_cast<CartridgeSpeaker>(scaledIndex(model,4));
            const auto t=mapCartridgeMacros(p.codecMode,p.speakerModel,1.0f-macroA*.72f,macroA,macroA,macroB);
            p.bits=t.bits;p.converterRateHz=t.converterRateHz;p.jitter=t.jitter;p.lowPassHz=t.lowPassHz;p.highPassHz=t.highPassHz;p.preEmphasis=t.preEmphasis;p.companding=t.companding;p.blockMs=t.blockMs;
            p.saturation=t.saturation;p.edge=t.edge;p.dcDrift=t.dcDrift;p.hum=t.hum;p.whine=t.whine;p.noise=t.noise;p.noiseTracking=t.noiseTracking;p.speaker=t.speaker;
            p.bits=std::clamp(p.bits+(int)std::lround((s.detail[0]-.5f)*12.0f),4,16);p.converterRateHz=clamp(p.converterRateHz*std::pow(2.0f,(s.detail[1]-.5f)*2.0f),4000.0f,48000.0f);p.jitter=adjusted(2,p.jitter,.8f);p.speaker=adjusted(3,t.speaker,1.0f);p.room=adjusted(4,t.room,.8f);p.noise=adjusted(5,t.noise,.8f);
            p.microDelayMs=t.microDelayMs;p.microDelayMix=t.microDelayMix;p.roomMs=t.roomMs;p.limiter=t.limiter;p.ceiling=t.ceiling;p.outputGain=t.outputGain;p.bleepsEnabled=false;p.mix=1.0f;
            slot.cartridge.process(data, channels, samples, p); break;
        }
        case SuiteEngine::television:
        {
            TelevisionParameters p;p.model=static_cast<TelevisionModel>(scaledIndex(model,4));p.reception=static_cast<TelevisionReception>(scaledIndex(model,3));p.staticAmount=macroB*.52f;p.hum=.08f+macroA*.16f;p.whine=.025f+macroA*.08f;
            const auto t=mapTelevisionMacros(p.model,p.reception,macroA,macroA,macroA*.65f,macroB);
            p.highPassHz=t.highPassHz;p.lowPassHz=t.lowPassHz;p.midHumpDb=t.midHumpDb;p.midFrequencyHz=t.midFrequencyHz;p.drive=t.drive;p.compression=t.compression;p.noiseHiss=t.noiseHiss;
            p.noiseCrackle=t.noiseCrackle;p.tunerDrift=t.tunerDrift;p.syncInstability=t.syncInstability;p.powerSag=t.powerSag;p.cabinet=t.cabinet;p.cabinetRattle=t.cabinetRattle;
            p.lowPassHz=clamp(p.lowPassHz*std::pow(2.0f,(s.detail[0]-.5f)*1.4f),1000.0f,16000.0f);p.staticAmount=adjusted(1,p.staticAmount,.9f);p.hum=adjusted(2,p.hum,.6f);p.whine=adjusted(3,p.whine,.45f);p.cabinet=adjusted(4,t.cabinet,.9f);p.auxiliaryGain=clamp(1.0f+(s.detail[5]-.5f)*2.0f,0.0f,2.0f);
            p.limiter=t.limiter;p.ceiling=t.ceiling;p.outputGain=t.outputGain;p.mix=1.0f;slot.television.process(data, channels, samples, p, televisionBed);break;
        }
        case SuiteEngine::occlusion:
        {
            OcclusionParameters p;p.material=static_cast<OcclusionMaterial>(scaledIndex(model,7));p.construction=static_cast<OcclusionConstruction>(scaledIndex(model,4));
            const auto wall=adjusted(0,macroA,1.0f),distance=adjusted(1,macroA,1.0f),source=adjusted(2,.25f+macroB*.3f,1.0f),listener=adjusted(3,macroB,1.0f);const auto t=mapOcclusionMacros(p.material,p.construction,distance,wall,source,listener);p.sourceRoom=source;p.listenerRoom=listener;
            p.hpHz=t.hpHz;p.lpHz=t.lpHz;p.dipHz=t.dipHz;p.dipDb=t.dipDb;p.bumpHz=t.bumpHz;p.bumpDb=t.bumpDb;p.resonance=t.resonance;p.cavity=t.cavity;
            p.rattle=t.rattle;p.looseness=t.looseness;p.smear=t.smear;p.leak=adjusted(5,t.leak,.8f);p.leakTone=t.leakTone;p.roomMix=t.roomMix;p.predelayMs=t.predelayMs;p.roomSize=t.roomSize;p.damp=t.damp;p.resonance=adjusted(4,t.resonance,.9f);
            p.outputGain=t.outputGain;p.mix=1.0f;p.limiter=.45f;p.ceiling=.94f;slot.occlusion.process(data, channels, samples, p);break;
        }
        case SuiteEngine::openMicNight:
        {
            OpenMicParameters p;p.mic=static_cast<OpenMicModel>(scaledIndex(model,4));p.venue=static_cast<OpenMicVenue>(scaledIndex(model,5));p.pa=static_cast<OpenMicPA>(scaledIndex(model,4));
            const auto hotMic=adjusted(0,macroA,1.0f),room=adjusted(1,macroB,1.0f);const auto t=mapOpenMicMacros(p.mic,p.venue,p.pa,hotMic,.18f+hotMic*.58f,room);
            p.proximity=t.proximity;p.micDrive=t.micDrive;p.paDrive=t.paDrive;p.monitorLevel=t.monitorLevel;p.stageBleed=t.stageBleed;p.roomAmount=t.roomAmount;p.wallAbsorption=t.wallAbsorption;p.electricalNoise=t.electricalNoise;
            p.crowdLevel=adjusted(2,macroB*.34f,.9f);p.crowdMood=.35f+macroB*.55f;p.stageBleed=adjusted(3,t.stageBleed,.8f);p.electricalNoise=adjusted(4,t.electricalNoise,.35f);p.feedbackArmed=s.feedbackArmed;p.feedbackAmount=adjusted(5,macroB*.78f,.9f);p.feedbackFrequency=780.0f+model*2900.0f;
            p.feedbackQ=8.0f+macroB*24.0f;p.feedbackBuildMs=520.0f;p.feedbackReleaseMs=180.0f;p.mix=1.0f;slot.openMic.process(data, channels, samples, p);break;
        }
        case SuiteEngine::empty: break;
    }
}

void SuiteProcessor::processSlot(std::size_t slotIndex, SuiteEngine engine, float* const* data,
                                 std::size_t channels, std::size_t samples,
                                 const SuiteSlotParameters& parameters, float global1, float global2,
                                 const float* televisionBed) noexcept
{
    auto& runtime = slots_[slotIndex];
    const auto macroA = clamp(parameters.macroA + (global1 - .5f) * 2.0f * parameters.global1ToA + (global2 - .5f) * 2.0f * parameters.global2ToA, 0.0f, 1.0f);
    const auto macroB = clamp(parameters.macroB + (global1 - .5f) * 2.0f * parameters.global1ToB + (global2 - .5f) * 2.0f * parameters.global2ToB, 0.0f, 1.0f);
    const auto latency = engineLatency(runtime, engine);
    for (std::size_t c = 0; c < channels; ++c)
        for (std::size_t i = 0; i < samples; ++i)
            original_[c][i] = runtime.dry[c].pushRead(data[c][i], latency);
    auto safeParameters = parameters;
    if (engine == SuiteEngine::openMicNight)
    {
        if (!parameters.feedbackArmed) feedbackEligible_[slotIndex] = true;
        safeParameters.feedbackArmed = feedbackEligible_[slotIndex] && parameters.feedbackArmed;
    }
    else
    {
        feedbackEligible_[slotIndex] = false;
        safeParameters.feedbackArmed = false;
    }
    processEngine(runtime, engine, data, channels, samples, safeParameters, macroA, macroB, televisionBed);
    const auto mix = clamp(parameters.mix, 0.0f, 1.0f);
    const auto dry = std::cos(mix * pi * .5f), wet = std::sin(mix * pi * .5f);
    for (std::size_t c = 0; c < channels; ++c)
        for (std::size_t i = 0; i < samples; ++i)
            data[c][i] = original_[c][i] * dry + data[c][i] * wet;
}

void SuiteProcessor::beginTopologyChange(const Topology& topology) noexcept
{
    pendingTopology_ = topology; topologyPending_ = true; fadingOut_ = true;
}

void SuiteProcessor::applyPendingTopology() noexcept
{
    for (std::size_t i = 0; i < slotCount; ++i)
        if (activeTopology_.engines[i] != pendingTopology_.engines[i])
        {
            resetSlot(slots_[i], seed_ ^ static_cast<std::uint32_t>((i + 1u) * 0x9e3779b9u));
            feedbackEligible_[i] = false;
        }
        else if (activeTopology_.bypass[i] != pendingTopology_.bypass[i])
        {
            feedbackEligible_[i] = false;
        }
    activeTopology_ = pendingTopology_; topologyPending_ = false; fadingOut_ = false; topologyGain_ = 0.0f;
}

float SuiteProcessor::inputPeak(std::size_t channel) const noexcept { return inputPeak_[std::min(channel, maxChannels - 1)]; }
float SuiteProcessor::outputPeak(std::size_t channel) const noexcept { return outputPeak_[std::min(channel, maxChannels - 1)]; }

void SuiteProcessor::process(float* const* data, std::size_t channelCount,
                             std::size_t sampleCount, const SuiteParameters& parameters,
                             const float* televisionBed) noexcept
{
    const auto channels = std::min({ channelCount, preparedChannels_, maxChannels });
    if (channels == 0 || data == nullptr) return;
    const auto requested = topologyFrom(parameters);
    if (!topologyInitialized_) { activeTopology_ = requested; topologyInitialized_ = true; }
    else if (!(requested == (topologyPending_ ? pendingTopology_ : activeTopology_))) beginTopologyChange(requested);
    inputPeak_.fill(0.0f); outputPeak_.fill(0.0f); safetyEngaged_ = false;
    const auto global1 = clamp(parameters.globalMacro1, 0.0f, 1.0f), global2 = clamp(parameters.globalMacro2, 0.0f, 1.0f);
    const auto globalMix = clamp(parameters.mix, 0.0f, 1.0f);
    const auto masterDryGain = std::cos(globalMix * pi * .5f), masterWetGain = std::sin(globalMix * pi * .5f);
    const auto ceiling = clamp(parameters.ceiling, .25f, .99f);
    const auto attack = timeCoefficient(1.8f, sampleRate_), release = timeCoefficient(120.0f, sampleRate_);
    const auto topologyStep = 1.0f / static_cast<float>(std::max(1.0, sampleRate_ * .004));

    for (std::size_t offset = 0; offset < sampleCount; offset += chunkSize)
    {
        if (topologyPending_ && topologyGain_ <= .0001f) applyPendingTopology();
        const auto count = std::min(chunkSize, sampleCount - offset);
        std::array<float*, maxChannels> chunk {};
        for (std::size_t c = 0; c < channels; ++c)
        {
            chunk[c] = data[c] + offset;
            for (std::size_t i = 0; i < count; ++i)
            {
                const auto input = chunk[c][i]; inputPeak_[c] = std::max(inputPeak_[c], std::abs(input));
                const auto gained = input * clamp(parameters.inputGain, 0.0f, 2.0f);
                masterInput_[c][i] = gained; chunk[c][i] = gained;
            }
        }

        int chainLatency = 0;
        for (const auto ordered : activeTopology_.order)
        {
            const auto slotIndex = static_cast<std::size_t>(ordered);
            if (activeTopology_.bypass[slotIndex]) continue;
            const auto engine = activeTopology_.engines[slotIndex];
            processSlot(slotIndex, engine, chunk.data(), channels, count, parameters.slots[slotIndex], global1, global2,
                        televisionBed != nullptr ? televisionBed + offset : nullptr);
            chainLatency += engineLatency(slots_[slotIndex], engine);
        }
        const auto pad = std::clamp(fixedLatencySamples_ - chainLatency, 0, fixedLatencySamples_);
        for (std::size_t i = 0; i < count; ++i)
        {
            if (fadingOut_) topologyGain_ = std::max(0.0f, topologyGain_ - topologyStep);
            else topologyGain_ = std::min(1.0f, topologyGain_ + topologyStep);
            for (std::size_t c = 0; c < channels; ++c)
            {
                const auto dry = masterDry_[c].pushRead(masterInput_[c][i], fixedLatencySamples_);
                const auto wet = wetPad_[c].pushRead(chunk[c][i], pad);
                auto output = (dry * masterDryGain + wet * masterWetGain) * clamp(parameters.outputGain, 0.0f, 1.5f) * topologyGain_;
                const auto magnitude = std::abs(output);
                const auto coefficient = magnitude > limiterEnvelope_[c] ? attack : release;
                limiterEnvelope_[c] = magnitude + coefficient * (limiterEnvelope_[c] - magnitude);
                const auto reduction = limiterEnvelope_[c] > ceiling ? ceiling / (limiterEnvelope_[c] + 1.0e-8f) : 1.0f;
                if (reduction < .999f) safetyEngaged_ = true;
                const auto limited = clamp(output * reduction, -ceiling, ceiling);
                output += (limited - output) * clamp(parameters.limiter, 0.0f, 1.0f);
                output = clamp(output, -1.0f, 1.0f); chunk[c][i] = output;
                outputPeak_[c] = std::max(outputPeak_[c], std::abs(output));
            }
        }
    }
}
} // namespace lost_audio::core
