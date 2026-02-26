#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

namespace
{
float equalPowerDry(float wet)
{
    const auto a = juce::jlimit(0.0f, 1.0f, wet) * juce::MathConstants<float>::halfPi;
    return std::cos(a);
}

float equalPowerWet(float wet)
{
    const auto a = juce::jlimit(0.0f, 1.0f, wet) * juce::MathConstants<float>::halfPi;
    return std::sin(a);
}
} // namespace

OpenMicNightAudioProcessor::OpenMicNightAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout()),
      rng(0xfeedc0de)
{
}

juce::AudioProcessorValueTreeState::ParameterLayout OpenMicNightAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto n01 = juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f);

    p.push_back(std::make_unique<juce::AudioParameterFloat>("hotMic", "Hot Mic", n01, 0.55f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("fbFreq", "FB Freq", juce::NormalisableRange<float>(200.0f, 5000.0f, 1.0f), 1800.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("ringQ", "Ring Q", juce::NormalisableRange<float>(1.0f, 40.0f, 0.1f), 16.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("fbDelayMs", "FB Delay", juce::NormalisableRange<float>(8.0f, 70.0f, 0.1f), 24.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("fbTone", "FB Tone", n01, 0.55f));

    p.push_back(std::make_unique<juce::AudioParameterFloat>("wall", "Wall", n01, 0.65f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("room", "Room", n01, 0.55f));

    p.push_back(std::make_unique<juce::AudioParameterFloat>("limit", "Limit", n01, 0.65f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("outGain", "Out", juce::NormalisableRange<float>(0.0f, 1.5f, 0.01f), 0.95f));

    return { p.begin(), p.end() };
}

const juce::String OpenMicNightAudioProcessor::getName() const { return JucePlugin_Name; }
bool OpenMicNightAudioProcessor::acceptsMidi() const { return false; }
bool OpenMicNightAudioProcessor::producesMidi() const { return false; }
bool OpenMicNightAudioProcessor::isMidiEffect() const { return false; }
double OpenMicNightAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int OpenMicNightAudioProcessor::getNumPrograms() { return 1; }
int OpenMicNightAudioProcessor::getCurrentProgram() { return 0; }
void OpenMicNightAudioProcessor::setCurrentProgram(int) {}
const juce::String OpenMicNightAudioProcessor::getProgramName(int) { return {}; }
void OpenMicNightAudioProcessor::changeProgramName(int, const juce::String&) {}

float OpenMicNightAudioProcessor::CrowdPlayer::readLoop() noexcept
{
    if (samples.empty())
        return 0.0f;
    const auto i0 = (int) readPos;
    const auto i1 = (i0 + 1) % (int) samples.size();
    const auto frac = readPos - (float) i0;
    const auto y = samples[(size_t) i0] + (samples[(size_t) i1] - samples[(size_t) i0]) * frac;
    readPos += 1.0f;
    if (readPos >= (float) samples.size())
        readPos -= (float) samples.size();
    return y;
}

float OpenMicNightAudioProcessor::clampf(float x, float lo, float hi)
{
    return juce::jlimit(lo, hi, x);
}

float OpenMicNightAudioProcessor::softClip(float x)
{
    return std::tanh(x);
}

float OpenMicNightAudioProcessor::nextSigned()
{
    return unif(rng) * 2.0f - 1.0f;
}

void OpenMicNightAudioProcessor::loadClipFromBinary(const void* data, size_t size, CrowdPlayer& out, double targetSampleRate)
{
    out.samples.clear();

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();

    auto mem = std::make_unique<juce::MemoryInputStream>(data, size, false);
    std::unique_ptr<juce::AudioFormatReader> r(fm.createReaderFor(std::move(mem)));
    if (! r)
        return;

    const auto n = (int) r->lengthInSamples;
    if (n <= 0)
        return;

    juce::AudioBuffer<float> tmp((int) r->numChannels, n);
    r->read(&tmp, 0, n, 0, true, true);

    juce::AudioBuffer<float> mono(1, n);
    mono.clear();
    for (int ch = 0; ch < tmp.getNumChannels(); ++ch)
        mono.addFrom(0, 0, tmp, ch, 0, n, 1.0f / (float) tmp.getNumChannels());

    if (std::abs(r->sampleRate - targetSampleRate) < 1.0)
    {
        out.samples.resize((size_t) n);
        std::memcpy(out.samples.data(), mono.getReadPointer(0), (size_t) n * sizeof(float));
        return;
    }

    const auto ratio = targetSampleRate / r->sampleRate;
    const auto outN = juce::jmax(1, (int) std::floor(n * ratio));
    out.samples.resize((size_t) outN);
    for (int i = 0; i < outN; ++i)
    {
        const auto src = (float) i / (float) ratio;
        const auto i0 = juce::jlimit(0, n - 1, (int) src);
        const auto i1 = juce::jlimit(0, n - 1, i0 + 1);
        const auto frac = src - (float) i0;
        const auto s0 = mono.getSample(0, i0);
        const auto s1 = mono.getSample(0, i1);
        out.samples[(size_t) i] = s0 + (s1 - s0) * frac;
    }
}

void OpenMicNightAudioProcessor::loadCrowdAssets(double sampleRate)
{
    auto loadByName = [this, sampleRate](const char* resourceId, CrowdPlayer& dst)
    {
        int size = 0;
        if (const auto* data = BinaryData::getNamedResource(resourceId, size))
            loadClipFromBinary(data, (size_t) size, dst, sampleRate);
    };

    loadByName("freesound_communitycityterracenight16943_mp3", crowdBeds[0]);
    loadByName("freesound_communitynewyorkcitynighteveningrooftopambiencetrafficbushwickbrooklynabovegroundsubwaymtrain6775_mp3", crowdBeds[1]);
    loadByName("dbsoundrainfallingatnightcarsandpeopleinthedistance248232_mp3", crowdBeds[2]);
    loadByName("banter_mp3", banterClip);
    loadByName("bandmate_intro_mp3", introClip);
    loadByName("applause_mp3", applauseClip);

    for (auto& c : crowdBeds)
        c.readPos = c.valid() ? unif(rng) * (float) c.samples.size() : 0.0f;
    if (banterClip.valid())
        banterClip.readPos = unif(rng) * (float) banterClip.samples.size();
    if (introClip.valid())
        introClip.readPos = 0.0f;
    if (applauseClip.valid())
        applauseClip.readPos = 0.0f;
}

void OpenMicNightAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);

    fbDelay.assign((size_t) juce::jmax(2048, (int) std::ceil(sampleRate * 0.25)), 0.0f);
    fbWrite = 0;

    limEnv = 0.0f;
    inputEnv = 0.0f;

    juce::Reverb::Parameters rp;
    rp.roomSize = 0.55f;
    rp.damping = 0.45f;
    rp.wetLevel = 0.25f;
    rp.dryLevel = 0.75f;
    rp.width = 1.0f;
    rp.freezeMode = 0.0f;
    reverb.setParameters(rp);
    reverb.reset();

    updateFilters(sampleRate);
}

void OpenMicNightAudioProcessor::releaseResources() {}

bool OpenMicNightAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in != out)
        return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void OpenMicNightAudioProcessor::updateFilters(double sampleRate)
{
    const auto wall = clampf(apvts.getRawParameterValue("wall")->load(), 0.0f, 1.0f);
    const auto wallLp = 9000.0f - wall * 7200.0f;
    const auto hpHz = 45.0f + wall * 80.0f;

    auto hp = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, hpHz, 0.707f);
    auto lp = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, wallLp, 0.85f);
    auto dip = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 480.0f, 0.8f, juce::Decibels::decibelsToGain(-1.2f - wall * 3.4f));

    const auto fbFreq = clampf(apvts.getRawParameterValue("fbFreq")->load(), 200.0f, 5000.0f);
    const auto ringQ = clampf(apvts.getRawParameterValue("ringQ")->load(), 1.0f, 40.0f);
    const auto fbTone = clampf(apvts.getRawParameterValue("fbTone")->load(), 0.0f, 1.0f);
    const auto fbToneHz = 1200.0f + fbTone * 7800.0f;

    auto band = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, fbFreq, ringQ);
    auto fbLp = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, fbToneHz, 0.707f);

    tone.hp.coefficients = hp;
    tone.wallLp1.coefficients = lp;
    tone.wallLp2.coefficients = lp;
    tone.wallDip.coefficients = dip;
    tone.fbBand.coefficients = band;
    tone.fbTone.coefficients = fbLp;

    juce::Reverb::Parameters rp;
    const auto room = clampf(apvts.getRawParameterValue("room")->load(), 0.0f, 1.0f);
    rp.roomSize = 0.22f + room * 0.75f;
    rp.damping = 0.2f + (1.0f - room) * 0.7f;
    rp.wetLevel = 0.05f + room * 0.5f;
    rp.dryLevel = 1.0f - rp.wetLevel * 0.75f;
    rp.width = 1.0f;
    rp.freezeMode = 0.0f;
    reverb.setParameters(rp);
}

void OpenMicNightAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi);
    juce::ScopedNoDenormals nd;

    const auto inCh = getTotalNumInputChannels();
    const auto outCh = getTotalNumOutputChannels();
    for (int ch = inCh; ch < outCh; ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());

    updateFilters(getSampleRate());
    const auto sr = (float) getSampleRate();

    const auto hot = clampf(apvts.getRawParameterValue("hotMic")->load(), 0.0f, 1.0f);
    const auto fbGain = clampf(0.03f + hot * hot * 0.97f, 0.0f, 0.97f);
    const auto fbDelayMs = clampf(apvts.getRawParameterValue("fbDelayMs")->load(), 8.0f, 70.0f);
    const auto fbDelaySamp = juce::jlimit(1, (int) fbDelay.size() - 1, (int) std::lround((fbDelayMs / 1000.0f) * sr));

    const auto limitAmt = clampf(apvts.getRawParameterValue("limit")->load(), 0.0f, 1.0f);
    const auto outGain = clampf(apvts.getRawParameterValue("outGain")->load(), 0.0f, 1.5f);
    const auto limDry = equalPowerDry(limitAmt);
    const auto limWet = equalPowerWet(limitAmt);
    const auto ceiling = 0.90f;

    const auto limAtk = std::exp(-1.0f / (0.003f * sr));
    const auto limRel = std::exp(-1.0f / (0.09f * sr));

    auto* l = buffer.getWritePointer(0);
    auto* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const auto inL = l[i];
        const auto inR = r != nullptr ? r[i] : inL;
        const auto x = 0.5f * (inL + inR);
        const auto sum = x;

        const auto fbReadIdx = (fbWrite - fbDelaySamp + (int) fbDelay.size()) % (int) fbDelay.size();
        auto fb = fbDelay[(size_t) fbReadIdx];
        fb = tone.fbBand.processSample(fb);
        fb = tone.fbTone.processSample(fb);
        fb = softClip(fb * 2.5f);
        const auto fbDyn = 1.0f - clampf(std::abs(sum) * 0.65f, 0.0f, 0.30f);
        const auto fbWriteSample = clampf((sum + fb) * fbGain * fbDyn, -0.98f, 0.98f);
        fbDelay[(size_t) fbWrite] = fbWriteSample;
        fbWrite = (fbWrite + 1) % (int) fbDelay.size();

        auto y = sum + fb * 0.78f;
        y = tone.hp.processSample(y);
        y = tone.wallDip.processSample(y);
        y = tone.wallLp1.processSample(y);
        y = tone.wallLp2.processSample(y);

        float revL = y;
        float revR = y;
        reverb.processStereo(&revL, &revR, 1);
        y = 0.5f * (revL + revR);

        auto limitedIn = y * outGain;
        const auto aa = std::abs(limitedIn);
        const auto lc = aa > limEnv ? limAtk : limRel;
        limEnv = aa + lc * (limEnv - aa);
        const auto g = limEnv > ceiling ? ceiling / (limEnv + 1.0e-6f) : 1.0f;
        auto limited = clampf(limitedIn * g, -ceiling, ceiling);

        const auto out = clampf(y * limDry + limited * limWet, -1.0f, 1.0f);
        l[i] = out;
        if (r != nullptr)
            r[i] = out;
    }
}

bool OpenMicNightAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* OpenMicNightAudioProcessor::createEditor()
{
    return new OpenMicNightAudioProcessorEditor(*this);
}

void OpenMicNightAudioProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, dest);
}

void OpenMicNightAudioProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OpenMicNightAudioProcessor();
}
