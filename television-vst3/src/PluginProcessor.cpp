#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <BinaryData.h>

namespace
{
float clampf(float x, float lo, float hi) { return juce::jlimit(lo, hi, x); }
float softClip(float x) { return std::tanh(x); }

std::vector<float> resampleLinear(const std::vector<float>& in, double srcRate, double dstRate)
{
    if (in.empty() || srcRate <= 1000.0 || dstRate <= 1000.0) return {};
    if (std::abs(srcRate - dstRate) < 1.0) return in;

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
        if (pos > (double) in.size() - 1.0) pos = (double) in.size() - 1.0;
    }
    return out;
}
}

TelevisionEngineAudioProcessor::TelevisionEngineAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout()),
      rng(0xdecafbad)
{
}

juce::AudioProcessorValueTreeState::ParameterLayout TelevisionEngineAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto n01 = juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f);

    p.push_back(std::make_unique<juce::AudioParameterFloat>("vibe", "Vibe", n01, 0.45f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("speaker", "Speaker", n01, 0.55f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("agc", "AGC", n01, 0.22f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("static", "Static", n01, 0.12f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("hum", "Hum", n01, 0.18f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("whine", "Whine", n01, 0.08f));

    p.push_back(std::make_unique<juce::AudioParameterFloat>("hpHz", "High-pass", juce::NormalisableRange<float>(20.0f, 1200.0f, 1.0f), 70.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("lpHz", "Low-pass", juce::NormalisableRange<float>(800.0f, 18000.0f, 1.0f), 9000.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("midHumpDb", "Mid Hump", juce::NormalisableRange<float>(-6.0f, 10.0f, 0.1f), 1.2f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("midFreq", "Mid Freq", juce::NormalisableRange<float>(600.0f, 5000.0f, 1.0f), 1800.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("noiseHiss", "Noise Hiss", n01, 0.55f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("noiseCrackle", "Noise Crackle", n01, 0.08f));

    p.push_back(std::make_unique<juce::AudioParameterBool>("bedEnable", "Bed Enable", false));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("bedLevel", "Bed Level", n01, 0.22f));

    p.push_back(std::make_unique<juce::AudioParameterFloat>("outGain", "Output Gain", juce::NormalisableRange<float>(0.0f, 1.5f, 0.01f), 1.0f));

    return { p.begin(), p.end() };
}

const juce::String TelevisionEngineAudioProcessor::getName() const { return JucePlugin_Name; }
bool TelevisionEngineAudioProcessor::acceptsMidi() const { return false; }
bool TelevisionEngineAudioProcessor::producesMidi() const { return false; }
bool TelevisionEngineAudioProcessor::isMidiEffect() const { return false; }
double TelevisionEngineAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int TelevisionEngineAudioProcessor::getNumPrograms() { return 1; }
int TelevisionEngineAudioProcessor::getCurrentProgram() { return 0; }
void TelevisionEngineAudioProcessor::setCurrentProgram(int) {}
const juce::String TelevisionEngineAudioProcessor::getProgramName(int) { return {}; }
void TelevisionEngineAudioProcessor::changeProgramName(int, const juce::String&) {}

float TelevisionEngineAudioProcessor::nextWhite() { return unif(rng) * 2.0f - 1.0f; }

std::vector<float> TelevisionEngineAudioProcessor::decodeMp3ToMono(const void* data, size_t bytes, double targetSampleRate) const
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    auto stream = std::make_unique<juce::MemoryInputStream>(data, bytes, false);
    std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(std::move(stream)));
    if (reader == nullptr) return {};

    const auto len = (int) reader->lengthInSamples;
    if (len <= 0) return {};

    juce::AudioBuffer<float> tmp((int) reader->numChannels, len);
    reader->read(&tmp, 0, len, 0, true, true);

    std::vector<float> mono((size_t) len, 0.0f);
    if (reader->numChannels == 1)
    {
        auto* c = tmp.getReadPointer(0);
        for (int i = 0; i < len; ++i) mono[(size_t) i] = c[i];
    }
    else
    {
        auto* a = tmp.getReadPointer(0);
        auto* b = tmp.getReadPointer(1);
        for (int i = 0; i < len; ++i) mono[(size_t) i] = 0.5f * (a[i] + b[i]);
    }

    return resampleLinear(mono, reader->sampleRate, targetSampleRate);
}

void TelevisionEngineAudioProcessor::initBed(double sampleRate)
{
    bedSample = decodeMp3ToMono(BinaryData::crt_mp3, (size_t) BinaryData::crt_mp3Size, sampleRate);
    bedPos = 0.0f;
}

float TelevisionEngineAudioProcessor::readBedSample(float pos) const
{
    if (bedSample.size() < 2) return 0.0f;
    auto p = std::fmod(pos, (float) bedSample.size());
    if (p < 0.0f) p += (float) bedSample.size();
    const auto i0 = (int) p;
    const auto i1 = (i0 + 1) % (int) bedSample.size();
    const auto frac = p - (float) i0;
    return bedSample[(size_t) i0] + (bedSample[(size_t) i1] - bedSample[(size_t) i0]) * frac;
}

void TelevisionEngineAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    for (auto& c : chans)
    {
        c = {};
        c.whinePhase = unif(rng) * juce::MathConstants<float>::twoPi;
        c.humPhase = unif(rng) * juce::MathConstants<float>::twoPi;
    }
    initBed(sampleRate);
    updateToneFilters();
}

void TelevisionEngineAudioProcessor::releaseResources() {}

bool TelevisionEngineAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void TelevisionEngineAudioProcessor::applyMacro(Runtime& r) const
{
    const auto v = std::pow(clampf(r.vibe, 0.0f, 1.0f), 1.15f);
    const auto sp = std::pow(clampf(r.speaker, 0.0f, 1.0f), 1.15f);
    const auto a = std::pow(clampf(r.agc, 0.0f, 1.0f), 1.25f);
    const auto st = std::pow(clampf(r.statik, 0.0f, 1.0f), 1.2f);

    r.hpHz = 45.0f + (1.0f - sp) * 110.0f + v * 30.0f;
    r.lpHz = 16000.0f - (1.0f - sp) * 10000.0f - v * 2600.0f;
    r.midHumpDb = 0.6f + (1.0f - sp) * 2.4f + v * 0.6f;
    r.midFreq = 1550.0f + (1.0f - sp) * 650.0f;
    r.noiseHiss = clampf(0.45f + st * 0.5f, 0.0f, 1.0f);
    r.noiseCrackle = clampf(0.04f + v * 0.12f, 0.0f, 1.0f);

    juce::ignoreUnused(a);
}

void TelevisionEngineAudioProcessor::updateToneFilters()
{
    const auto sr = getSampleRate();
    if (sr <= 1000.0) return;

    Runtime r;
    r.vibe = apvts.getRawParameterValue("vibe")->load();
    r.speaker = apvts.getRawParameterValue("speaker")->load();
    r.agc = apvts.getRawParameterValue("agc")->load();
    r.statik = apvts.getRawParameterValue("static")->load();
    applyMacro(r);

    r.hpHz = apvts.getRawParameterValue("hpHz")->load();
    r.lpHz = apvts.getRawParameterValue("lpHz")->load();
    r.midHumpDb = apvts.getRawParameterValue("midHumpDb")->load();
    r.midFreq = apvts.getRawParameterValue("midFreq")->load();

    auto hp = juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, clampf(r.hpHz, 20.0f, 1200.0f), 0.707f);
    auto lp = juce::dsp::IIR::Coefficients<float>::makeLowPass(sr, clampf(r.lpHz, 800.0f, 18000.0f), 0.85f);
    auto dip = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, 650.0f, 0.9f, juce::Decibels::decibelsToGain(-0.35f * r.midHumpDb));
    auto hump = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, clampf(r.midFreq, 600.0f, 5000.0f), 1.15f, juce::Decibels::decibelsToGain(r.midHumpDb));

    for (auto& t : tone)
    {
        t.hp1.coefficients = hp;
        t.hp2.coefficients = hp;
        t.lp1.coefficients = lp;
        t.lp2.coefficients = lp;
        t.dip.coefficients = dip;
        t.hump.coefficients = hump;
    }
}

void TelevisionEngineAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi);
    juce::ScopedNoDenormals nd;

    const auto inCh = getTotalNumInputChannels();
    const auto outCh = getTotalNumOutputChannels();
    for (int ch = inCh; ch < outCh; ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());

    updateToneFilters();

    Runtime r;
    r.vibe = apvts.getRawParameterValue("vibe")->load();
    r.speaker = apvts.getRawParameterValue("speaker")->load();
    r.agc = apvts.getRawParameterValue("agc")->load();
    r.statik = apvts.getRawParameterValue("static")->load();
    r.hum = apvts.getRawParameterValue("hum")->load();
    r.whine = apvts.getRawParameterValue("whine")->load();
    applyMacro(r);
    r.noiseHiss = apvts.getRawParameterValue("noiseHiss")->load();
    r.noiseCrackle = apvts.getRawParameterValue("noiseCrackle")->load();
    r.bedEnable = apvts.getRawParameterValue("bedEnable")->load() > 0.5f;
    r.bedLevel = apvts.getRawParameterValue("bedLevel")->load();
    r.outGain = apvts.getRawParameterValue("outGain")->load();

    const auto sr = (float) getSampleRate();
    const auto compAmt = clampf(0.08f + std::pow(clampf(r.agc, 0.0f, 1.0f), 1.25f) * 0.65f, 0.0f, 1.0f);
    const auto drive = 0.35f + compAmt * 1.2f;
    const auto thr = 0.16f + (1.0f - compAmt) * 0.2f;
    const auto attack = std::exp(-1.0f / (sr * 0.008f));
    const auto release = std::exp(-1.0f / (sr * 0.12f));
    const auto limAtk = std::exp(-1.0f / (sr * 0.002f));
    const auto limRel = std::exp(-1.0f / (sr * 0.08f));

    const auto hpA = 0.6f;
    const auto lpA = 0.004f + (1.0f - clampf(r.noiseHiss, 0.0f, 1.0f)) * 0.06f;

    const auto humHz = 60.0f;
    const auto whineHz = clampf((float) getSampleRate() * 0.45f, 2000.0f, 15734.0f);

    for (int ch = 0; ch < inCh && ch < 2; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        auto& s = chans[(size_t) ch];
        auto& t = tone[(size_t) ch];

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            auto y = data[i];

            auto white = nextWhite();
            s.hpState = hpA * (s.hpState + white - s.prevWhite);
            s.prevWhite = white;
            s.lpState += (s.hpState - s.lpState) * lpA;

            if (s.crackleHold > 0)
                s.crackleHold--;
            else
            {
                const auto p = 0.00001f + clampf(r.noiseCrackle, 0.0f, 1.0f) * 0.00028f;
                if (unif(rng) < p)
                {
                    s.crackleHold = 8 + (int) std::floor(unif(rng) * 24.0f);
                    s.crackleAmp = 0.7f + unif(rng) * 0.6f;
                }
            }
            const auto crack = s.crackleHold > 0 ? s.crackleAmp * nextWhite() : 0.0f;
            const auto noise = (s.lpState + crack * 0.25f) * clampf(r.statik, 0.0f, 1.0f);

            s.humPhase += juce::MathConstants<float>::twoPi * humHz / sr;
            if (s.humPhase > juce::MathConstants<float>::twoPi) s.humPhase -= juce::MathConstants<float>::twoPi;
            const auto hum = std::sin(s.humPhase) * clampf(r.hum, 0.0f, 1.0f) * 0.03f;

            s.whinePhase += juce::MathConstants<float>::twoPi * whineHz / sr;
            if (s.whinePhase > juce::MathConstants<float>::twoPi) s.whinePhase -= juce::MathConstants<float>::twoPi;
            const auto whine = std::sin(s.whinePhase) * clampf(r.whine, 0.0f, 1.0f) * 0.003f;

            float bed = 0.0f;
            if (r.bedEnable)
            {
                bed = readBedSample(bedPos) * clampf(r.bedLevel, 0.0f, 1.0f);
                bedPos += 1.0f;
                if (!bedSample.empty() && bedPos >= (float) bedSample.size()) bedPos -= (float) bedSample.size();
            }

            y += noise + hum + whine + bed;

            y = t.hp1.processSample(y);
            y = t.hp2.processSample(y);
            y = t.dip.processSample(y);
            y = t.hump.processSample(y);
            y = t.lp1.processSample(y);
            y = t.lp2.processSample(y);

            const auto pre = (y * (2.2f + drive * 0.55f));
            y = softClip(pre);

            const auto a = std::abs(y);
            const auto c = a > s.compEnv ? attack : release;
            s.compEnv = a + c * (s.compEnv - a);
            auto g = 1.0f;
            if (s.compEnv > thr) g = thr / (s.compEnv + 1e-6f);
            y *= g;

            auto post = y * clampf(r.outGain, 0.0f, 1.5f);
            const auto aa = std::abs(post);
            const auto lc = aa > s.limEnv ? limAtk : limRel;
            s.limEnv = aa + lc * (s.limEnv - aa);
            const auto lim = s.limEnv > 0.97f ? 0.97f / (s.limEnv + 1e-6f) : 1.0f;
            post *= lim;

            data[i] = clampf(post, -1.0f, 1.0f);
        }
    }
}

bool TelevisionEngineAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* TelevisionEngineAudioProcessor::createEditor() { return new TelevisionEngineAudioProcessorEditor(*this); }

void TelevisionEngineAudioProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, dest);
}

void TelevisionEngineAudioProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TelevisionEngineAudioProcessor(); }
