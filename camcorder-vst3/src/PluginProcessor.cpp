#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <BinaryData.h>
#include <lost_audio/core/TempoSync.h>

#include <algorithm>
#include <cmath>

namespace { float clamp01(float value) noexcept { return juce::jlimit(0.0f, 1.0f, value); } }

CamcorderEngineAudioProcessor::CamcorderEngineAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
    apvts.state.setProperty("engineId", "camcorder", nullptr); apvts.state.setProperty("schemaVersion", 3, nullptr);
}

juce::AudioProcessorValueTreeState::ParameterLayout CamcorderEngineAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p; const auto n01 = juce::NormalisableRange<float>(0.0f, 1.0f, .001f);
    // Preserve the V1 IDs and ordering for automation and project recall.
    p.push_back(std::make_unique<juce::AudioParameterFloat>("coverage", "Coverage", n01, .35f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("movement", "Movement", n01, .25f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("corruption", "Corruption", n01, .18f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("agc", "AGC Drive", n01, .35f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("wind", "Wind", false));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("windLevel", "Wind Level", juce::NormalisableRange<float>(0.0f, 1.5f, .001f), .80f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("hpHz", "High-pass", juce::NormalisableRange<float>(10.0f, 1200.0f, 1.0f), 96.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("lpHz", "Low-pass", juce::NormalisableRange<float>(800.0f, 22000.0f, 1.0f), 10492.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("boxDb", "Body Resonance", juce::NormalisableRange<float>(0.0f, 14.0f, .1f), 4.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("boxHz", "Body Frequency", juce::NormalisableRange<float>(650.0f, 4200.0f, 1.0f), 1846.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("agcAmt", "AGC Amount", n01, .519f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("agcSpeed", "AGC Speed", n01, .385f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("clip", "Preamp Clip", n01, .239f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("crush", "Converter Damage", n01, .098f));
    p.push_back(std::make_unique<juce::AudioParameterInt>("bits", "Converter Bits", 4, 16, 13));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("rate", "Converter Rate", juce::NormalisableRange<float>(8000.0f, 48000.0f, 1.0f), 29608.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("drop", "Dropouts", n01, .085f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("dropMs", "Dropout Length", juce::NormalisableRange<float>(1.0f, 500.0f, 1.0f), 25.0f));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("dropMode", "Concealment", juce::StringArray { "Hold", "Mute", "Interpolate", "Repeat" }, 0));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("repeatMs", "Repeat Length", juce::NormalisableRange<float>(1.0f, 600.0f, 1.0f), 38.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("chirp", "Codec Chirp", n01, .065f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("handling", "Handling", n01, .168f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("rub", "Body Rub", n01, .132f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("hiss", "Mic Hiss", n01, .138f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("ceiling", "Ceiling", juce::NormalisableRange<float>(.2f, 1.0f, .001f), .914f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("outGain", "Output", juce::NormalisableRange<float>(0.0f, 1.5f, .01f), .97f));
    // V2 additions are appended; Surface mapping never rewrites Wind or Concealment.
    p.push_back(std::make_unique<juce::AudioParameterBool>("macroLink", "Legacy Macro Link", false));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("format", "Recording Format", juce::StringArray { "VHS-C", "Video8 / Hi8", "MiniDV", "Digicam", "Action Cam" }, 2));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("micModel", "Camera Microphone", juce::StringArray { "Electret", "Cheap Mono", "Stereo Capsule", "Waterproof", "Shotgun" }, 0));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("agcPump", "AGC Pump", n01, .358f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("flutter", "Transport Flutter", n01, .155f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("motorBleed", "Motor Bleed", n01, .155f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("inputGain", "Input Gain", juce::NormalisableRange<float>(-24.0f, 24.0f, .1f), 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Mix", n01, 1.0f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("camBedEnable", "Captured Camera Bed", false));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("camBedLevel", "Camera Bed Level", n01, .24f));
    const juce::StringArray divisions { "1 Bar", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T", "1/8D", "1/16D" };
    p.push_back(std::make_unique<juce::AudioParameterBool>("dropTempoSync", "Clock Dropouts", false));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("dropDivision", "Dropout Grid", divisions, 2));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("dropProbability", "Dropout Probability", n01, .40f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("dropLengthSync", "Clock Drop Length", false));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("dropLengthDivision", "Dropout Length Grid", divisions, 4));
    p.push_back(std::make_unique<juce::AudioParameterBool>("faultTempoSync", "Clock Codec Faults", false));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("faultDivision", "Codec Fault Grid", divisions, 3));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("faultProbability", "Codec Fault Probability", n01, .30f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("faultStrength", "Codec Fault Strength", n01, .65f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("faultDurationMs", "Codec Fault Length", juce::NormalisableRange<float>(4.0f, 800.0f, 1.0f), 80.0f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("faultLengthSync", "Clock Fault Length", false));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("faultLengthDivision", "Codec Fault Length Grid", divisions, 4));
    p.push_back(std::make_unique<juce::AudioParameterBool>("handlingTempoSync", "Clock Body Hits", false));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("handlingDivision", "Body Hit Grid", divisions, 2));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("handlingProbability", "Body Hit Probability", n01, .35f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("handlingStrength", "Body Hit Strength", n01, .55f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("flutterTempoSync", "Clock Flutter", false));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("flutterDivision", "Flutter Cycle", divisions, 0));
    return { p.begin(), p.end() };
}

float CamcorderEngineAudioProcessor::value(const char* id) const noexcept
{
    if (const auto* raw = apvts.getRawParameterValue(id)) return raw->load(std::memory_order_relaxed);
    return 0.0f;
}
bool CamcorderEngineAudioProcessor::legacyMacrosActive() const noexcept { return value("macroLink") > .5f; }
void CamcorderEngineAudioProcessor::materialiseLegacyMacros()
{
    if (!legacyMacrosActive()) return;
    const auto format = (lost_audio::core::CamcorderFormat) juce::jlimit(0, 4, (int) std::lround(value("format")));
    const auto mic = (lost_audio::core::CameraMic) juce::jlimit(0, 4, (int) std::lround(value("micModel")));
    const auto t = lost_audio::core::mapCamcorderMacros(format, mic, value("coverage"), value("movement"), value("corruption"), value("agc"));
    const auto set = [this] (const char* id, float plain) { if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(plain)); };
    set("hpHz", t.highPassHz); set("lpHz", t.lowPassHz); set("boxDb", t.bodyDb); set("boxHz", t.bodyHz);
    set("agcAmt", t.agcAmount); set("agcSpeed", t.agcSpeed); set("agcPump", t.agcPump); set("clip", t.clip);
    set("crush", t.crush); set("bits", (float) t.bits); set("rate", t.converterRateHz); set("flutter", t.flutter);
    set("drop", t.dropout); set("dropMs", t.dropoutMs); set("repeatMs", t.repeatMs); set("chirp", t.chirp);
    set("handling", t.handling); set("rub", t.rub); set("hiss", t.hiss); set("motorBleed", t.motorBleed);
    set("outGain", t.outputGain); set("ceiling", t.ceiling); set("macroLink", 0.0f);
}
std::array<float, 64> CamcorderEngineAudioProcessor::outputTrace() const noexcept
{
    std::array<float, 64> result {}; for (std::size_t i = 0; i < result.size(); ++i) result[i] = trace[i].load(std::memory_order_relaxed); return result;
}
float CamcorderEngineAudioProcessor::syncedDuration(const char* syncId, const char* divisionId, const char* millisecondsId, double bpm) const noexcept
{
    if (value(syncId) > .5f) return lost_audio::core::tempoDivisionMilliseconds(bpm, (int) value(divisionId)) * .001f;
    return value(millisecondsId) * .001f;
}

const juce::String CamcorderEngineAudioProcessor::getName() const { return JucePlugin_Name; }
bool CamcorderEngineAudioProcessor::acceptsMidi() const { return false; }
bool CamcorderEngineAudioProcessor::producesMidi() const { return false; }
bool CamcorderEngineAudioProcessor::isMidiEffect() const { return false; }
double CamcorderEngineAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int CamcorderEngineAudioProcessor::getNumPrograms() { return 1; }
int CamcorderEngineAudioProcessor::getCurrentProgram() { return 0; }
void CamcorderEngineAudioProcessor::setCurrentProgram(int) {}
const juce::String CamcorderEngineAudioProcessor::getProgramName(int) { return {}; }
void CamcorderEngineAudioProcessor::changeProgramName(int, const juce::String&) {}

std::vector<float> CamcorderEngineAudioProcessor::decodeBed(const void* data, std::size_t bytes, double targetRate) const
{
    if (data == nullptr || bytes == 0 || targetRate <= 0.0) return {};
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    auto stream = std::make_unique<juce::MemoryInputStream>(data, bytes, false);
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(std::move(stream)));
    if (!reader || reader->lengthInSamples <= 0 || reader->numChannels == 0) return {};
    const auto length = (int) reader->lengthInSamples;
    juce::AudioBuffer<float> source((int) reader->numChannels, length); reader->read(&source, 0, length, 0, true, true);
    const auto ratio = targetRate / reader->sampleRate;
    const auto outputLength = juce::jmax(1, (int) std::floor(length * ratio));
    std::vector<float> output((std::size_t) outputLength);
    for (int i = 0; i < outputLength; ++i)
    {
        const auto position = (float) i / (float) ratio; const auto a = juce::jlimit(0, length - 1, (int) position), b = juce::jlimit(0, length - 1, a + 1); const auto fraction = position - (float) a;
        float sample = 0; for (unsigned channel = 0; channel < reader->numChannels; ++channel) sample += source.getSample((int) channel, a) + (source.getSample((int) channel, b) - source.getSample((int) channel, a)) * fraction;
        output[(std::size_t) i] = sample / (float) reader->numChannels;
    }
    return output;
}

float CamcorderEngineAudioProcessor::readBed(const std::vector<float>& bed, float& position) const noexcept
{
    if (bed.empty()) return 0.0f; const auto a = (std::size_t) position; const auto b = (a + 1u) % bed.size(); const auto fraction = position - (float) a;
    const auto sample = bed[a] + (bed[b] - bed[a]) * fraction; position += 1.0f; if (position >= (float) bed.size()) position -= (float) bed.size(); return sample;
}

void CamcorderEngineAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock); camcorderCore.prepare(sampleRate, (std::size_t) juce::jlimit(1, 2, getTotalNumInputChannels()));
    camcorderCore.reset(0x43414d45u); setLatencySamples(camcorderCore.latencySamples());
    const void* cameraData[] { BinaryData::camerabed1_wav, BinaryData::camerabed2_wav,
                               BinaryData::camerabed3_wav, BinaryData::camerabed4_wav };
    const int cameraBytes[] { BinaryData::camerabed1_wavSize, BinaryData::camerabed2_wavSize,
                              BinaryData::camerabed3_wavSize, BinaryData::camerabed4_wavSize };
    const void* windData[] { BinaryData::windbed1_wav, BinaryData::windbed2_wav,
                             BinaryData::windbed3_wav, BinaryData::windbed4_wav };
    const int windBytes[] { BinaryData::windbed1_wavSize, BinaryData::windbed2_wavSize,
                            BinaryData::windbed3_wavSize, BinaryData::windbed4_wavSize };
    for (int i = 0; i < 4; ++i)
    {
        cameraBeds[(std::size_t) i] = decodeBed(cameraData[i], (std::size_t) cameraBytes[i], sampleRate);
        windBeds[(std::size_t) i] = decodeBed(windData[i], (std::size_t) windBytes[i], sampleRate);
    }
    cameraBedPositions.fill(0.0f); windBedPositions.fill(0.0f); bedChunk.resize((std::size_t) juce::jmax(1, samplesPerBlock));
    for (auto& peak : inputPeaks) peak.store(0); for (auto& peak : outputPeaks) peak.store(0);
    for (auto& point : trace) point.store(0); pendingDropTrigger.store(false); pendingFaultTrigger.store(false); pendingHandlingTrigger.store(false); currentBpm = 120.0;
}
void CamcorderEngineAudioProcessor::releaseResources() {}
bool CamcorderEngineAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet(); return input == layouts.getMainOutputChannelSet()
        && (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo());
}

lost_audio::core::CamcorderParameters CamcorderEngineAudioProcessor::readParameters(double bpm) const noexcept
{
    const auto value = [this](const char* id) { return apvts.getRawParameterValue(id)->load(); };
    lost_audio::core::CamcorderParameters p; p.format = (lost_audio::core::CamcorderFormat) juce::jlimit(0, 4, (int) std::lround(value("format")));
    p.microphone = (lost_audio::core::CameraMic) juce::jlimit(0, 4, (int) std::lround(value("micModel")));
    p.concealment = (lost_audio::core::CameraConcealment) juce::jlimit(0, 3, (int) std::lround(value("dropMode")));
    p.coverage = clamp01(value("coverage")); p.movement = clamp01(value("movement")); p.corruption = clamp01(value("corruption")); p.agcDrive = clamp01(value("agc"));
    p.windEnabled = value("wind") > .5f; p.windLevel = juce::jlimit(0.0f, 1.5f, value("windLevel"));
    p.inputGain = juce::Decibels::decibelsToGain(value("inputGain")); p.mix = clamp01(value("mix"));
    if (value("macroLink") > .5f)
    {
        const auto t = lost_audio::core::mapCamcorderMacros(p.format, p.microphone, p.coverage, p.movement, p.corruption, p.agcDrive);
        p.highPassHz = t.highPassHz; p.lowPassHz = t.lowPassHz; p.bodyDb = t.bodyDb; p.bodyHz = t.bodyHz;
        p.agcAmount = t.agcAmount; p.agcSpeed = t.agcSpeed; p.agcPump = t.agcPump; p.clip = t.clip; p.crush = t.crush; p.bits = t.bits;
        p.converterRateHz = t.converterRateHz; p.flutter = t.flutter; p.dropout = t.dropout; p.dropoutMs = t.dropoutMs; p.repeatMs = t.repeatMs; p.chirp = t.chirp;
        p.handling = t.handling; p.rub = t.rub; p.hiss = t.hiss; p.motorBleed = t.motorBleed;
        p.outputGain = juce::jlimit(0.0f, 1.5f, t.outputGain * value("outGain") / .98f); p.ceiling = t.ceiling;
    }
    else
    {
        p.highPassHz = value("hpHz"); p.lowPassHz = value("lpHz"); p.bodyDb = value("boxDb"); p.bodyHz = value("boxHz");
        p.agcAmount = value("agcAmt"); p.agcSpeed = value("agcSpeed"); p.agcPump = value("agcPump"); p.clip = value("clip");
        p.crush = value("crush"); p.bits = (int) std::lround(value("bits")); p.converterRateHz = value("rate"); p.flutter = value("flutter");
        p.dropout = value("drop"); p.dropoutMs = value("dropMs"); p.repeatMs = value("repeatMs"); p.chirp = value("chirp");
        p.handling = value("handling"); p.rub = value("rub"); p.hiss = value("hiss"); p.motorBleed = value("motorBleed");
        p.outputGain = value("outGain"); p.ceiling = value("ceiling");
    }
    if (value("dropTempoSync") > .5f) p.dropout = 0.0f;
    if (value("faultTempoSync") > .5f) p.chirp = 0.0f;
    if (value("handlingTempoSync") > .5f) p.handling = 0.0f;
    if (value("flutterTempoSync") > .5f) p.flutterRateHz = lost_audio::core::tempoDivisionRateHz(bpm, (int) value("flutterDivision"));
    return p;
}

void CamcorderEngineAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi); juce::ScopedNoDenormals noDenormals; const auto channels = juce::jlimit(0, 2, getTotalNumInputChannels());
    for (int channel = channels; channel < getTotalNumOutputChannels(); ++channel) buffer.clear(channel, 0, buffer.getNumSamples());
    if (channels == 0 || getSampleRate() <= 0 || buffer.getNumSamples() <= 0) return;
    const auto sampleCount = buffer.getNumSamples();
    for (int channel = 0; channel < channels; ++channel) inputPeaks[(std::size_t) channel].store(buffer.getMagnitude(channel, 0, sampleCount));
    if (channels == 1) inputPeaks[1].store(inputPeaks[0].load());

    bool playing = false, hasPpq = false; double bpm = currentBpm, ppq = 0.0;
    if (auto* head = getPlayHead()) if (const auto position = head->getPosition())
    {
        playing = position->getIsPlaying(); if (const auto hostBpm = position->getBpm()) bpm = *hostBpm;
        if (const auto hostPpq = position->getPpqPosition()) { ppq = *hostPpq; hasPpq = true; }
    }
    currentBpm = juce::jlimit(20.0, 400.0, bpm);
    lost_audio::core::TempoEventSchedule dropSchedule, faultSchedule, handlingSchedule;
    if (playing && hasPpq && value("dropTempoSync") > .5f) dropSchedule = lost_audio::core::tempoEventsInBlock(ppq, currentBpm, (int) value("dropDivision"), getSampleRate(), sampleCount);
    if (playing && hasPpq && value("faultTempoSync") > .5f) faultSchedule = lost_audio::core::tempoEventsInBlock(ppq, currentBpm, (int) value("faultDivision"), getSampleRate(), sampleCount);
    if (playing && hasPpq && value("handlingTempoSync") > .5f) handlingSchedule = lost_audio::core::tempoEventsInBlock(ppq, currentBpm, (int) value("handlingDivision"), getSampleRate(), sampleCount);

    const auto parameters = readParameters(currentBpm); const auto cameraIndex = juce::jlimit(0, 3, (int) parameters.format % 4);
    const auto windIndex = juce::jlimit(0, 3, ((int) parameters.format + (int) parameters.microphone) % 4);
    const auto cameraBedEnabled = value("camBedEnable") > .5f;
    const auto cameraBedLevel = clamp01(value("camBedLevel"));
    if (bedChunk.size() < (std::size_t) sampleCount) bedChunk.resize((std::size_t) sampleCount);
    auto maxCameraBed = 0.0f, maxWindBed = 0.0f;
    for (int i = 0; i < sampleCount; ++i)
    {
        const auto camera = cameraBedEnabled ? readBed(cameraBeds[(std::size_t) cameraIndex], cameraBedPositions[(std::size_t) cameraIndex]) * cameraBedLevel * .36f : 0.0f;
        const auto wind = parameters.windEnabled ? readBed(windBeds[(std::size_t) windIndex], windBedPositions[(std::size_t) windIndex]) * juce::jlimit(0.0f, 1.5f, parameters.windLevel) * .10f : 0.0f;
        maxCameraBed = std::max(maxCameraBed, std::abs(camera)); maxWindBed = std::max(maxWindBed, std::abs(wind)); bedChunk[(std::size_t) i] = juce::jlimit(-.35f, .35f, camera + wind);
    }

    std::array<int, 100> boundaries {}; int boundaryCount = 0; boundaries[(std::size_t) boundaryCount++] = 0; boundaries[(std::size_t) boundaryCount++] = sampleCount;
    for (std::size_t i = 0; i < dropSchedule.size; ++i) boundaries[(std::size_t) boundaryCount++] = dropSchedule.events[i].sampleOffset;
    for (std::size_t i = 0; i < faultSchedule.size; ++i) boundaries[(std::size_t) boundaryCount++] = faultSchedule.events[i].sampleOffset;
    for (std::size_t i = 0; i < handlingSchedule.size; ++i) boundaries[(std::size_t) boundaryCount++] = handlingSchedule.events[i].sampleOffset;
    std::sort(boundaries.begin(), boundaries.begin() + boundaryCount); boundaryCount = (int) std::distance(boundaries.begin(), std::unique(boundaries.begin(), boundaries.begin() + boundaryCount));
    auto fireDrop = pendingDropTrigger.exchange(false), fireFault = pendingFaultTrigger.exchange(false), fireHandling = pendingHandlingTrigger.exchange(false);
    auto maxAgc = 0.0f, maxFlutter = 0.0f, maxLimiter = 0.0f;
    for (int boundary = 0; boundary < boundaryCount - 1; ++boundary)
    {
        const auto offset = boundaries[(std::size_t) boundary];
        for (std::size_t i = 0; i < dropSchedule.size; ++i) if (dropSchedule.events[i].sampleOffset == offset && lost_audio::core::tempoEventDecision(dropSchedule.events[i].stepIndex, value("dropProbability"), 0x43414d44524f50ull)) fireDrop = true;
        for (std::size_t i = 0; i < faultSchedule.size; ++i) if (faultSchedule.events[i].sampleOffset == offset && lost_audio::core::tempoEventDecision(faultSchedule.events[i].stepIndex, value("faultProbability"), 0x43414d4641554c54ull)) fireFault = true;
        for (std::size_t i = 0; i < handlingSchedule.size; ++i) if (handlingSchedule.events[i].sampleOffset == offset && lost_audio::core::tempoEventDecision(handlingSchedule.events[i].stepIndex, value("handlingProbability"), 0x43414d484954ull)) fireHandling = true;
        if (fireDrop && !camcorderCore.dropoutActive()) camcorderCore.triggerDropout(syncedDuration("dropLengthSync", "dropLengthDivision", "dropMs", currentBpm));
        if (fireFault && !camcorderCore.corruptionActive()) camcorderCore.triggerCodecFault(value("faultStrength"), syncedDuration("faultLengthSync", "faultLengthDivision", "faultDurationMs", currentBpm));
        if (fireHandling && !camcorderCore.handlingActive()) camcorderCore.triggerHandling(value("handlingStrength"));
        fireDrop = fireFault = fireHandling = false;
        const auto count = boundaries[(std::size_t) boundary + 1] - offset; if (count <= 0) continue;
        float* pointers[] { buffer.getWritePointer(0) + offset, channels > 1 ? buffer.getWritePointer(1) + offset : nullptr };
        camcorderCore.process(pointers, (std::size_t) channels, (std::size_t) count, parameters, bedChunk.data() + offset);
        maxAgc = std::max(maxAgc, camcorderCore.agcActivity()); maxFlutter = std::max(maxFlutter, camcorderCore.flutterActivity()); maxLimiter = std::max(maxLimiter, camcorderCore.limiterActivity());
    }
    for (int channel = 0; channel < channels; ++channel) outputPeaks[(std::size_t) channel].store(buffer.getMagnitude(channel, 0, sampleCount));
    if (channels == 1) outputPeaks[1].store(outputPeaks[0].load());
    for (int i = 0; i < (int) trace.size(); ++i) { const auto first = i * sampleCount / (int) trace.size(), last = juce::jmax(first + 1, (i + 1) * sampleCount / (int) trace.size()); trace[(std::size_t) i].store(buffer.getRMSLevel(0, first, juce::jmin(sampleCount, last) - first)); }
    windState.store(camcorderCore.windActive()); handlingState.store(camcorderCore.handlingActive()); dropoutState.store(camcorderCore.dropoutActive()); corruptionState.store(camcorderCore.corruptionActive());
    dropoutProgressMeter.store(camcorderCore.dropoutProgress()); corruptionProgressMeter.store(camcorderCore.corruptionProgress()); handlingProgressMeter.store(camcorderCore.handlingProgress()); windProgressMeter.store(camcorderCore.windProgress());
    agcMeter.store(maxAgc); flutterMeter.store(maxFlutter); limiterMeter.store(maxLimiter); cameraBedMeter.store(juce::jlimit(0.0f, 1.0f, maxCameraBed * 8.0f)); windBedMeter.store(juce::jlimit(0.0f, 1.0f, maxWindBed * 8.0f));
}

bool CamcorderEngineAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* CamcorderEngineAudioProcessor::createEditor() { return new CamcorderEngineAudioProcessorEditor(*this); }
void CamcorderEngineAudioProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    auto state = apvts.copyState(); state.setProperty("engineId", "camcorder", nullptr); state.setProperty("schemaVersion", 3, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, dest);
}
void CamcorderEngineAudioProcessor::setStateInformation(const void* data, int size)
{ if (auto xml = getXmlFromBinary(data, size)) if (xml->hasTagName(apvts.state.getType())) { apvts.replaceState(juce::ValueTree::fromXml(*xml)); apvts.state.setProperty("engineId", "camcorder", nullptr); apvts.state.setProperty("schemaVersion", 3, nullptr); } }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CamcorderEngineAudioProcessor(); }
