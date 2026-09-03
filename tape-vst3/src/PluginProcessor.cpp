#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <BinaryData.h>

namespace
{
float clampf(float x, float lo, float hi)
{
    return juce::jlimit(lo, hi, x);
}

std::vector<float> resampleLinear(const std::vector<float>& in, double srcRate, double dstRate)
{
    if (in.empty() || srcRate <= 1000.0 || dstRate <= 1000.0)
        return {};
    if (std::abs(srcRate - dstRate) < 1.0)
        return in;

    const auto outLen = juce::jmax(1, (int) std::llround((double) in.size() * dstRate / srcRate));
    std::vector<float> out((size_t) outLen, 0.0f);
    const auto step = srcRate / dstRate;
    double pos = 0.0;
    for (int i = 0; i < outLen; ++i)
    {
        const auto i0 = juce::jlimit(0, (int) in.size() - 1, (int) pos);
        const auto i1 = juce::jlimit(0, (int) in.size() - 1, i0 + 1);
        const auto frac = (float) (pos - (double) i0);
        out[(size_t) i] = in[(size_t) i0] + (in[(size_t) i1] - in[(size_t) i0]) * frac;
        pos += step;
        if (pos > (double) in.size() - 1.0)
            pos = (double) in.size() - 1.0;
    }
    return out;
}
}

TapeEngineAudioProcessor::TapeEngineAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout()),
      rng(0x74617065)
{
    apvts.state.setProperty("engineId", "tape", nullptr);
    apvts.state.setProperty("schemaVersion", 5, nullptr);
}

