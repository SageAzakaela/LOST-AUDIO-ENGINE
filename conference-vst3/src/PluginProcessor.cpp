#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
float clampf(float x, float lo, float hi)
{
    return juce::jlimit(lo, hi, x);
}

float softClip(float x)
{
    return std::tanh(x);
}
}

ConferenceEngineAudioProcessor::ConferenceEngineAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout()),
      rng(0x636f6e66)
{
}

juce::AudioProcessorValueTreeState::ParameterLayout ConferenceEngineAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto n01 = juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f);

    p.push_back(std::make_unique<juce::AudioParameterChoice>("mode", "Mode", juce::StringArray { "Discord", "Zoom", "Skype", "Cell" }, 0));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("bandwidth", "Bandwidth", n01, 0.45f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("codec", "Codec", n01, 0.35f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("dropouts", "Dropouts", n01, 0.25f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("jitter", "Jitter Macro", n01, 0.2f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("robot", "Robot", n01, 0.12f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("noise", "Noise", n01, 0.12f));

    p.push_back(std::make_unique<juce::AudioParameterFloat>("hpHz", "HP", juce::NormalisableRange<float>(40.0f, 1200.0f, 1.0f), 260.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("lpHz", "LP", juce::NormalisableRange<float>(800.0f, 16000.0f, 1.0f), 4200.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("midHumpDb", "Mid Hump", juce::NormalisableRange<float>(0.0f, 14.0f, 0.1f), 2.2f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("midFreq", "Mid Freq", juce::NormalisableRange<float>(600.0f, 5000.0f, 1.0f), 1750.0f));

    p.push_back(std::make_unique<juce::AudioParameterChoice>("concealMode", "Conceal", juce::StringArray { "Hold", "Mute", "Interp", "Repeat" }, 0));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("packetLoss", "Packet Loss", n01, 0.18f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("packetMs", "Packet Ms", juce::NormalisableRange<float>(4.0f, 240.0f, 1.0f), 24.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("repeatMs", "Repeat Ms", juce::NormalisableRange<float>(1.0f, 300.0f, 1.0f), 42.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("jitterMs", "Jitter Ms", juce::NormalisableRange<float>(0.0f, 3.0f, 0.001f), 0.12f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("jitterRate", "Jitter Rate", juce::NormalisableRange<float>(1.0f, 220.0f, 0.1f), 34.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("gate", "Gate", n01, 0.12f));
    p.push_back(std::make_unique<juce::AudioParameterInt>("bits", "Bits", 4, 16, 12));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("rate", "Rate", juce::NormalisableRange<float>(6000.0f, 48000.0f, 1.0f), 24000.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("ceiling", "Ceiling", juce::NormalisableRange<float>(0.2f, 1.0f, 0.001f), 0.92f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("outGain", "Out Gain", juce::NormalisableRange<float>(0.0f, 1.5f, 0.01f), 0.98f));

    return { p.begin(), p.end() };
}

const juce::String ConferenceEngineAudioProcessor::getName() const { return JucePlugin_Name; }
bool ConferenceEngineAudioProcessor::acceptsMidi() const { return false; }
bool ConferenceEngineAudioProcessor::producesMidi() const { return false; }
bool ConferenceEngineAudioProcessor::isMidiEffect() const { return false; }
double ConferenceEngineAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int ConferenceEngineAudioProcessor::getNumPrograms() { return 1; }
int ConferenceEngineAudioProcessor::getCurrentProgram() { return 0; }
void ConferenceEngineAudioProcessor::setCurrentProgram(int) {}
const juce::String ConferenceEngineAudioProcessor::getProgramName(int) { return {}; }
void ConferenceEngineAudioProcessor::changeProgramName(int, const juce::String&) {}

float ConferenceEngineAudioProcessor::nextWhite()
{
    return unif(rng) * 2.0f - 1.0f;
}

void ConferenceEngineAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);

    core.delay.assign((size_t) juce::jmax(256, (int) std::ceil(sampleRate * 0.012)), 0.0f);
    core.di = 0;
    core.jPhase = unif(rng) * juce::MathConstants<float>::twoPi;
    core.jNoise = 0.0f;

    core.ring.assign((size_t) juce::jmax(1024, (int) std::ceil(sampleRate * 0.4)), 0.0f);
    core.ri = 0;

    core.env = 0.0f;
    core.gateGain = 1.0f;
    core.packetRemain = 0;
    core.packetTotal = 0;
    core.inDrop = false;
    core.dropFade = 0.0f;
    core.lastGood = 0.0f;
    core.dropStart = 0.0f;
    core.hold = 0.0f;
    core.holdPeriod = 1;
    core.rateAcc = 0;

    core.robotRemain = 0;
    core.robotLen = 0;
    core.robotI = 0;
    core.robotBuf.assign((size_t) juce::jmax(64, (int) std::ceil(sampleRate * 0.09)), 0.0f);

    updateFilters(sampleRate);
}

void ConferenceEngineAudioProcessor::releaseResources() {}

bool ConferenceEngineAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in != out)
        return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void ConferenceEngineAudioProcessor::updateFilters(double sampleRate)
{
    const auto hpHz = apvts.getRawParameterValue("hpHz")->load();
    const auto lpHz = apvts.getRawParameterValue("lpHz")->load();
    const auto midDb = apvts.getRawParameterValue("midHumpDb")->load();
    const auto midFreq = apvts.getRawParameterValue("midFreq")->load();

    auto hp = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, clampf(hpHz, 10.0f, 20000.0f), 0.707f);
    auto lp = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, clampf(lpHz, 20.0f, 20000.0f), 0.85f);
    auto hump = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, clampf(midFreq, 100.0f, 20000.0f), 1.25f, juce::Decibels::decibelsToGain(midDb));
    auto dip = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 650.0f, 0.9f, juce::Decibels::decibelsToGain(-0.35f * midDb));

    tone.hp1.coefficients = hp;
    tone.hp2.coefficients = hp;
    tone.dip.coefficients = dip;
    tone.hump.coefficients = hump;
    tone.lp1.coefficients = lp;
    tone.lp2.coefficients = lp;
}

float ConferenceEngineAudioProcessor::readDelay(float delaySamps) const
{
    const auto len = (int) core.delay.size();
    const auto read = (float) core.di - delaySamps;
    auto i0 = (int) std::floor(read);
    while (i0 < 0)
        i0 += len;
    i0 %= len;
    const auto i1 = (i0 + 1) % len;
    const auto frac = read - std::floor(read);
    return core.delay[(size_t) i0] * (1.0f - frac) + core.delay[(size_t) i1] * frac;
}

void ConferenceEngineAudioProcessor::startPacket(int packetSamps, float lossProb)
{
    core.packetTotal = juce::jmax(1, packetSamps);
    core.packetRemain = core.packetTotal;
    const auto wasDrop = core.inDrop;
    core.inDrop = unif(rng) < lossProb;
    if (core.inDrop)
        core.dropStart = core.lastGood;
    else if (wasDrop)
        core.dropFade = 0.12f;
}

void ConferenceEngineAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi);
    juce::ScopedNoDenormals nd;

    const auto inCh = getTotalNumInputChannels();
    const auto outCh = getTotalNumOutputChannels();
    for (int ch = inCh; ch < outCh; ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());

    updateFilters(getSampleRate());

    const auto n = buffer.getNumSamples();
    const auto sr = (float) getSampleRate();

    const auto mode = juce::jlimit(0, 3, (int) std::lround(apvts.getRawParameterValue("mode")->load()));
    const auto concealMode = juce::jlimit(0, 3, (int) std::lround(apvts.getRawParameterValue("concealMode")->load()));
    const auto packetLoss = clampf(apvts.getRawParameterValue("packetLoss")->load(), 0.0f, 1.0f);
    const auto packetMs = clampf(apvts.getRawParameterValue("packetMs")->load(), 4.0f, 240.0f);
    const auto repeatMs = clampf(apvts.getRawParameterValue("repeatMs")->load(), 1.0f, 300.0f);
    const auto jitterMs = clampf(apvts.getRawParameterValue("jitterMs")->load(), 0.0f, 3.0f);
    const auto jitterRate = clampf(apvts.getRawParameterValue("jitterRate")->load(), 1.0f, 220.0f);
    const auto gate = clampf(apvts.getRawParameterValue("gate")->load(), 0.0f, 1.0f);
    const auto bits = juce::jlimit(4, 16, (int) std::lround(apvts.getRawParameterValue("bits")->load()));
    const auto rateParam = clampf(apvts.getRawParameterValue("rate")->load(), 6000.0f, 48000.0f);
    const auto robot = clampf(apvts.getRawParameterValue("robot")->load(), 0.0f, 1.0f);
    const auto noise = clampf(apvts.getRawParameterValue("noise")->load(), 0.0f, 1.0f);
    const auto ceiling = clampf(apvts.getRawParameterValue("ceiling")->load(), 0.2f, 1.0f);
    const auto outGain = clampf(apvts.getRawParameterValue("outGain")->load(), 0.0f, 1.5f);

    const auto modeLossScale = mode == 3 ? 1.35f : (mode == 2 ? 1.05f : (mode == 1 ? 0.95f : 1.0f));
    const auto lossProb = clampf(packetLoss * modeLossScale, 0.0f, 1.0f);

    const auto packetSamps = juce::jmax(1, (int) std::lround((packetMs / 1000.0f) * sr));
    if (core.packetRemain <= 0 || core.packetTotal != packetSamps)
        startPacket(packetSamps, lossProb);

    const auto jDepth = (jitterMs / 1000.0f) * sr;
    const auto jInc = (juce::MathConstants<float>::twoPi * jitterRate) / sr;

    core.holdPeriod = juce::jmax(1, (int) std::lround(sr / juce::jmax(6000.0f, juce::jmin(sr, rateParam))));
    const auto q = (float) std::pow(2.0, bits - 1);

    const auto envAtk = std::exp(-1.0f / (0.003f * sr));
    const auto envRel = std::exp(-1.0f / (0.06f * sr));
    const auto gateThr = 0.002f + gate * gate * 0.06f;

    const auto robotRate = 0.05f + robot * 1.25f;
    const auto robotP = robotRate / sr;

    auto* l = buffer.getWritePointer(0);
    auto* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

    for (int i = 0; i < n; ++i)
    {
        const auto inL = l[i];
        const auto inR = r != nullptr ? r[i] : inL;
        auto x = 0.5f * (inL + inR);

        core.delay[(size_t) core.di] = x;
        core.di = (core.di + 1) % (int) core.delay.size();

        core.jNoise = 0.995f * core.jNoise + 0.005f * nextWhite();
        const auto jMod = 0.65f + 0.35f * std::sin(core.jPhase);
        core.jPhase += jInc;
        if (core.jPhase > juce::MathConstants<float>::twoPi)
            core.jPhase -= juce::MathConstants<float>::twoPi;
        const auto jSamps = jDepth * (jMod + 0.25f * core.jNoise);
        x = readDelay(juce::jmax(0.0f, jSamps));

        x = tone.hp1.processSample(x);
        x = tone.hp2.processSample(x);
        x = tone.dip.processSample(x);
        x = tone.hump.processSample(x);
        x = tone.lp1.processSample(x);
        x = tone.lp2.processSample(x);

        const auto absx = std::abs(x);
        const auto e = absx > core.env ? envAtk : envRel;
        core.env = (1.0f - e) * absx + e * core.env;
        const auto wantGate = core.env < gateThr ? 0.1f : 1.0f;
        core.gateGain = 0.995f * core.gateGain + 0.005f * wantGate;
        x *= core.gateGain;

        if (robot > 0.0001f && core.robotRemain <= 0 && unif(rng) < robotP)
        {
            const auto durMs = 18.0f + robot * 90.0f;
            core.robotLen = juce::jlimit(8, (int) core.robotBuf.size(), (int) std::lround((durMs / 1000.0f) * sr));
            core.robotI = 0;
            core.robotRemain = juce::jmax(8, (int) std::lround((float) core.robotLen * (1.2f + robot * 3.2f)));
            for (int k = 0; k < core.robotLen; ++k)
            {
                const auto idx = (core.di - 1 - k + (int) core.delay.size()) % (int) core.delay.size();
                core.robotBuf[(size_t) (core.robotLen - 1 - k)] = core.delay[(size_t) idx];
            }
        }
        if (core.robotRemain > 0 && core.robotLen > 0)
        {
            x = core.robotBuf[(size_t) core.robotI];
            core.robotI = (core.robotI + 1) % core.robotLen;
            --core.robotRemain;
        }

        if (core.inDrop)
        {
            if (concealMode == 1)
                x = 0.0f;
            else if (concealMode == 3)
            {
                const auto back = juce::jmax(1, (int) std::lround((repeatMs / 1000.0f) * sr));
                const auto idx = (core.ri - back + (int) core.ring.size()) % (int) core.ring.size();
                x = core.ring[(size_t) idx];
            }
            else
                x = core.lastGood;
        }
        else
        {
            core.lastGood = x;
        }

        --core.packetRemain;
        if (core.packetRemain <= 0)
            startPacket(packetSamps, lossProb);

        if (!core.inDrop && core.dropFade > 0.0f)
        {
            const auto a = clampf(core.dropFade, 0.0f, 1.0f);
            x = core.dropStart * (1.0f - a) + x * a;
            core.dropFade += 0.08f;
            if (core.dropFade >= 1.0f)
                core.dropFade = 0.0f;
        }

        ++core.rateAcc;
        if (core.rateAcc >= core.holdPeriod)
        {
            core.rateAcc = 0;
            core.hold = x;
        }
        x = core.hold;

        x = std::round(x * q) / q;
        x += nextWhite() * (noise * (0.002f + (mode == 3 ? 0.002f : 0.001f)));

        x *= outGain;
        x = softClip(x);
        x = clampf(x, -ceiling, ceiling);

        l[i] = x;
        if (r != nullptr)
            r[i] = x;

        core.ring[(size_t) core.ri] = x;
        core.ri = (core.ri + 1) % (int) core.ring.size();
    }
}

bool ConferenceEngineAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* ConferenceEngineAudioProcessor::createEditor()
{
    return new ConferenceEngineAudioProcessorEditor(*this);
}

void ConferenceEngineAudioProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, dest);
}

void ConferenceEngineAudioProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ConferenceEngineAudioProcessor();
}
