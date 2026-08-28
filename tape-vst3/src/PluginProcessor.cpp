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
}

juce::AudioProcessorValueTreeState::ParameterLayout TapeEngineAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto n01 = juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f);

    p.push_back(std::make_unique<juce::AudioParameterFloat>("quality", "Quality", n01, 0.55f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("age", "Age", n01, 0.35f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("wow", "Wow", n01, 0.25f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("glitch", "Glitch", n01, 0.18f));

    p.push_back(std::make_unique<juce::AudioParameterFloat>("hpHz", "High-pass", juce::NormalisableRange<float>(10.0f, 240.0f, 1.0f), 35.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("lpHz", "Low-pass", juce::NormalisableRange<float>(1200.0f, 22000.0f, 1.0f), 11000.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("headBumpDb", "Head Bump dB", juce::NormalisableRange<float>(0.0f, 12.0f, 0.1f), 2.2f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("headBumpHz", "Head Bump Hz", juce::NormalisableRange<float>(30.0f, 220.0f, 1.0f), 85.0f));

    p.push_back(std::make_unique<juce::AudioParameterFloat>("speed", "Speed", juce::NormalisableRange<float>(0.5f, 2.0f, 0.001f), 1.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("wowDepthMs", "Wow Depth", juce::NormalisableRange<float>(0.0f, 20.0f, 0.1f), 3.5f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("flutterDepthMs", "Flutter Depth", juce::NormalisableRange<float>(0.0f, 10.0f, 0.1f), 1.2f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("drive", "Drive", n01, 0.35f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("comp", "Compression", n01, 0.28f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("hiss", "Hiss", n01, 0.12f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("hum", "Hum", n01, 0.05f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("dropout", "Dropout", n01, 0.18f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("dropoutMs", "Dropout Length", juce::NormalisableRange<float>(1.0f, 400.0f, 1.0f), 38.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("ceiling", "Ceiling", juce::NormalisableRange<float>(0.2f, 1.0f, 0.001f), 0.92f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("outGain", "Output Gain", juce::NormalisableRange<float>(0.0f, 1.5f, 0.01f), 0.98f));

    p.push_back(std::make_unique<juce::AudioParameterBool>("sfxEnable", "SFX Enable", false));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("sfxBank", "SFX Bank", juce::StringArray { "Cassette", "VHS" }, 0));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("sfxMode", "SFX Mode", juce::StringArray { "Bed", "Edges", "Sequence" }, 0));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("sfxLevel", "SFX Level", n01, 0.22f));

    return { p.begin(), p.end() };
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
    juce::ignoreUnused(samplesPerBlock);
    tapeCore.prepare(sampleRate, (std::size_t) juce::jlimit(1, 2, getTotalNumInputChannels()));
    tapeCore.reset(0x74617065u);
    setLatencySamples(tapeCore.latencySamples());
    for (auto& filters : tone)
    {
        filters.hp.reset();
        filters.bump.reset();
        filters.lp1.reset();
        filters.lp2.reset();
    }
    initSfx(sampleRate);
    updateToneFilters();
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

    const auto hpHz = apvts.getRawParameterValue("hpHz")->load();
    const auto lpHz = apvts.getRawParameterValue("lpHz")->load();
    const auto bumpDb = apvts.getRawParameterValue("headBumpDb")->load();
    const auto bumpHz = apvts.getRawParameterValue("headBumpHz")->load();

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

void TapeEngineAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi);
    juce::ScopedNoDenormals nd;

    const auto inCh = getTotalNumInputChannels();
    const auto outCh = getTotalNumOutputChannels();
    for (int ch = inCh; ch < outCh; ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());

    updateToneFilters();

    const auto sr = (float) getSampleRate();
    const auto glitch = clampf(apvts.getRawParameterValue("glitch")->load(), 0.0f, 1.0f);

    const auto sfxEnable = apvts.getRawParameterValue("sfxEnable")->load() > 0.5f;
    const auto sfxBank = juce::jlimit(0, 1, (int) apvts.getRawParameterValue("sfxBank")->load());
    const auto sfxMode = (SfxMode) juce::jlimit(0, 2, (int) apvts.getRawParameterValue("sfxMode")->load());
    const auto sfxLevel = clampf(apvts.getRawParameterValue("sfxLevel")->load(), 0.0f, 1.0f);

    std::array<float*, 2> writePtrs { nullptr, nullptr };
    for (int ch = 0; ch < inCh && ch < 2; ++ch)
        writePtrs[(size_t) ch] = buffer.getWritePointer(ch);

    // Mechanical beds and edge events are decoded by the JUCE adapter, then
    // enter the same point as the browser graph's SFX input: immediately
    // before the portable transport/nonlinear processor.
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float inAbs = 0.0f;
        for (int ch = 0; ch < inCh && ch < 2; ++ch)
            inAbs += std::abs(writePtrs[(size_t) ch][i]);
        inAbs /= juce::jmax(1, juce::jmin(inCh, 2));

        const auto sfx = processSfxSample(inAbs, sr, glitch, sfxEnable, sfxBank, sfxMode, sfxLevel);
        for (int ch = 0; ch < inCh && ch < 2; ++ch)
            writePtrs[(size_t) ch][i] += sfx;
    }

    lost_audio::core::TapeParameters parameters;
    parameters.speed = apvts.getRawParameterValue("speed")->load();
    parameters.wowDepthMs = apvts.getRawParameterValue("wowDepthMs")->load();
    parameters.flutterDepthMs = apvts.getRawParameterValue("flutterDepthMs")->load();
    parameters.wowAmount = apvts.getRawParameterValue("wow")->load();
    parameters.drive = apvts.getRawParameterValue("drive")->load();
    parameters.compression = apvts.getRawParameterValue("comp")->load();
    parameters.hiss = apvts.getRawParameterValue("hiss")->load();
    parameters.hum = apvts.getRawParameterValue("hum")->load();
    parameters.dropout = apvts.getRawParameterValue("dropout")->load();
    parameters.dropoutMs = apvts.getRawParameterValue("dropoutMs")->load();
    parameters.ceiling = apvts.getRawParameterValue("ceiling")->load();
    parameters.outputGain = apvts.getRawParameterValue("outGain")->load();
    tapeCore.process(writePtrs.data(), (std::size_t) juce::jlimit(0, 2, inCh), (std::size_t) buffer.getNumSamples(), parameters);

    // Web Audio places the cabinet filters after the worklet. Retaining JUCE's
    // filters here keeps the adapter thin while matching that topology.
    for (int ch = 0; ch < inCh && ch < 2; ++ch)
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

    float blockPeak = 0.0f;
    for (int ch = 0; ch < inCh && ch < 2; ++ch)
        blockPeak = juce::jmax(blockPeak, buffer.getMagnitude(ch, 0, buffer.getNumSamples()));
    outputPeak.store(juce::jlimit(0.0f, 1.0f, blockPeak), std::memory_order_relaxed);
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
    state.setProperty("schemaVersion", 2, nullptr);
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
            apvts.state.setProperty("schemaVersion", 2, nullptr);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TapeEngineAudioProcessor();
}
