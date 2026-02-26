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

CDEngineAudioProcessor::CDEngineAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout()),
      rng(0x4344454e)
{
}

juce::AudioProcessorValueTreeState::ParameterLayout CDEngineAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto n01 = juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f);

    p.push_back(std::make_unique<juce::AudioParameterFloat>("clarity", "Clarity", n01, 0.65f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("damage", "Damage", n01, 0.25f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("tracking", "Tracking", n01, 0.22f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("jitterMacro", "Jitter", n01, 0.18f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("carComp", "Car Comp", n01, 0.20f));

    p.push_back(std::make_unique<juce::AudioParameterChoice>("mode", "Concealment", juce::StringArray { "Hold", "Mute", "Interp", "Repeat" }, 0));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("errorRate", "Error Rate", n01, 0.18f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("burstMs", "Burst", juce::NormalisableRange<float>(1.0f, 600.0f, 1.0f), 24.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("repeatMs", "Repeat", juce::NormalisableRange<float>(1.0f, 600.0f, 1.0f), 42.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("scratchRate", "Scratch Rate", n01, 0.25f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("scratchAmt", "Scratch Amt", n01, 0.35f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("jitterMs", "Jitter Depth", juce::NormalisableRange<float>(0.0f, 3.0f, 0.001f), 0.18f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("jitterRate", "Jitter Rate", juce::NormalisableRange<float>(1.0f, 240.0f, 0.1f), 38.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("hfLoss", "HF Loss", n01, 0.10f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("servoNoise", "Servo Noise", n01, 0.12f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("softClip", "Soft Clip", true));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("ceiling", "Ceiling", juce::NormalisableRange<float>(0.2f, 1.0f, 0.001f), 0.94f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("outGain", "Output Gain", juce::NormalisableRange<float>(0.0f, 1.5f, 0.01f), 0.98f));

    return { p.begin(), p.end() };
}

const juce::String CDEngineAudioProcessor::getName() const { return JucePlugin_Name; }
bool CDEngineAudioProcessor::acceptsMidi() const { return false; }
bool CDEngineAudioProcessor::producesMidi() const { return false; }
bool CDEngineAudioProcessor::isMidiEffect() const { return false; }
double CDEngineAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int CDEngineAudioProcessor::getNumPrograms() { return 1; }
int CDEngineAudioProcessor::getCurrentProgram() { return 0; }
void CDEngineAudioProcessor::setCurrentProgram(int) {}
const juce::String CDEngineAudioProcessor::getProgramName(int) { return {}; }
void CDEngineAudioProcessor::changeProgramName(int, const juce::String&) {}

float CDEngineAudioProcessor::nextWhite()
{
    return unif(rng) * 2.0f - 1.0f;
}

float CDEngineAudioProcessor::readDelay(float delaySamps) const
{
    const auto len = (int) core.delay.size();
    if (len < 2)
        return 0.0f;

    const auto read = (float) core.di - delaySamps;
    auto r0 = (int) std::floor(read);
    while (r0 < 0)
        r0 += len;
    r0 %= len;
    const auto r1 = (r0 + 1) % len;
    const auto frac = read - std::floor(read);
    return core.delay[(size_t) r0] * (1.0f - frac) + core.delay[(size_t) r1] * frac;
}

void CDEngineAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);

    const auto delayLen = juce::jmax(256, (int) std::ceil(sampleRate * 0.01));
    const auto ringLen = juce::jmax(1024, (int) std::ceil(sampleRate * 0.35));

    core.delay.assign((size_t) delayLen, 0.0f);
    core.di = 0;

    core.ring.assign((size_t) ringLen, 0.0f);
    core.ri = 0;

    core.jPhase = unif(rng);
    core.jNoise = 0.0f;

    core.errRemain = 0;
    core.errTotal = 0;
    core.lastGood = 0.0f;
    core.errStart = 0.0f;
    core.errEnd = 0.0f;

    core.clickRemain = 0;
    core.clickTotal = 0;
    core.clickAmp = 0.0f;
    core.clickSign = 1.0f;

    core.servoPhaseA = unif(rng);
    core.servoPhaseB = unif(rng);

    core.hfZ = 0.0f;
    core.limEnv = 0.0f;
}

void CDEngineAudioProcessor::releaseResources() {}

bool CDEngineAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in != out)
        return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void CDEngineAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi);
    juce::ScopedNoDenormals nd;

    const auto inCh = getTotalNumInputChannels();
    const auto outCh = getTotalNumOutputChannels();
    for (int ch = inCh; ch < outCh; ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());

    const auto sr = (float) getSampleRate();
    const auto mode = juce::jlimit(0, 3, (int) apvts.getRawParameterValue("mode")->load());
    const auto errorRate = clampf(apvts.getRawParameterValue("errorRate")->load(), 0.0f, 1.0f);
    const auto burstMs = clampf(apvts.getRawParameterValue("burstMs")->load(), 1.0f, 600.0f);
    const auto repeatMs = clampf(apvts.getRawParameterValue("repeatMs")->load(), 1.0f, 600.0f);
    const auto scratchRate = clampf(apvts.getRawParameterValue("scratchRate")->load(), 0.0f, 1.0f);
    const auto scratchAmt = clampf(apvts.getRawParameterValue("scratchAmt")->load(), 0.0f, 1.0f);
    const auto jitterMs = clampf(apvts.getRawParameterValue("jitterMs")->load(), 0.0f, 3.0f);
    const auto jitterRate = clampf(apvts.getRawParameterValue("jitterRate")->load(), 1.0f, 240.0f);
    const auto hfLoss = clampf(apvts.getRawParameterValue("hfLoss")->load(), 0.0f, 1.0f);
    const auto servoNoise = clampf(apvts.getRawParameterValue("servoNoise")->load(), 0.0f, 1.0f);
    const auto doSoftClip = apvts.getRawParameterValue("softClip")->load() > 0.5f;
    const auto ceiling = clampf(apvts.getRawParameterValue("ceiling")->load(), 0.2f, 1.0f);
    const auto outGain = clampf(apvts.getRawParameterValue("outGain")->load(), 0.0f, 1.5f);
    const auto carComp = clampf(apvts.getRawParameterValue("carComp")->load(), 0.0f, 1.0f);

    const auto burstSamps = juce::jmax(1, (int) std::round((burstMs / 1000.0f) * sr));
    const auto repeatSamps = juce::jmax(1, (int) std::round((repeatMs / 1000.0f) * sr));

    const auto depthSamps = (juce::jmin(1.6f, jitterMs) / 1000.0f) * sr;
    const auto jHz = 8.0f + jitterRate;
    const auto jNoiseAmt = 0.35f + 0.4f * errorRate;

    const auto clickP = (0.000005f + scratchRate * scratchRate * 0.0002f) * (0.6f + 0.9f * errorRate);
    const auto clickLenMin = juce::jmax(2, (int) std::round((0.25f / 1000.0f) * sr));
    const auto clickLenMax = juce::jmax(clickLenMin + 1, (int) std::round((4.0f / 1000.0f) * sr));

    const auto servo = servoNoise * servoNoise;
    const auto humHz = 120.0f + 60.0f * servo;
    const auto whirrHz = 420.0f + 280.0f * servo;
    const auto servoDepth = servo * 0.02f;
    const auto chatterDepth = servo * 0.01f;

    const auto hfCut = 1800.0f + (1.0f - hfLoss) * 16000.0f;
    const auto a = std::exp((-2.0f * juce::MathConstants<float>::pi * hfCut) / sr);

    const auto limAtk = std::exp(-1.0f / (0.002f * sr));
    const auto limRel = std::exp(-1.0f / (0.06f * sr));

    const auto errP = (0.0000008f + errorRate * errorRate * 0.00005f) * (1.0f + 2.5f * scratchRate);

    std::array<float*, 2> writePtrs { nullptr, nullptr };
    for (int ch = 0; ch < inCh && ch < 2; ++ch)
        writePtrs[(size_t) ch] = buffer.getWritePointer(ch);

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const auto xL = (inCh > 0 && writePtrs[0] != nullptr) ? writePtrs[0][i] : 0.0f;
        const auto xR = (inCh > 1 && writePtrs[1] != nullptr) ? writePtrs[1][i] : xL;
        const auto x = (inCh > 1) ? 0.5f * (xL + xR) : xL;

        core.ring[(size_t) core.ri] = x;
        core.ri = (core.ri + 1) % (int) core.ring.size();

        core.delay[(size_t) core.di] = x;
        const auto n = nextWhite();
        core.jNoise = core.jNoise * 0.995f + n * 0.005f;
        core.jPhase += jHz / sr;
        if (core.jPhase >= 1.0f)
            core.jPhase -= 1.0f;
        const auto lfo = std::sin(core.jPhase * juce::MathConstants<float>::twoPi);
        const auto mod = (lfo + core.jNoise * jNoiseAmt) * 0.5f;
        const auto d = juce::jmax(0.0f, depthSamps * clampf(mod, -1.0f, 1.0f));

        auto y = readDelay(d);
        core.di = (core.di + 1) % (int) core.delay.size();

        if (core.errRemain <= 0 && unif(rng) < errP)
        {
            core.errRemain = burstSamps;
            core.errTotal = burstSamps;
            core.errStart = core.lastGood;
            core.errEnd = x;
        }

        if (core.errRemain > 0)
        {
            const auto t = 1.0f - (float) core.errRemain / (float) core.errTotal;
            if (mode == 1)
                y = 0.0f;
            else if (mode == 2)
                y = core.errStart + (core.errEnd - core.errStart) * t;
            else if (mode == 3)
            {
                auto read = core.ri - repeatSamps;
                while (read < 0)
                    read += (int) core.ring.size();
                read %= (int) core.ring.size();
                y = core.ring[(size_t) read];
            }
            else
                y = core.lastGood;

            --core.errRemain;
        }
        else
        {
            core.lastGood = y;
        }

        if (core.clickRemain <= 0 && scratchAmt > 0.0001f && unif(rng) < clickP)
        {
            const auto len = clickLenMin + (int) std::floor(unif(rng) * (float) (clickLenMax - clickLenMin));
            core.clickTotal = len;
            core.clickRemain = len;
            const auto a0 = 0.08f + 0.55f * scratchAmt;
            core.clickAmp = a0 * (0.55f + 0.9f * unif(rng));
            core.clickSign = unif(rng) < 0.5f ? -1.0f : 1.0f;
        }

        if (core.clickRemain > 0)
        {
            const auto t = 1.0f - (float) core.clickRemain / (float) core.clickTotal;
            const auto env = std::pow(1.0f - t, 2.8f);
            const auto edge = t < 0.1f ? t / 0.1f : 1.0f;
            const auto c = core.clickSign * core.clickAmp * env * edge;
            y = clampf(y + c, -1.2f, 1.2f);
            --core.clickRemain;
        }

        if (servo > 0.0001f)
        {
            core.servoPhaseA += humHz / sr;
            core.servoPhaseB += whirrHz / sr;
            if (core.servoPhaseA >= 1.0f)
                core.servoPhaseA -= 1.0f;
            if (core.servoPhaseB >= 1.0f)
                core.servoPhaseB -= 1.0f;

            const auto hum = std::sin(core.servoPhaseA * juce::MathConstants<float>::twoPi) * servoDepth;
            const auto whirr = std::sin(core.servoPhaseB * juce::MathConstants<float>::twoPi) * servoDepth * 0.65f;
            const auto chatter = nextWhite() * chatterDepth;
            y += hum + whirr + chatter;
        }

        core.hfZ = (1.0f - a) * y + a * core.hfZ;
        y = core.hfZ;

        // Extra plugin-only car stereo compression blend, kept off when carComp = 0.
        if (carComp > 0.0001f)
            y = softClip(y * (1.0f + carComp * 2.1f)) * (0.96f + 0.04f * carComp);

        auto post = y * outGain;
        if (doSoftClip)
            post = softClip(post);

        const auto la = std::abs(post);
        const auto lc = la > core.limEnv ? limAtk : limRel;
        core.limEnv = la + lc * (core.limEnv - la);
        const auto limGain = core.limEnv > ceiling ? ceiling / (core.limEnv + 1e-6f) : 1.0f;
        post *= limGain;

        const auto outMono = clampf(post, -ceiling, ceiling);
        for (int ch = 0; ch < inCh && ch < 2; ++ch)
            writePtrs[(size_t) ch][i] = outMono;
    }
}

bool CDEngineAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* CDEngineAudioProcessor::createEditor()
{
    return new CDEngineAudioProcessorEditor(*this);
}

void CDEngineAudioProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, dest);
}

void CDEngineAudioProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CDEngineAudioProcessor();
}
