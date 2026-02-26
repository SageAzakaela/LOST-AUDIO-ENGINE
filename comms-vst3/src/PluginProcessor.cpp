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

int roundToInt(float x)
{
    return (int) std::lround(x);
}
}

CommsEngineAudioProcessor::CommsEngineAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout()),
      rng(0x6f6d6d73)
{
}

juce::AudioProcessorValueTreeState::ParameterLayout CommsEngineAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto n01 = juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f);

    p.push_back(std::make_unique<juce::AudioParameterChoice>("mode", "Mode", juce::StringArray { "Landline", "Cell", "Intercom", "PA", "Alarm" }, 0));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("bandwidth", "Bandwidth", n01, 0.4f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("drive", "Drive", n01, 0.35f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("glitch", "Glitch", n01, 0.2f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("noise", "Noise", n01, 0.18f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("alarmTone", "Alarm Tone", false));

    p.push_back(std::make_unique<juce::AudioParameterFloat>("hpHz", "HP", juce::NormalisableRange<float>(40.0f, 1200.0f, 1.0f), 280.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("lpHz", "LP", juce::NormalisableRange<float>(800.0f, 14000.0f, 1.0f), 3400.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("midHumpDb", "Mid Hump", juce::NormalisableRange<float>(0.0f, 14.0f, 0.1f), 3.5f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("midFreq", "Mid Freq", juce::NormalisableRange<float>(600.0f, 5000.0f, 1.0f), 1850.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("comp", "Comp", n01, 0.45f));
    p.push_back(std::make_unique<juce::AudioParameterInt>("bits", "Bits", 4, 16, 12));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("rate", "Rate", juce::NormalisableRange<float>(6000.0f, 48000.0f, 1.0f), 24000.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("packet", "Packet", n01, 0.2f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("packetMs", "Packet Ms", juce::NormalisableRange<float>(8.0f, 160.0f, 1.0f), 28.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("hum", "Hum", n01, 0.25f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("hiss", "Hiss", n01, 0.22f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("toneMix", "Tone Mix", n01, 0.35f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("ceiling", "Ceiling", juce::NormalisableRange<float>(0.2f, 1.0f, 0.001f), 0.92f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("outGain", "Out Gain", juce::NormalisableRange<float>(0.0f, 1.5f, 0.01f), 0.95f));

    p.push_back(std::make_unique<juce::AudioParameterFloat>("echoMix", "Echo Mix", n01, 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("echoMs", "Echo Ms", juce::NormalisableRange<float>(10.0f, 2500.0f, 1.0f), 180.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("echoFb", "Echo Fb", juce::NormalisableRange<float>(0.0f, 0.92f, 0.001f), 0.28f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("echoTone", "Echo Tone", n01, 0.55f));

    p.push_back(std::make_unique<juce::AudioParameterFloat>("verbMix", "Verb Mix", n01, 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("verbMs", "Verb Ms", juce::NormalisableRange<float>(35.0f, 2500.0f, 1.0f), 240.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("verbDamp", "Verb Damp", n01, 0.45f));

    return { p.begin(), p.end() };
}

const juce::String CommsEngineAudioProcessor::getName() const { return JucePlugin_Name; }
bool CommsEngineAudioProcessor::acceptsMidi() const { return false; }
bool CommsEngineAudioProcessor::producesMidi() const { return false; }
bool CommsEngineAudioProcessor::isMidiEffect() const { return false; }
double CommsEngineAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int CommsEngineAudioProcessor::getNumPrograms() { return 1; }
int CommsEngineAudioProcessor::getCurrentProgram() { return 0; }
void CommsEngineAudioProcessor::setCurrentProgram(int) {}
const juce::String CommsEngineAudioProcessor::getProgramName(int) { return {}; }
void CommsEngineAudioProcessor::changeProgramName(int, const juce::String&) {}

float CommsEngineAudioProcessor::nextWhite()
{
    return unif(rng) * 2.0f - 1.0f;
}

void CommsEngineAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    const auto echoLen = juce::jmax(64, (int) std::ceil(sampleRate * 2.5));
    echo.delay.assign((size_t) echoLen, 0.0f);
    echo.writePos = 0;
    echo.fbTone = 0.0f;

    core = {};

    verbL.assign((size_t) juce::jmax(32, samplesPerBlock), 0.0f);
    verbR.assign((size_t) juce::jmax(32, samplesPerBlock), 0.0f);

    juce::Reverb::Parameters rp;
    rp.roomSize = 0.4f;
    rp.damping = 0.45f;
    rp.wetLevel = 1.0f;
    rp.dryLevel = 0.0f;
    rp.width = 1.0f;
    rp.freezeMode = 0.0f;
    reverb.setParameters(rp);

    updateFilters(sampleRate);
}

void CommsEngineAudioProcessor::releaseResources() {}

bool CommsEngineAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in != out)
        return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void CommsEngineAudioProcessor::updateFilters(double sampleRate)
{
    const auto hpHz = apvts.getRawParameterValue("hpHz")->load();
    const auto lpHz = apvts.getRawParameterValue("lpHz")->load();
    const auto midHumpDb = apvts.getRawParameterValue("midHumpDb")->load();
    const auto midFreq = apvts.getRawParameterValue("midFreq")->load();

    auto hp = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, clampf(hpHz, 10.0f, 20000.0f), 0.707f);
    auto lp = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, clampf(lpHz, 20.0f, 20000.0f), 0.85f);
    auto hump = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, clampf(midFreq, 100.0f, 20000.0f), 1.25f, juce::Decibels::decibelsToGain(midHumpDb));
    auto dip = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 650.0f, 0.9f, juce::Decibels::decibelsToGain(-0.35f * midHumpDb));

    tone.hp1.coefficients = hp;
    tone.hp2.coefficients = hp;
    tone.lp1.coefficients = lp;
    tone.lp2.coefficients = lp;
    tone.hump.coefficients = hump;
    tone.dip.coefficients = dip;
}

float CommsEngineAudioProcessor::readEcho(float delaySamps) const
{
    const auto len = (int) echo.delay.size();
    const auto read = (float) echo.writePos - delaySamps;
    auto i0 = (int) std::floor(read);
    while (i0 < 0)
        i0 += len;
    i0 %= len;
    const auto i1 = (i0 + 1) % len;
    const auto frac = read - std::floor(read);
    return echo.delay[(size_t) i0] * (1.0f - frac) + echo.delay[(size_t) i1] * frac;
}

void CommsEngineAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi);
    juce::ScopedNoDenormals nd;

    const auto inCh = getTotalNumInputChannels();
    const auto outCh = getTotalNumOutputChannels();
    for (int ch = inCh; ch < outCh; ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());

    updateFilters(getSampleRate());

    const auto n = buffer.getNumSamples();
    if ((int) verbL.size() < n)
    {
        verbL.resize((size_t) n);
        verbR.resize((size_t) n);
    }

    const auto sr = (float) getSampleRate();

    const auto drive = clampf(apvts.getRawParameterValue("drive")->load(), 0.0f, 1.0f);
    const auto comp = clampf(apvts.getRawParameterValue("comp")->load(), 0.0f, 1.0f);
    const auto bits = juce::jlimit(4, 16, (int) std::lround(apvts.getRawParameterValue("bits")->load()));
    const auto rate = clampf(apvts.getRawParameterValue("rate")->load(), 6000.0f, 48000.0f);
    const auto packet = clampf(apvts.getRawParameterValue("packet")->load(), 0.0f, 1.0f);
    const auto packetMs = clampf(apvts.getRawParameterValue("packetMs")->load(), 8.0f, 160.0f);
    const auto hum = clampf(apvts.getRawParameterValue("hum")->load(), 0.0f, 1.0f);
    const auto hiss = clampf(apvts.getRawParameterValue("hiss")->load(), 0.0f, 1.0f);
    const auto toneMix = clampf(apvts.getRawParameterValue("toneMix")->load(), 0.0f, 1.0f);
    const auto alarm = apvts.getRawParameterValue("alarmTone")->load() > 0.5f;
    const auto mode = juce::jlimit(0, 4, (int) std::lround(apvts.getRawParameterValue("mode")->load()));
    const auto ceiling = clampf(apvts.getRawParameterValue("ceiling")->load(), 0.2f, 1.0f);
    const auto outGain = clampf(apvts.getRawParameterValue("outGain")->load(), 0.0f, 1.5f);

    const auto echoMix = clampf(apvts.getRawParameterValue("echoMix")->load(), 0.0f, 1.0f);
    const auto echoMs = clampf(apvts.getRawParameterValue("echoMs")->load(), 10.0f, 2500.0f);
    const auto echoFb = clampf(apvts.getRawParameterValue("echoFb")->load(), 0.0f, 0.92f);
    const auto echoTone = clampf(apvts.getRawParameterValue("echoTone")->load(), 0.0f, 1.0f);
    const auto verbMix = clampf(apvts.getRawParameterValue("verbMix")->load(), 0.0f, 1.0f);
    const auto verbMs = clampf(apvts.getRawParameterValue("verbMs")->load(), 35.0f, 2500.0f);
    const auto verbDamp = clampf(apvts.getRawParameterValue("verbDamp")->load(), 0.0f, 1.0f);

    const auto target = 0.18f;
    const auto envAtk = std::exp(-1.0f / (0.006f * sr));
    const auto envRel = std::exp(-1.0f / (0.11f * sr));
    const auto baseDrive = 1.0f + drive * 14.0f;
    const auto asym = 0.06f * drive;
    const auto compPow = 1.0f + comp * 1.8f;
    const auto maxGain = 6.5f;

    const auto qLevels = juce::jmax(1.0f, (float) ((1 << (bits - 1)) - 1));
    const auto basePeriod = juce::jmax(1, roundToInt(sr / juce::jmin(sr, rate)));

    const auto packetSamples = juce::jmax(1, roundToInt((packetMs / 1000.0f) * sr));
    const auto dropSlew = 1.0f / 48.0f;

    const auto humHz = mode == 1 ? 180.0f : (mode == 2 ? 50.0f : (mode == 3 ? 120.0f : 60.0f));
    const auto humDepth = hum * hum * (mode == 3 ? 0.03f : 0.02f);
    const auto hissDepth = hiss * hiss * 0.035f;

    const auto warbleRate = mode == 4 ? 2.1f : 2.7f;
    const auto toneBaseA = mode == 4 ? 960.0f : 880.0f;
    const auto toneBaseB = mode == 4 ? 1400.0f : 1200.0f;
    const auto toneDepth = alarm ? (0.14f + 0.25f * toneMix) : 0.0f;

    const auto echoWetGain = juce::jmin(1.25f, echoMix * 0.95f);
    const auto echoDelaySamps = (echoMs / 1000.0f) * sr;
    const auto echoToneHz = 900.0f + echoTone * 7500.0f;
    const auto echoAlpha = std::exp((-2.0f * juce::MathConstants<float>::pi * echoToneHz) / sr);

    const auto verbWetGain = juce::jmin(1.35f, verbMix * 1.15f);
    const auto wetAmt = juce::jmax(echoMix, verbMix);
    const auto dryGain = juce::jmax(0.55f, 1.0f - wetAmt * 0.5f);

    juce::Reverb::Parameters rp;
    rp.roomSize = clampf((verbMs - 35.0f) / (2500.0f - 35.0f), 0.0f, 1.0f);
    rp.damping = verbDamp;
    rp.wetLevel = 1.0f;
    rp.dryLevel = 0.0f;
    rp.width = 1.0f;
    rp.freezeMode = 0.0f;
    reverb.setParameters(rp);

    auto* l = buffer.getWritePointer(0);
    auto* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

    if (core.dropBlockRemain <= 0)
        core.dropBlockRemain = packetSamples;

    const auto limAtk = std::exp(-1.0f / (0.002f * sr));
    const auto limRel = std::exp(-1.0f / (0.06f * sr));

    for (int i = 0; i < n; ++i)
    {
        const auto inL = l[i];
        const auto inR = r != nullptr ? r[i] : inL;
        const auto inMono = (inL + inR) * 0.5f;

        float x = inMono;
        x = tone.hp1.processSample(x);
        x = tone.hp2.processSample(x);
        x = tone.dip.processSample(x);
        x = tone.hump.processSample(x);
        x = tone.lp1.processSample(x);
        x = tone.lp2.processSample(x);

        const auto a = std::abs(x);
        const auto ec = a > core.env ? envAtk : envRel;
        core.env = a + ec * (core.env - a);

        const auto env = core.env + 1.0e-6f;
        const auto wantGain = std::pow(target / env, compPow * 0.35f);
        const auto agc = clampf(wantGain, 0.2f, maxGain);

        auto y = x * agc;
        y = softClip((y + asym) * baseDrive) - asym * 0.75f;

        if (core.holdCount <= 0)
        {
            const auto wob = packet * 0.35f;
            const auto j = wob > 0.0f ? nextWhite() * wob : 0.0f;
            core.holdPeriod = juce::jmax(1, roundToInt((float) basePeriod * (1.0f + j)));
            core.hold = y;
            core.holdCount = core.holdPeriod;
        }
        y = core.hold;
        --core.holdCount;

        y = std::round(clampf(y, -1.0f, 1.0f) * qLevels) / qLevels;

        if (core.dropBlockRemain <= 0)
        {
            core.dropBlockRemain = packetSamples;
            const auto doDrop = unif(rng) < (packet * packet);
            core.dropRemain = doDrop ? packetSamples : 0;
        }
        --core.dropBlockRemain;

        core.dropTarget = core.dropRemain > 0 ? 0.0f : 1.0f;
        core.dropGain = clampf(core.dropGain + (core.dropTarget - core.dropGain) * dropSlew, 0.0f, 1.0f);
        if (core.dropRemain > 0)
            --core.dropRemain;
        y *= core.dropGain;

        core.humPhase += (juce::MathConstants<float>::twoPi * humHz) / sr;
        if (core.humPhase > juce::MathConstants<float>::twoPi)
            core.humPhase -= juce::MathConstants<float>::twoPi;
        y += std::sin(core.humPhase) * humDepth + nextWhite() * hissDepth;

        if (alarm && toneMix > 0.0001f)
        {
            core.warblePhase += (juce::MathConstants<float>::twoPi * warbleRate) / sr;
            if (core.warblePhase > juce::MathConstants<float>::twoPi)
                core.warblePhase -= juce::MathConstants<float>::twoPi;
            const auto w = 0.5f + 0.5f * std::sin(core.warblePhase);
            const auto f1 = toneBaseA * (0.96f + 0.08f * w);
            const auto f2 = toneBaseB * (1.03f - 0.06f * w);

            core.tonePhase += (juce::MathConstants<float>::twoPi * f1) / sr;
            core.tonePhase2 += (juce::MathConstants<float>::twoPi * f2) / sr;
            if (core.tonePhase > juce::MathConstants<float>::twoPi)
                core.tonePhase -= juce::MathConstants<float>::twoPi;
            if (core.tonePhase2 > juce::MathConstants<float>::twoPi)
                core.tonePhase2 -= juce::MathConstants<float>::twoPi;

            const auto toneSig = (std::sin(core.tonePhase) + 0.6f * std::sin(core.tonePhase2)) * toneDepth;
            y = y * (1.0f - 0.55f * toneMix) + toneSig * (0.55f * toneMix) + y * (0.12f * toneMix);
        }

        const auto echoTap = readEcho(echoDelaySamps);
        echo.fbTone = (1.0f - echoAlpha) * echoTap + echoAlpha * echo.fbTone;
        echo.delay[(size_t) echo.writePos] = y + echo.fbTone * echoFb;
        echo.writePos = (echo.writePos + 1) % (int) echo.delay.size();

        verbL[(size_t) i] = y;
        verbR[(size_t) i] = y;

        auto dryPost = y;
        dryPost *= outGain;
        const auto dryAbs = std::abs(dryPost);
        const auto lc = dryAbs > core.limEnv ? limAtk : limRel;
        core.limEnv = dryAbs + lc * (core.limEnv - dryAbs);
        const auto lg = core.limEnv > ceiling ? ceiling / (core.limEnv + 1.0e-6f) : 1.0f;
        dryPost *= lg;

        const auto echoPost = echoTap * echoWetGain;
        const auto sumMono = dryPost * dryGain + echoPost;
        l[i] = sumMono;
        if (r != nullptr)
            r[i] = sumMono;
    }

    reverb.processStereo(verbL.data(), verbR.data(), n);

    for (int i = 0; i < n; ++i)
    {
        auto outMono = l[i] + verbL[(size_t) i] * verbWetGain;
        outMono = clampf(outMono, -ceiling, ceiling);
        l[i] = outMono;
        if (r != nullptr)
            r[i] = outMono;
    }
}

bool CommsEngineAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* CommsEngineAudioProcessor::createEditor()
{
    return new CommsEngineAudioProcessorEditor(*this);
}

void CommsEngineAudioProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, dest);
}

void CommsEngineAudioProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CommsEngineAudioProcessor();
}
