#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
float clampf(float x, float lo, float hi)
{
    return juce::jlimit(lo, hi, x);
}

void eqPow(float wet, float& dryOut, float& wetOut)
{
    const auto w = clampf(wet, 0.0f, 1.0f);
    const auto a = w * juce::MathConstants<float>::halfPi;
    dryOut = std::cos(a);
    wetOut = std::sin(a);
}
}

OcclusionEngineAudioProcessor::OcclusionEngineAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout OcclusionEngineAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto n01 = juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f);

    p.push_back(std::make_unique<juce::AudioParameterFloat>("distance", "Distance", n01, 0.35f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("wall", "Wall", n01, 0.45f));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("material", "Material", juce::StringArray { "Drywall", "Brick", "Wood", "Curtain", "Door", "Glass" }, 0));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("sourceRoom", "Source Room", n01, 0.35f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("listenerRoom", "Listener Room", n01, 0.45f));

    p.push_back(std::make_unique<juce::AudioParameterFloat>("hpHz", "HP", juce::NormalisableRange<float>(10.0f, 600.0f, 1.0f), 50.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("lpHz", "LP", juce::NormalisableRange<float>(80.0f, 20000.0f, 1.0f), 5200.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("dipHz", "Dip Hz", juce::NormalisableRange<float>(120.0f, 10000.0f, 1.0f), 1600.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("dipDb", "Dip dB", juce::NormalisableRange<float>(-18.0f, 6.0f, 0.1f), -2.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("dipQ", "Dip Q", juce::NormalisableRange<float>(0.2f, 8.0f, 0.01f), 1.1f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("bumpHz", "Bump Hz", juce::NormalisableRange<float>(120.0f, 4000.0f, 1.0f), 420.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("bumpDb", "Bump dB", juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 1.2f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("bumpQ", "Bump Q", juce::NormalisableRange<float>(0.2f, 5.0f, 0.01f), 0.95f));

    p.push_back(std::make_unique<juce::AudioParameterFloat>("leak", "Leak", n01, 0.08f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("roomMix", "Room Mix", n01, 0.22f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("predelayMs", "Predelay", juce::NormalisableRange<float>(0.0f, 120.0f, 1.0f), 12.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("roomSize", "Room Size", n01, 0.5f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("damp", "Damp", n01, 0.68f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("outGain", "Out Gain", juce::NormalisableRange<float>(0.0f, 1.5f, 0.01f), 1.0f));

    return { p.begin(), p.end() };
}

const juce::String OcclusionEngineAudioProcessor::getName() const { return JucePlugin_Name; }
bool OcclusionEngineAudioProcessor::acceptsMidi() const { return false; }
bool OcclusionEngineAudioProcessor::producesMidi() const { return false; }
bool OcclusionEngineAudioProcessor::isMidiEffect() const { return false; }
double OcclusionEngineAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int OcclusionEngineAudioProcessor::getNumPrograms() { return 1; }
int OcclusionEngineAudioProcessor::getCurrentProgram() { return 0; }
void OcclusionEngineAudioProcessor::setCurrentProgram(int) {}
const juce::String OcclusionEngineAudioProcessor::getProgramName(int) { return {}; }
void OcclusionEngineAudioProcessor::changeProgramName(int, const juce::String&) {}

void OcclusionEngineAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);

    const auto predelayLen = juce::jmax(64, (int) std::ceil(sampleRate * 0.12));
    for (auto& c : channels)
    {
        c.predelay.assign((size_t) predelayLen, 0.0f);
        c.writePos = 0;
    }

    filteredL.assign((size_t) juce::jmax(32, samplesPerBlock), 0.0f);
    filteredR.assign((size_t) juce::jmax(32, samplesPerBlock), 0.0f);
    wetL.assign((size_t) juce::jmax(32, samplesPerBlock), 0.0f);
    wetR.assign((size_t) juce::jmax(32, samplesPerBlock), 0.0f);

    juce::Reverb::Parameters rp;
    rp.roomSize = 0.5f;
    rp.damping = 0.68f;
    rp.wetLevel = 1.0f;
    rp.dryLevel = 0.0f;
    rp.width = 1.0f;
    rp.freezeMode = 0.0f;
    reverb.setParameters(rp);

    updateFilters();
}

void OcclusionEngineAudioProcessor::releaseResources() {}

bool OcclusionEngineAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in != out)
        return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void OcclusionEngineAudioProcessor::updateFilters()
{
    const auto sr = getSampleRate();
    if (sr <= 1000.0)
        return;

    const auto hpHz = apvts.getRawParameterValue("hpHz")->load();
    const auto lpHz = apvts.getRawParameterValue("lpHz")->load();
    const auto dipHz = apvts.getRawParameterValue("dipHz")->load();
    const auto dipDb = apvts.getRawParameterValue("dipDb")->load();
    const auto dipQ = apvts.getRawParameterValue("dipQ")->load();
    const auto bumpHz = apvts.getRawParameterValue("bumpHz")->load();
    const auto bumpDb = apvts.getRawParameterValue("bumpDb")->load();
    const auto bumpQ = apvts.getRawParameterValue("bumpQ")->load();

    auto hp = juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, clampf(hpHz, 10.0f, 20000.0f), 0.707f);
    auto lp = juce::dsp::IIR::Coefficients<float>::makeLowPass(sr, clampf(lpHz, 20.0f, 20000.0f), 0.85f);
    auto dip = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, clampf(dipHz, 20.0f, 20000.0f), juce::jmax(0.2f, dipQ), juce::Decibels::decibelsToGain(dipDb));
    auto bump = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, clampf(bumpHz, 20.0f, 20000.0f), juce::jmax(0.2f, bumpQ), juce::Decibels::decibelsToGain(bumpDb));

    for (auto& f : filters)
    {
        f.hp1.coefficients = hp;
        f.hp2.coefficients = hp;
        f.lp1.coefficients = lp;
        f.lp2.coefficients = lp;
        f.dip.coefficients = dip;
        f.bump.coefficients = bump;
    }
}

float OcclusionEngineAudioProcessor::readPredelay(const ChannelState& st, float delaySamps) const
{
    const auto len = (int) st.predelay.size();
    if (len < 2)
        return 0.0f;

    const auto read = (float) st.writePos - delaySamps;
    auto i0 = (int) std::floor(read);
    while (i0 < 0)
        i0 += len;
    i0 %= len;
    const auto i1 = (i0 + 1) % len;
    const auto frac = read - std::floor(read);
    return st.predelay[(size_t) i0] * (1.0f - frac) + st.predelay[(size_t) i1] * frac;
}

void OcclusionEngineAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi);
    juce::ScopedNoDenormals nd;

    const auto inCh = getTotalNumInputChannels();
    const auto outCh = getTotalNumOutputChannels();
    for (int ch = inCh; ch < outCh; ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());

    updateFilters();

    const auto n = buffer.getNumSamples();
    if ((int) filteredL.size() < n)
    {
        filteredL.resize((size_t) n);
        filteredR.resize((size_t) n);
        wetL.resize((size_t) n);
        wetR.resize((size_t) n);
    }

    const auto sr = (float) getSampleRate();
    const auto leak = clampf(apvts.getRawParameterValue("leak")->load(), 0.0f, 1.0f);
    const auto roomMix = clampf(apvts.getRawParameterValue("roomMix")->load(), 0.0f, 1.0f);
    const auto predelayMs = clampf(apvts.getRawParameterValue("predelayMs")->load(), 0.0f, 120.0f);
    const auto roomSize = clampf(apvts.getRawParameterValue("roomSize")->load(), 0.0f, 1.0f);
    const auto damp = clampf(apvts.getRawParameterValue("damp")->load(), 0.0f, 1.0f);
    const auto outGain = clampf(apvts.getRawParameterValue("outGain")->load(), 0.0f, 1.5f);

    juce::Reverb::Parameters rp;
    rp.roomSize = roomSize;
    rp.damping = damp;
    rp.wetLevel = 1.0f;
    rp.dryLevel = 0.0f;
    rp.width = 1.0f;
    rp.freezeMode = 0.0f;
    reverb.setParameters(rp);

    float filteredDry = 0.0f;
    float filteredWet = 0.0f;
    eqPow(roomMix, filteredDry, filteredWet);

    const auto predelaySamps = (predelayMs / 1000.0f) * sr;

    auto* l = buffer.getWritePointer(0);
    auto* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

    for (int i = 0; i < n; ++i)
    {
        const auto inL = l[i];
        const auto inR = r != nullptr ? r[i] : inL;

        auto fL = inL;
        auto fR = inR;

        fL = filters[0].hp1.processSample(fL);
        fL = filters[0].hp2.processSample(fL);
        fL = filters[0].bump.processSample(fL);
        fL = filters[0].dip.processSample(fL);
        fL = filters[0].lp1.processSample(fL);
        fL = filters[0].lp2.processSample(fL);

        fR = filters[1].hp1.processSample(fR);
        fR = filters[1].hp2.processSample(fR);
        fR = filters[1].bump.processSample(fR);
        fR = filters[1].dip.processSample(fR);
        fR = filters[1].lp1.processSample(fR);
        fR = filters[1].lp2.processSample(fR);

        filteredL[(size_t) i] = fL;
        filteredR[(size_t) i] = fR;

        channels[0].predelay[(size_t) channels[0].writePos] = fL;
        channels[1].predelay[(size_t) channels[1].writePos] = fR;
        wetL[(size_t) i] = readPredelay(channels[0], predelaySamps);
        wetR[(size_t) i] = readPredelay(channels[1], predelaySamps);

        channels[0].writePos = (channels[0].writePos + 1) % (int) channels[0].predelay.size();
        channels[1].writePos = (channels[1].writePos + 1) % (int) channels[1].predelay.size();
    }

    reverb.processStereo(wetL.data(), wetR.data(), n);

    for (int i = 0; i < n; ++i)
    {
        const auto inL = l[i];
        const auto inR = r != nullptr ? r[i] : inL;
        const auto ocL = filteredL[(size_t) i] * filteredDry + wetL[(size_t) i] * filteredWet;
        const auto ocR = filteredR[(size_t) i] * filteredDry + wetR[(size_t) i] * filteredWet;
        const auto outL = (inL * leak + ocL) * outGain;
        const auto outR = (inR * leak + ocR) * outGain;
        l[i] = clampf(outL, -1.0f, 1.0f);
        if (r != nullptr)
            r[i] = clampf(outR, -1.0f, 1.0f);
    }
}

bool OcclusionEngineAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* OcclusionEngineAudioProcessor::createEditor()
{
    return new OcclusionEngineAudioProcessorEditor(*this);
}

void OcclusionEngineAudioProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, dest);
}

void OcclusionEngineAudioProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OcclusionEngineAudioProcessor();
}