juce::AudioProcessorValueTreeState::ParameterLayout TapeEngineAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto n01 = juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f);

    p.push_back(std::make_unique<juce::AudioParameterFloat>("quality", "Quality", n01, 0.55f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("age", "Age", n01, 0.35f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("wow", "Wow", n01, 0.25f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("glitch", "Glitch", n01, 0.18f));

    // The original four macros remain at their historical indices so old
    // sessions load exactly. New sessions author the resolved DSP state below.
    p.push_back(std::make_unique<juce::AudioParameterFloat>("hpHz", "High-pass", juce::NormalisableRange<float>(10.0f, 240.0f, 1.0f), 54.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("lpHz", "Low-pass", juce::NormalisableRange<float>(1200.0f, 22000.0f, 1.0f), 12759.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("headBumpDb", "Head Bump dB", juce::NormalisableRange<float>(0.0f, 12.0f, 0.1f), 2.95f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("headBumpHz", "Head Bump Hz", juce::NormalisableRange<float>(30.0f, 220.0f, 1.0f), 82.0f));

    p.push_back(std::make_unique<juce::AudioParameterFloat>("speed", "Speed", juce::NormalisableRange<float>(0.5f, 2.0f, 0.001f), 0.992f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("wowDepthMs", "Wow Depth", juce::NormalisableRange<float>(0.0f, 20.0f, 0.1f), 3.3f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("flutterDepthMs", "Flutter Depth", juce::NormalisableRange<float>(0.0f, 10.0f, 0.1f), 1.2f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("drive", "Drive", n01, 0.42f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("comp", "Compression", n01, 0.255f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("hiss", "Hiss", n01, 0.102f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("hum", "Hum", n01, 0.030f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("dropout", "Dropout", n01, 0.078f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("dropoutMs", "Dropout Length", juce::NormalisableRange<float>(1.0f, 400.0f, 1.0f), 32.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("ceiling", "Ceiling", juce::NormalisableRange<float>(0.2f, 1.0f, 0.001f), 0.904f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("outGain", "Output Gain", juce::NormalisableRange<float>(0.0f, 1.5f, 0.01f), 1.01f));

    p.push_back(std::make_unique<juce::AudioParameterBool>("sfxEnable", "SFX Enable", false));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("sfxBank", "SFX Bank", juce::StringArray { "Cassette", "VHS" }, 0));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("sfxMode", "SFX Mode", juce::StringArray { "Bed", "Edges", "Sequence" }, 0));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("sfxLevel", "SFX Level", n01, 0.22f));

    // Appended to preserve the indices of every parameter shipped before V3.
    // Surface mode is evaluated in the processor so host automation works even
    // when the editor is closed. Moving an Advanced control disables the link.
    p.push_back(std::make_unique<juce::AudioParameterBool>("macroLink", "Legacy Macro Link", false));

    // Appended so existing host automation and saved states keep their indices.
    p.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Mix", n01, 1.0f));

    // V5 performer parameters are append-only for host automation compatibility.
    const juce::StringArray divisions { "1 Bar", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T", "1/8D", "1/16D" };
    p.push_back(std::make_unique<juce::AudioParameterBool>("dropoutTempoSync", "Clock Sync Dropouts", false));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("dropoutDivision", "Dropout Trigger Grid", divisions, 2));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("dropoutProbability", "Dropout Probability", n01, 0.45f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("dropoutStrength", "Dropout Strength", n01, 0.72f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("dropoutLengthSync", "Sync Dropout Length", false));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("dropoutLengthDivision", "Dropout Length", divisions, 4));
    p.push_back(std::make_unique<juce::AudioParameterBool>("wowTempoSync", "Clock Sync Wow", false));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("wowDivision", "Wow Cycle", divisions, 0));
    p.push_back(std::make_unique<juce::AudioParameterBool>("flutterTempoSync", "Clock Sync Flutter", false));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("flutterDivision", "Flutter Cycle", divisions, 5));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("transportDrift", "Transport Drift", n01, 0.25f));

    return { p.begin(), p.end() };
}

float TapeEngineAudioProcessor::value(const char* id) const noexcept
{
    if (const auto* raw = apvts.getRawParameterValue(id))
        return raw->load(std::memory_order_relaxed);
    return 0.0f;
}

bool TapeEngineAudioProcessor::legacyMacrosActive() const noexcept
{
    return value("macroLink") > 0.5f;
}

void TapeEngineAudioProcessor::materialiseLegacyMacros()
{
    if (!legacyMacrosActive())
        return;
    const auto targets = lost_audio::core::mapTapeMacros(value("quality"), value("age"), value("wow"), value("glitch"));
    const auto set = [this] (const char* id, float plain)
    {
        if (auto* parameter = apvts.getParameter(id))
            parameter->setValueNotifyingHost(parameter->convertTo0to1(plain));
    };
    set("hpHz", targets.highPassHz); set("lpHz", targets.lowPassHz);
    set("headBumpDb", targets.headBumpDb); set("headBumpHz", targets.headBumpHz);
    set("speed", targets.speed); set("wowDepthMs", targets.wowDepthMs);
    set("flutterDepthMs", targets.flutterDepthMs); set("drive", targets.drive);
    set("comp", targets.compression); set("hiss", targets.hiss); set("hum", targets.hum);
    set("dropout", targets.dropout); set("dropoutMs", targets.dropoutMs);
    set("ceiling", targets.ceiling);
    set("outGain", clampf(targets.outputGain * value("outGain") / 0.98f, 0.0f, 1.5f));
    set("macroLink", 0.0f);
}

float TapeEngineAudioProcessor::inputPeak(int channel) const noexcept
{
    return inputPeaks[(size_t) juce::jlimit(0, 1, channel)].load(std::memory_order_relaxed);
}

float TapeEngineAudioProcessor::outputPeakForChannel(int channel) const noexcept
{
    return outputPeaks[(size_t) juce::jlimit(0, 1, channel)].load(std::memory_order_relaxed);
}

std::array<float, 64> TapeEngineAudioProcessor::outputTrace() const noexcept
{
    std::array<float, 64> result {};
    for (size_t index = 0; index < result.size(); ++index)
        result[index] = trace[index].load(std::memory_order_relaxed);
    return result;
}

const juce::String TapeEngineAudioProcessor::getName() const { return JucePlugin_Name; }
bool TapeEngineAudioProcessor::acceptsMidi() const { return false; }
bool TapeEngineAudioProcessor::producesMidi() const { return false; }
bool TapeEngineAudioProcessor::isMidiEffect() const { return false; }
double TapeEngineAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int TapeEngineAudioProcessor::getNumPrograms() { return 1; }
int TapeEngineAudioProcessor::getCurrentProgram() { return 0; }
void TapeEngineAudioProcessor::setCurrentProgram(int) {}
const juce::String TapeEngineAudioProcessor::getProgramName(int) { return {}; }
void TapeEngineAudioProcessor::changeProgramName(int, const juce::String&) {}

std::vector<float> TapeEngineAudioProcessor::decodeWavToMono(const void* data, size_t bytes, double targetSampleRate) const
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();

    auto stream = std::make_unique<juce::MemoryInputStream>(data, bytes, false);
    std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(std::move(stream)));
    if (reader == nullptr)
        return {};

    const auto len = (int) reader->lengthInSamples;
    if (len <= 0)
        return {};

    juce::AudioBuffer<float> tmp((int) reader->numChannels, len);
    reader->read(&tmp, 0, len, 0, true, true);

    std::vector<float> mono((size_t) len, 0.0f);
    if (reader->numChannels == 1)
    {
        auto* c = tmp.getReadPointer(0);
        for (int i = 0; i < len; ++i)
            mono[(size_t) i] = c[i];
    }
    else
    {
        auto* a = tmp.getReadPointer(0);
        auto* b = tmp.getReadPointer(1);
        for (int i = 0; i < len; ++i)
            mono[(size_t) i] = 0.5f * (a[i] + b[i]);
    }

    return resampleLinear(mono, reader->sampleRate, targetSampleRate);
}

void TapeEngineAudioProcessor::initSfx(double sampleRate)
{
    sfxSamples.clear();
    sfxSamples.reserve(11);

    sfxSamples.push_back(decodeWavToMono(BinaryData::OASD_Casette_Working_09_wav, (size_t) BinaryData::OASD_Casette_Working_09_wavSize, sampleRate));
    sfxSamples.push_back(decodeWavToMono(BinaryData::OASD_Turn_ON_12_wav, (size_t) BinaryData::OASD_Turn_ON_12_wavSize, sampleRate));
    sfxSamples.push_back(decodeWavToMono(BinaryData::OASD_New_Cassette_19_wav, (size_t) BinaryData::OASD_New_Cassette_19_wavSize, sampleRate));
    sfxSamples.push_back(decodeWavToMono(BinaryData::OASD_Play_Button_06_wav, (size_t) BinaryData::OASD_Play_Button_06_wavSize, sampleRate));
    sfxSamples.push_back(decodeWavToMono(BinaryData::OASD_Stop_16_wav, (size_t) BinaryData::OASD_Stop_16_wavSize, sampleRate));
    sfxSamples.push_back(decodeWavToMono(BinaryData::OASD_Casette_End_11_wav, (size_t) BinaryData::OASD_Casette_End_11_wavSize, sampleRate));
    sfxSamples.push_back(decodeWavToMono(BinaryData::OASD_Turn_OFF_13_wav, (size_t) BinaryData::OASD_Turn_OFF_13_wavSize, sampleRate));
    sfxSamples.push_back(decodeWavToMono(BinaryData::OASD_VHS_Working_05_wav, (size_t) BinaryData::OASD_VHS_Working_05_wavSize, sampleRate));
    sfxSamples.push_back(decodeWavToMono(BinaryData::OASD_VHS_IN_03_wav, (size_t) BinaryData::OASD_VHS_IN_03_wavSize, sampleRate));
    sfxSamples.push_back(decodeWavToMono(BinaryData::OASD_Play_Button_17_wav, (size_t) BinaryData::OASD_Play_Button_17_wavSize, sampleRate));
    sfxSamples.push_back(decodeWavToMono(BinaryData::OASD_VHS_Out_01_wav, (size_t) BinaryData::OASD_VHS_Out_01_wavSize, sampleRate));

    bedPos = { 0.0f, 0.0f };
    for (auto& v : sfxVoices)
        v = {};
    sfxGateEnv = 0.0f;
    sfxBelowCount = 0;
    sfxInSilence = false;
    sfxSeqCount = 0;
}

float TapeEngineAudioProcessor::readEmbeddedSample(int sampleIndex, float pos) const
{
    if (sampleIndex < 0 || sampleIndex >= (int) sfxSamples.size())
        return 0.0f;
    const auto& s = sfxSamples[(size_t) sampleIndex];
    if (s.size() < 2)
        return 0.0f;
    auto p = std::fmod(pos, (float) s.size());
    if (p < 0.0f)
        p += (float) s.size();
    const auto i0 = (int) p;
    const auto i1 = (i0 + 1) % (int) s.size();
    const auto frac = p - (float) i0;
    return s[(size_t) i0] + (s[(size_t) i1] - s[(size_t) i0]) * frac;
}

void TapeEngineAudioProcessor::startSfxVoice(int sampleIndex, float gain)
{
    for (auto& v : sfxVoices)
    {
        if (!v.active)
        {
            v.sampleIndex = sampleIndex;
            v.pos = 0.0f;
            v.step = 0.96f + 0.08f * unif(rng);
            v.gain = gain * (0.8f + 0.4f * unif(rng));
            v.active = true;
            return;
        }
    }

    auto& v = sfxVoices[0];
    v.sampleIndex = sampleIndex;
    v.pos = 0.0f;
    v.step = 1.0f;
    v.gain = gain;
    v.active = true;
}

float TapeEngineAudioProcessor::processSfxSample(float signalAbs, float sampleRate, float glitch, bool enabled, int bank, SfxMode mode, float level)
{
    if (!enabled || level <= 0.0001f || sfxSamples.empty())
        return 0.0f;

    const auto b = juce::jlimit(0, 1, bank);
    float y = 0.0f;

    if (mode == SfxMode::bed || mode == SfxMode::sequence)
    {
        const auto bedIdx = bedSampleByBank[(size_t) b];
        y += readEmbeddedSample(bedIdx, bedPos[(size_t) b]) * (0.30f + 0.45f * level);
        bedPos[(size_t) b] += 1.0f;
        const auto& s = sfxSamples[(size_t) bedIdx];
        if (!s.empty() && bedPos[(size_t) b] >= (float) s.size())
            bedPos[(size_t) b] -= (float) s.size();
    }

    const auto gateThr = 0.035f;
    const auto alpha = std::exp(-1.0f / (sampleRate * 0.010f));
    sfxGateEnv = std::sqrt((1.0f - alpha) * signalAbs * signalAbs + alpha * sfxGateEnv * sfxGateEnv);

    if (sfxGateEnv < gateThr)
        ++sfxBelowCount;
    else
        sfxBelowCount = 0;

    const auto minSilenceSamples = juce::jmax(1, (int) (0.18f * sampleRate));
    const auto nowSilence = sfxBelowCount >= minSilenceSamples;
    if (mode == SfxMode::edges)
    {
        if (!sfxInSilence && nowSilence)
        {
            sfxInSilence = true;
            const auto& pool = endByBank[(size_t) b];
            const auto idx = pool[(size_t) juce::jlimit(0, (int) pool.size() - 1, (int) std::floor(unif(rng) * (float) pool.size()))];
            startSfxVoice(idx, 0.5f + level);
        }
        else if (sfxInSilence && sfxGateEnv >= gateThr * 1.18f)
        {
            sfxInSilence = false;
            sfxBelowCount = 0;
            const auto& pool = startByBank[(size_t) b];
            const auto idx = pool[(size_t) juce::jlimit(0, (int) pool.size() - 1, (int) std::floor(unif(rng) * (float) pool.size()))];
            startSfxVoice(idx, 0.5f + level);
        }
    }

    if (mode == SfxMode::sequence)
    {
        if (--sfxSeqCount <= 0)
        {
            const auto p = 0.15f + 1.3f * glitch * glitch;
            if (unif(rng) < p / sampleRate)
            {
                const bool doStart = unif(rng) < 0.55f;
                const auto& pool = doStart ? startByBank[(size_t) b] : endByBank[(size_t) b];
                const auto idx = pool[(size_t) juce::jlimit(0, (int) pool.size() - 1, (int) std::floor(unif(rng) * (float) pool.size()))];
                startSfxVoice(idx, 0.45f + level);
                sfxSeqCount = juce::jmax(1, (int) (sampleRate * (0.12f + 0.35f * unif(rng))));
            }
            else
            {
                sfxSeqCount = juce::jmax(1, (int) (sampleRate * 0.03f));
            }
        }
    }

    for (auto& v : sfxVoices)
    {
        if (!v.active)
            continue;
        y += readEmbeddedSample(v.sampleIndex, v.pos) * v.gain * level;
        v.pos += v.step;
        const auto& s = sfxSamples[(size_t) juce::jlimit(0, (int) sfxSamples.size() - 1, v.sampleIndex)];
        if (s.empty() || v.pos >= (float) s.size())
            v.active = false;
    }

    return clampf(y, -1.0f, 1.0f);
}

void TapeEngineAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    tapeCore.prepare(sampleRate, (std::size_t) juce::jlimit(1, 2, getTotalNumInputChannels()));
    tapeCore.reset(0x74617065u);
    setLatencySamples(tapeCore.latencySamples());
    alignedDry.setSize(2, juce::jmax(1, samplesPerBlock), false, false, true);
    const auto dryDelayLength = (std::size_t) juce::jmax(2, tapeCore.latencySamples() + 1);
    for (auto& delay : dryDelay)
        delay.assign(dryDelayLength, 0.0f);
    dryWriteIndex = 0;
    for (auto& filters : tone)
    {
        filters.hp.reset();
        filters.bump.reset();
        filters.lp1.reset();
        filters.lp2.reset();
    }
    initSfx(sampleRate);
    updateToneFilters();
    currentBpm = 120.0;
    lastTempoStep = -1;
    fallbackTempoStep = 0;
    lastTempoDivision = -1;
    tempoFallbackSamples = 0;
    lastHostPpq = 0.0;
    hostTempoWasPlaying = false;
    pendingMechanismTrigger.store(false);
    for (auto& meter : inputPeaks) meter.store(0.0f);
    for (auto& meter : outputPeaks) meter.store(0.0f);
    for (auto& point : trace) point.store(0.0f);
}

void TapeEngineAudioProcessor::releaseResources() {}

bool TapeEngineAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in != out)
        return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void TapeEngineAudioProcessor::updateToneFilters()
{
    const auto sr = getSampleRate();
    if (sr <= 1000.0)
        return;

    auto hpHz = apvts.getRawParameterValue("hpHz")->load();
    auto lpHz = apvts.getRawParameterValue("lpHz")->load();
    auto bumpDb = apvts.getRawParameterValue("headBumpDb")->load();
    auto bumpHz = apvts.getRawParameterValue("headBumpHz")->load();
    if (apvts.getRawParameterValue("macroLink")->load() > 0.5f)
    {
        const auto targets = lost_audio::core::mapTapeMacros(
            apvts.getRawParameterValue("quality")->load(),
            apvts.getRawParameterValue("age")->load(),
            apvts.getRawParameterValue("wow")->load(),
            apvts.getRawParameterValue("glitch")->load());
        hpHz = targets.highPassHz;
        lpHz = targets.lowPassHz;
        bumpDb = targets.headBumpDb;
        bumpHz = targets.headBumpHz;
    }

    auto hp = juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, hpHz, 0.707f);
    auto lp = juce::dsp::IIR::Coefficients<float>::makeLowPass(sr, lpHz, 0.85f);
    auto bump = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, bumpHz, 0.9f, juce::Decibels::decibelsToGain(bumpDb));

    for (auto& t : tone)
    {
        t.hp.coefficients = hp;
        t.bump.coefficients = bump;
        t.lp1.coefficients = lp;
        t.lp2.coefficients = lp;
    }
}

lost_audio::core::TapeParameters TapeEngineAudioProcessor::readParameters(double bpm) const noexcept
{
    lost_audio::core::TapeParameters parameters;
    parameters.wowAmount = legacyMacrosActive() ? value("wow") : value("transportDrift");
    if (legacyMacrosActive())
    {
        const auto targets = lost_audio::core::mapTapeMacros(value("quality"), value("age"), parameters.wowAmount, value("glitch"));
        parameters.speed = targets.speed;
        parameters.wowDepthMs = targets.wowDepthMs;
        parameters.flutterDepthMs = targets.flutterDepthMs;
        parameters.drive = targets.drive;
        parameters.compression = targets.compression;
        parameters.hiss = targets.hiss;
        parameters.hum = targets.hum;
        parameters.dropout = targets.dropout;
        parameters.dropoutMs = targets.dropoutMs;
        parameters.ceiling = targets.ceiling;
        parameters.outputGain = clampf(targets.outputGain * value("outGain") / 0.98f, 0.0f, 1.5f);
    }
    else
    {
        parameters.speed = value("speed");
        parameters.wowDepthMs = value("wowDepthMs");
        parameters.flutterDepthMs = value("flutterDepthMs");
        parameters.drive = value("drive");
        parameters.compression = value("comp");
        parameters.hiss = value("hiss");
        parameters.hum = value("hum");
        parameters.dropout = value("dropout");
        parameters.dropoutMs = value("dropoutMs");
        parameters.ceiling = value("ceiling");
        parameters.outputGain = value("outGain");
    }
    if (value("dropoutTempoSync") > 0.5f)
        parameters.dropout = 0.0f;
    if (value("wowTempoSync") > 0.5f)
        parameters.wowRateHz = lost_audio::core::tempoDivisionRateHz(bpm, (int) value("wowDivision"));
    if (value("flutterTempoSync") > 0.5f)
        parameters.flutterRateHz = lost_audio::core::tempoDivisionRateHz(bpm, (int) value("flutterDivision"));
    return parameters;
}

float TapeEngineAudioProcessor::dropoutDurationSeconds(double bpm) const noexcept
{
    if (value("dropoutLengthSync") > 0.5f)
        return lost_audio::core::tempoDivisionMilliseconds(bpm, (int) value("dropoutLengthDivision")) * 0.001f;
    return value("dropoutMs") * 0.001f;
}

void TapeEngineAudioProcessor::triggerDropout() noexcept
{
    tapeCore.triggerDropout(value("dropoutStrength"), dropoutDurationSeconds(currentBpm));
}

void TapeEngineAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi);
    juce::ScopedNoDenormals nd;

    const auto inCh = juce::jlimit(0, 2, getTotalNumInputChannels());
    const auto outCh = getTotalNumOutputChannels();
    for (int ch = inCh; ch < outCh; ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());
    if (inCh == 0 || buffer.getNumSamples() == 0)
        return;

    for (int ch = 0; ch < inCh; ++ch)
        inputPeaks[(size_t) ch].store(buffer.getMagnitude(ch, 0, buffer.getNumSamples()), std::memory_order_relaxed);
    if (inCh == 1)
        inputPeaks[1].store(inputPeaks[0].load(std::memory_order_relaxed), std::memory_order_relaxed);

    updateToneFilters();

    const auto sr = (float) getSampleRate();
    const auto glitch = clampf(value("glitch"), 0.0f, 1.0f);

    bool isPlaying = false;
    bool hasPpq = false;
    auto bpm = currentBpm;
    auto ppq = 0.0;
    if (auto* hostPlayHead = getPlayHead())
        if (const auto position = hostPlayHead->getPosition())
        {
            isPlaying = position->getIsPlaying();
            if (const auto hostBpm = position->getBpm()) bpm = *hostBpm;
            if (const auto hostPpq = position->getPpqPosition()) { ppq = *hostPpq; hasPpq = true; }
        }
    currentBpm = juce::jlimit(20.0, 400.0, bpm);

    const auto tempoSync = value("dropoutTempoSync") > 0.5f;
    const auto division = juce::jlimit(0, 10, (int) value("dropoutDivision"));
    const auto usingHostSchedule = tempoSync && isPlaying && hasPpq;
    lost_audio::core::TempoEventSchedule events;
    if (!tempoSync || !isPlaying)
    {
        lastTempoStep = -1;
        lastTempoDivision = -1;
        tempoFallbackSamples = 0;
        fallbackTempoStep = 0;
        hostTempoWasPlaying = false;
    }
    else if (usingHostSchedule)
    {
        if (!hostTempoWasPlaying || ppq < lastHostPpq - 1.0e-7)
            lastTempoStep = -1;
        if (division != lastTempoDivision)
        {
            lastTempoStep = -1;
            lastTempoDivision = division;
        }
        events = lost_audio::core::tempoEventsInBlock(ppq, currentBpm, division, getSampleRate(), buffer.getNumSamples());
        lastHostPpq = ppq;
        hostTempoWasPlaying = true;
    }
    else
    {
        const auto interval = juce::jmax(1, (int) std::lround(
            lost_audio::core::tempoDivisionMilliseconds(currentBpm, division) * 0.001 * getSampleRate()));
        auto offset = tempoFallbackSamples;
        while (offset < buffer.getNumSamples() && events.size < lost_audio::core::TempoEventSchedule::capacity)
        {
            if (offset >= 0)
                events.events[events.size++] = { offset, fallbackTempoStep++ };
            offset += interval;
        }
        tempoFallbackSamples = offset - buffer.getNumSamples();
        lastTempoStep = -1;
        lastTempoDivision = division;
        hostTempoWasPlaying = true;
    }

    const auto sfxEnable = value("sfxEnable") > 0.5f;
    const auto sfxBank = juce::jlimit(0, 1, (int) value("sfxBank"));
    const auto sfxMode = (SfxMode) juce::jlimit(0, 2, (int) value("sfxMode"));
    const auto sfxLevel = clampf(value("sfxLevel"), 0.0f, 1.0f);

    std::array<float*, 2> writePtrs { nullptr, nullptr };
    for (int ch = 0; ch < inCh; ++ch)
        writePtrs[(size_t) ch] = buffer.getWritePointer(ch);

    const auto sampleCount = buffer.getNumSamples();
    if (alignedDry.getNumSamples() < sampleCount)
        alignedDry.setSize(2, sampleCount, false, false, true);
    const auto dryDelayLength = dryDelay[0].size();
    const auto dryLatency = (std::size_t) juce::jlimit(0, (int) dryDelayLength - 1, tapeCore.latencySamples());
    for (int i = 0; i < sampleCount; ++i)
    {
        const auto readIndex = (dryWriteIndex + dryDelayLength - dryLatency) % dryDelayLength;
        for (int ch = 0; ch < inCh; ++ch)
        {
            dryDelay[(size_t) ch][dryWriteIndex] = writePtrs[(size_t) ch][i];
            alignedDry.setSample(ch, i, dryDelay[(size_t) ch][readIndex]);
        }
        dryWriteIndex = (dryWriteIndex + 1) % dryDelayLength;
    }

    if (pendingMechanismTrigger.exchange(false, std::memory_order_acq_rel) && sfxEnable)
    {
        const auto useStart = unif(rng) < 0.6f;
        const auto& pool = useStart ? startByBank[(size_t) sfxBank] : endByBank[(size_t) sfxBank];
        const auto poolIndex = juce::jlimit(0, (int) pool.size() - 1,
                                           (int) std::floor(unif(rng) * (float) pool.size()));
        startSfxVoice(pool[(size_t) poolIndex], 0.75f + sfxLevel * 0.5f);
    }

    // Mechanical beds and edge events are decoded by the JUCE adapter, then
    // enter the same point as the browser graph's SFX input: immediately
    // before the portable transport/nonlinear processor.
    auto measuredMechanism = 0.0f;
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float inAbs = 0.0f;
        for (int ch = 0; ch < inCh; ++ch)
            inAbs += std::abs(writePtrs[(size_t) ch][i]);
        inAbs /= juce::jmax(1, inCh);

        const auto sfx = processSfxSample(inAbs, sr, glitch, sfxEnable, sfxBank, sfxMode, sfxLevel);
        measuredMechanism = juce::jmax(measuredMechanism, std::abs(sfx));
        for (int ch = 0; ch < inCh; ++ch)
            writePtrs[(size_t) ch][i] += sfx;
    }

    const auto parameters = readParameters(currentBpm);
    const auto processRange = [&] (int start, int length)
    {
        if (length <= 0) return;
        std::array<float*, 2> rangePointers { nullptr, nullptr };
        for (int channel = 0; channel < inCh; ++channel)
            rangePointers[(size_t) channel] = writePtrs[(size_t) channel] + start;
        tapeCore.process(rangePointers.data(), (size_t) inCh, (size_t) length, parameters);
    };

    auto cursor = 0;
    for (size_t index = 0; index < events.size; ++index)
    {
        const auto event = events.events[index];
        if (usingHostSchedule && event.stepIndex == lastTempoStep)
            continue;
        processRange(cursor, event.sampleOffset - cursor);
        cursor = event.sampleOffset;
        if (!tapeCore.dropoutActive()
            && lost_audio::core::tempoEventDecision(event.stepIndex, value("dropoutProbability"), 0x74617065ull))
            tapeCore.triggerDropout(value("dropoutStrength"), dropoutDurationSeconds(currentBpm));
        if (usingHostSchedule)
            lastTempoStep = event.stepIndex;
    }
    processRange(cursor, buffer.getNumSamples() - cursor);

    modulationTelemetry.store(std::abs(tapeCore.modulationDisplacementMs()), std::memory_order_relaxed);
    dropoutState.store(tapeCore.dropoutActive(), std::memory_order_relaxed);
    dropoutTelemetry.store(tapeCore.dropoutProgress(), std::memory_order_relaxed);
    compressionTelemetry.store(tapeCore.compressionReduction(), std::memory_order_relaxed);
    saturationTelemetry.store(tapeCore.saturationActivity(), std::memory_order_relaxed);
    noiseTelemetry.store(tapeCore.noiseActivity(), std::memory_order_relaxed);
    mechanismTelemetry.store(measuredMechanism, std::memory_order_relaxed);
    limiterTelemetry.store(tapeCore.limiterActivity(), std::memory_order_relaxed);

    // Web Audio places the cabinet filters after the worklet. Retaining JUCE's
    // filters here keeps the adapter thin while matching that topology.
    for (int ch = 0; ch < inCh; ++ch)
    {
        auto& filters = tone[(size_t) ch];
        auto* samples = writePtrs[(size_t) ch];
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            auto value = filters.hp.processSample(samples[i]);
            value = filters.bump.processSample(value);
            value = filters.lp1.processSample(value);
            samples[i] = filters.lp2.processSample(value);
        }
    }

    const auto mix = clampf(value("mix"), 0.0f, 1.0f);
    if (mix < 0.99999f)
        for (int ch = 0; ch < inCh; ++ch)
        {
            auto* wet = writePtrs[(size_t) ch];
            const auto* dry = alignedDry.getReadPointer(ch);
            for (int i = 0; i < sampleCount; ++i)
            {
                const auto dryOutput = dry[i] * parameters.outputGain;
                wet[i] = clampf(dryOutput + (wet[i] - dryOutput) * mix,
                                -parameters.ceiling, parameters.ceiling);
            }
        }

    float blockPeak = 0.0f;
    for (int ch = 0; ch < inCh; ++ch)
    {
        const auto peak = buffer.getMagnitude(ch, 0, buffer.getNumSamples());
        outputPeaks[(size_t) ch].store(peak, std::memory_order_relaxed);
        blockPeak = juce::jmax(blockPeak, peak);
    }
    if (inCh == 1)
        outputPeaks[1].store(outputPeaks[0].load(std::memory_order_relaxed), std::memory_order_relaxed);
    outputPeak.store(juce::jlimit(0.0f, 1.0f, blockPeak), std::memory_order_relaxed);

    for (size_t point = 0; point < trace.size(); ++point)
    {
        const auto sample = juce::jlimit(0, buffer.getNumSamples() - 1,
            (int) std::floor((double) point * (double) buffer.getNumSamples() / (double) trace.size()));
        auto valueAtPoint = 0.0f;
        for (int channel = 0; channel < inCh; ++channel)
            valueAtPoint += buffer.getSample(channel, sample);
        trace[point].store(valueAtPoint / (float) inCh, std::memory_order_relaxed);
    }
}

bool TapeEngineAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* TapeEngineAudioProcessor::createEditor()
{
    return new TapeEngineAudioProcessorEditor(*this);
}

void TapeEngineAudioProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty("engineId", "tape", nullptr);
    state.setProperty("schemaVersion", 5, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, dest);
}

void TapeEngineAudioProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType()))
        {
            // V1 states had no schema marker. Parameter identifiers remain
            // stable, so they migrate losslessly into the shared-core adapter.
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
            apvts.state.setProperty("engineId", "tape", nullptr);
            apvts.state.setProperty("schemaVersion", 5, nullptr);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TapeEngineAudioProcessor();
}
