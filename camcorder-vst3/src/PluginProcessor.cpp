#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

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

CamcorderEngineAudioProcessor::CamcorderEngineAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout()),
      rng(0x43414d45)
{
}

juce::AudioProcessorValueTreeState::ParameterLayout CamcorderEngineAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto n01 = juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f);

    p.push_back(std::make_unique<juce::AudioParameterFloat>("coverage", "Coverage", n01, 0.35f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("movement", "Movement", n01, 0.25f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("corruption", "Corruption", n01, 0.18f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("agc", "AGC Drive", n01, 0.35f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("wind", "Wind", false));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("windLevel", "Wind Level", juce::NormalisableRange<float>(0.0f, 1.5f, 0.001f), 0.95f));

    p.push_back(std::make_unique<juce::AudioParameterFloat>("hpHz", "HP", juce::NormalisableRange<float>(10.0f, 280.0f, 1.0f), 55.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("lpHz", "LP", juce::NormalisableRange<float>(900.0f, 22000.0f, 1.0f), 9200.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("boxDb", "Box dB", juce::NormalisableRange<float>(0.0f, 14.0f, 0.1f), 3.2f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("boxHz", "Box Hz", juce::NormalisableRange<float>(650.0f, 4200.0f, 1.0f), 1650.0f));

    p.push_back(std::make_unique<juce::AudioParameterFloat>("agcAmt", "AGC Amt", n01, 0.55f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("agcSpeed", "AGC Speed", n01, 0.45f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("clip", "Clip", n01, 0.25f));

    p.push_back(std::make_unique<juce::AudioParameterFloat>("crush", "Crush", n01, 0.12f));
    p.push_back(std::make_unique<juce::AudioParameterInt>("bits", "Bits", 4, 16, 12));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("rate", "Rate", juce::NormalisableRange<float>(8000.0f, 48000.0f, 1.0f), 24000.0f));

    p.push_back(std::make_unique<juce::AudioParameterFloat>("drop", "Drop", n01, 0.18f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("dropMs", "Drop Ms", juce::NormalisableRange<float>(1.0f, 500.0f, 1.0f), 28.0f));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("dropMode", "Drop Mode", juce::StringArray { "Hold", "Mute", "Interp", "Repeat" }, 0));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("repeatMs", "Repeat Ms", juce::NormalisableRange<float>(1.0f, 600.0f, 1.0f), 48.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("chirp", "Chirp", n01, 0.15f));

    p.push_back(std::make_unique<juce::AudioParameterFloat>("handling", "Handling", n01, 0.22f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("rub", "Rub", n01, 0.18f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("hiss", "Hiss", n01, 0.12f));

    p.push_back(std::make_unique<juce::AudioParameterFloat>("ceiling", "Ceiling", juce::NormalisableRange<float>(0.2f, 1.0f, 0.001f), 0.92f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("outGain", "Out Gain", juce::NormalisableRange<float>(0.0f, 1.5f, 0.01f), 0.98f));

    return { p.begin(), p.end() };
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

float CamcorderEngineAudioProcessor::nextWhite()
{
    return unif(rng) * 2.0f - 1.0f;
}

void CamcorderEngineAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    st = {};
    st.ring.assign((size_t) juce::jmax(1024, (int) std::ceil(sampleRate * 0.35)), 0.0f);
    updateFilters(sampleRate);
}

void CamcorderEngineAudioProcessor::releaseResources() {}

bool CamcorderEngineAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in != out)
        return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void CamcorderEngineAudioProcessor::updateFilters(double sampleRate)
{
    const auto hpHz = apvts.getRawParameterValue("hpHz")->load();
    const auto lpHz = apvts.getRawParameterValue("lpHz")->load();
    const auto boxDb = apvts.getRawParameterValue("boxDb")->load();
    const auto boxHz = apvts.getRawParameterValue("boxHz")->load();

    auto hp = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, clampf(hpHz, 10.0f, 20000.0f), 0.707f);
    auto lp = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, clampf(lpHz, 20.0f, 20000.0f), 0.85f);
    auto box = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, clampf(boxHz, 20.0f, 20000.0f), 1.2f, juce::Decibels::decibelsToGain(boxDb));
    auto dip = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 650.0f, 0.9f, juce::Decibels::decibelsToGain(-0.35f * boxDb));

    tone.hp.coefficients = hp;
    tone.lp1.coefficients = lp;
    tone.lp2.coefficients = lp;
    tone.box.coefficients = box;
    tone.dip.coefficients = dip;
}

void CamcorderEngineAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi);
    juce::ScopedNoDenormals nd;

    const auto inCh = getTotalNumInputChannels();
    const auto outCh = getTotalNumOutputChannels();
    for (int ch = inCh; ch < outCh; ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());

    updateFilters(getSampleRate());

    const auto sr = (float) getSampleRate();
    const auto n = buffer.getNumSamples();

    const auto coverage = clampf(apvts.getRawParameterValue("coverage")->load(), 0.0f, 1.0f);
    const auto movement = clampf(apvts.getRawParameterValue("movement")->load(), 0.0f, 1.0f);
    const auto corruption = clampf(apvts.getRawParameterValue("corruption")->load(), 0.0f, 1.0f);
    const auto agcDrive = clampf(apvts.getRawParameterValue("agc")->load(), 0.0f, 1.0f);
    const auto windOn = apvts.getRawParameterValue("wind")->load() > 0.5f;
    const auto windLevel = clampf(apvts.getRawParameterValue("windLevel")->load(), 0.0f, 1.5f);

    const auto agcAmt = clampf(apvts.getRawParameterValue("agcAmt")->load(), 0.0f, 1.0f);
    const auto agcSpeed = clampf(apvts.getRawParameterValue("agcSpeed")->load(), 0.0f, 1.0f);
    const auto clip = clampf(apvts.getRawParameterValue("clip")->load(), 0.0f, 1.0f);
    const auto crush = clampf(apvts.getRawParameterValue("crush")->load(), 0.0f, 1.0f);
    const auto bits = juce::jlimit(4, 16, (int) std::lround(apvts.getRawParameterValue("bits")->load()));
    const auto rateParam = clampf(apvts.getRawParameterValue("rate")->load(), 8000.0f, 48000.0f);

    const auto drop = clampf(apvts.getRawParameterValue("drop")->load(), 0.0f, 1.0f);
    const auto dropMs = clampf(apvts.getRawParameterValue("dropMs")->load(), 1.0f, 600.0f);
    const auto dropMode = juce::jlimit(0, 3, (int) std::lround(apvts.getRawParameterValue("dropMode")->load()));
    const auto repeatMs = clampf(apvts.getRawParameterValue("repeatMs")->load(), 1.0f, 800.0f);
    const auto chirp = clampf(apvts.getRawParameterValue("chirp")->load(), 0.0f, 1.0f);

    const auto handling = clampf(apvts.getRawParameterValue("handling")->load(), 0.0f, 1.0f);
    const auto rub = clampf(apvts.getRawParameterValue("rub")->load(), 0.0f, 1.0f);
    const auto hiss = clampf(apvts.getRawParameterValue("hiss")->load(), 0.0f, 1.0f);

    const auto ceiling = clampf(apvts.getRawParameterValue("ceiling")->load(), 0.2f, 1.0f);
    const auto outGain = clampf(apvts.getRawParameterValue("outGain")->load(), 0.0f, 1.5f);

    const auto target = 0.18f;
    const auto atk = 0.002f + (1.0f - agcSpeed) * 0.02f;
    const auto rel = 0.05f + (1.0f - agcSpeed) * 0.28f;
    const auto envAtk = std::exp(-1.0f / (atk * sr));
    const auto envRel = std::exp(-1.0f / (rel * sr));
    const auto compPow = 1.0f + agcAmt * 2.0f;

    const auto baseCut = 14000.0f - coverage * 11500.0f;
    const auto loudClose = 0.35f + 0.55f * coverage;

    const auto qLevels = (float) juce::jmax(1, (1 << (bits - 1)) - 1);
    const auto basePeriod = juce::jmax(1, (int) std::lround(sr / juce::jmin(sr, rateParam)));

    const auto dropSamps = juce::jmax(8, (int) std::lround((dropMs / 1000.0f) * sr));
    const auto dropP = (0.0000009f + drop * drop * 0.00006f) * (1.0f + 3.2f * corruption);

    const auto repeatSamps = juce::jmax(1, (int) std::lround((repeatMs / 1000.0f) * sr));
    const auto chirpP = (0.0000012f + chirp * chirp * 0.00009f) * (0.6f + 1.2f * corruption);
    const auto thumpP = (0.000004f + movement * movement * 0.00035f) * (0.55f + handling);
    const auto thumpLenMin = juce::jmax(8, (int) std::lround((10.0f / 1000.0f) * sr));
    const auto thumpLenMax = juce::jmax(thumpLenMin + 1, (int) std::lround((90.0f / 1000.0f) * sr));

    const auto rubDepth = rub * rub * (0.055f + 0.095f * movement);
    const auto rubA90 = std::exp((-2.0f * juce::MathConstants<float>::pi * 90.0f) / sr);
    const auto rubA1800 = std::exp((-2.0f * juce::MathConstants<float>::pi * 1800.0f) / sr);

    const auto hissDepth = hiss * hiss * 0.03f;
    const auto drive = 1.0f + agcDrive * 14.0f;
    const auto asym = 0.05f * agcDrive;

    const auto limAtk = std::exp(-1.0f / (0.002f * sr));
    const auto limRel = std::exp(-1.0f / (0.06f * sr));

    const auto windP = windOn ? (0.0000011f + movement * movement * 0.00022f) * 0.9f : 0.0f;
    const auto windLenMin = juce::jmax(8, (int) std::lround((35.0f / 1000.0f) * sr));
    const auto windLenMax = juce::jmax(windLenMin + 1, (int) std::lround((180.0f / 1000.0f) * sr));

    const auto wiggleP = (0.0000012f + movement * movement * 0.00018f) * (0.35f + 0.9f * corruption);
    const auto wiggleLenMin = juce::jmax(8, (int) std::lround((8.0f / 1000.0f) * sr));
    const auto wiggleLenMax = juce::jmax(wiggleLenMin + 1, (int) std::lround((55.0f / 1000.0f) * sr));

    auto* l = buffer.getWritePointer(0);
    auto* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

    for (int i = 0; i < n; ++i)
    {
        const auto inL = l[i];
        const auto inR = r != nullptr ? r[i] : inL;
        const auto xIn = 0.5f * (inL + inR);

        st.ring[(size_t) st.ri] = xIn;
        st.ri = (st.ri + 1) % (int) st.ring.size();

        const auto a0 = std::abs(xIn);
        const auto c0 = a0 > st.env ? envAtk : envRel;
        st.env = a0 + c0 * (st.env - a0);
        const auto env = st.env + 1.0e-6f;

        const auto want = std::pow(target / env, compPow * 0.35f);
        const auto agc = clampf(want, 0.25f, 7.5f);
        auto y = xIn * agc;

        if (clip > 0.0001f)
        {
            const auto amt = 1.0f + clip * 6.5f;
            y = softClip((y + asym) * amt * drive) - asym * 0.7f;
        }
        else
            y *= drive;

        const auto loud = clampf(env * 3.2f, 0.0f, 1.0f);
        const auto cut = juce::jmax(350.0f, baseCut * (1.0f - loud * loudClose * 0.55f));
        const auto a = std::exp((-2.0f * juce::MathConstants<float>::pi * cut) / sr);
        st.mufZ = (1.0f - a) * y + a * st.mufZ;
        y = st.mufZ;

        y = tone.hp.processSample(y);
        y = tone.box.processSample(y);
        y = tone.dip.processSample(y);
        y = tone.lp1.processSample(y);
        y = tone.lp2.processSample(y);

        const auto wob = corruption * 0.35f;
        if (st.holdCount <= 0)
        {
            const auto j = wob > 0.0f ? nextWhite() * wob : 0.0f;
            st.holdPeriod = juce::jmax(1, (int) std::lround((float) basePeriod * (1.0f + j)));
            st.hold = y;
            st.holdCount = st.holdPeriod;
        }
        y = st.hold;
        --st.holdCount;

        if (crush > 0.0001f)
        {
            const auto q = std::round(clampf(y, -1.0f, 1.0f) * qLevels) / qLevels;
            y = y * (1.0f - crush) + q * crush;
        }

        if (st.dropRemain <= 0 && unif(rng) < dropP)
        {
            st.dropRemain = dropSamps;
            st.dropTotal = dropSamps;
            st.dropStart = st.lastGood;
            st.dropEnd = xIn;
        }

        if (st.dropRemain > 0)
        {
            const auto t = 1.0f - (float) st.dropRemain / (float) st.dropTotal;
            if (dropMode == 1)
                y = 0.0f;
            else if (dropMode == 2)
                y = st.dropStart + (st.dropEnd - st.dropStart) * t;
            else if (dropMode == 3)
            {
                const auto read = (st.ri - repeatSamps + (int) st.ring.size()) % (int) st.ring.size();
                y = st.ring[(size_t) read];
            }
            else
                y = st.lastGood;
            --st.dropRemain;
        }
        else
            st.lastGood = y;

        if (st.chirpRemain <= 0 && chirp > 0.0001f && unif(rng) < chirpP)
        {
            const auto durMs = 10.0f + unif(rng) * (35.0f + chirp * 55.0f);
            st.chirpTotal = juce::jmax(8, (int) std::lround((durMs / 1000.0f) * sr));
            st.chirpRemain = st.chirpTotal;
            st.chirpPhase = unif(rng);
            const auto base = 900.0f + unif(rng) * 2500.0f;
            st.chirpF0 = base;
            st.chirpF1 = base + 3000.0f + unif(rng) * 5000.0f;
            st.chirpAmp = (0.02f + 0.12f * chirp) * (0.65f + 0.7f * unif(rng));
        }
        if (st.chirpRemain > 0)
        {
            const auto t = 1.0f - (float) st.chirpRemain / (float) st.chirpTotal;
            const auto f = st.chirpF0 + (st.chirpF1 - st.chirpF0) * t;
            st.chirpPhase += f / sr;
            if (st.chirpPhase >= 1.0f)
                st.chirpPhase -= 1.0f;
            const auto envc = std::sin(juce::MathConstants<float>::pi * t) * (1.0f - t);
            const auto sig = std::sin(st.chirpPhase * juce::MathConstants<float>::twoPi) * st.chirpAmp * envc;
            y = clampf(y + sig, -1.2f, 1.2f);
            --st.chirpRemain;
        }

        if (st.thumpRemain <= 0 && handling > 0.0001f && unif(rng) < thumpP)
        {
            const auto len = thumpLenMin + (int) std::floor(unif(rng) * (float) (thumpLenMax - thumpLenMin));
            st.thumpTotal = len;
            st.thumpRemain = len;
            st.thumpPhase = 0.0f;
            st.thumpHz = 35.0f + unif(rng) * 75.0f;
            st.thumpAmp = (0.045f + 0.38f * handling) * (0.75f + 0.9f * unif(rng));
        }
        if (st.thumpRemain > 0)
        {
            const auto t = 1.0f - (float) st.thumpRemain / (float) st.thumpTotal;
            const auto envt = std::pow(1.0f - t, 2.05f) * std::sin(juce::jmin(1.0f, t / 0.1f) * juce::MathConstants<float>::halfPi);
            st.thumpPhase += st.thumpHz / sr;
            if (st.thumpPhase >= 1.0f)
                st.thumpPhase -= 1.0f;
            const auto sig = std::sin(st.thumpPhase * juce::MathConstants<float>::twoPi) * st.thumpAmp * envt;
            y += sig;
            --st.thumpRemain;
        }

        if (st.wiggleRemain <= 0 && movement > 0.0001f && unif(rng) < wiggleP)
        {
            const auto len = wiggleLenMin + (int) std::floor(unif(rng) * (float) (wiggleLenMax - wiggleLenMin));
            st.wiggleTotal = len;
            st.wiggleRemain = len;
            st.wigglePhase = unif(rng);
            st.wiggleHz = 140.0f + unif(rng) * 520.0f;
            st.wiggleAmp = (0.02f + 0.16f * movement) * (0.6f + 0.8f * unif(rng));
        }
        if (st.wiggleRemain > 0)
        {
            const auto t = 1.0f - (float) st.wiggleRemain / (float) st.wiggleTotal;
            const auto envw = std::pow(1.0f - t, 1.8f);
            st.wigglePhase += st.wiggleHz / sr;
            if (st.wigglePhase >= 1.0f)
                st.wigglePhase -= 1.0f;
            const auto p = st.wigglePhase < 0.5f ? 1.0f : -1.0f;
            const auto chunk = softClip(p * st.wiggleAmp * (1.0f + 2.2f * corruption));
            y += chunk * envw;
            --st.wiggleRemain;
        }

        if (windOn && st.windRemain <= 0 && unif(rng) < windP)
        {
            const auto len = windLenMin + (int) std::floor(unif(rng) * (float) (windLenMax - windLenMin));
            st.windTotal = len;
            st.windRemain = len;
            st.windPhase = unif(rng);
            st.windAmp = (0.09f + 0.7f * movement) * (0.75f + 0.85f * unif(rng));
        }
        if (st.windRemain > 0)
        {
            const auto t = 1.0f - (float) st.windRemain / (float) st.windTotal;
            const auto envwi = std::sin(juce::MathConstants<float>::pi * t) * (1.0f - 0.2f * t);
            const auto f = 35.0f + 55.0f * (1.0f - t);
            st.windPhase += f / sr;
            if (st.windPhase >= 1.0f)
                st.windPhase -= 1.0f;
            const auto woof = std::sin(st.windPhase * juce::MathConstants<float>::twoPi) * st.windAmp;
            const auto wide = nextWhite() * (st.windAmp * 0.55f);
            y += (woof + wide) * envwi * windLevel;
            --st.windRemain;
        }

        if (rubDepth > 0.00001f)
        {
            const auto wn = nextWhite();
            st.rubLp90 = (1.0f - rubA90) * wn + rubA90 * st.rubLp90;
            const auto hp = wn - st.rubLp90;
            st.rubLp1800 = (1.0f - rubA1800) * hp + rubA1800 * st.rubLp1800;
            y += st.rubLp1800 * rubDepth;
        }

        if (hissDepth > 0.00001f)
        {
            const auto wn = nextWhite();
            const auto hp = wn - st.hissZ;
            st.hissZ = wn;
            y += hp * hissDepth;
        }

        auto post = y * outGain;
        const auto aa = std::abs(post);
        const auto lc = aa > st.limEnv ? limAtk : limRel;
        st.limEnv = aa + lc * (st.limEnv - aa);
        const auto g = st.limEnv > ceiling ? ceiling / (st.limEnv + 1.0e-6f) : 1.0f;
        post *= g;
        post = clampf(post, -ceiling, ceiling);

        l[i] = post;
        if (r != nullptr)
            r[i] = post;
    }
}

bool CamcorderEngineAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* CamcorderEngineAudioProcessor::createEditor()
{
    return new CamcorderEngineAudioProcessorEditor(*this);
}

void CamcorderEngineAudioProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, dest);
}

void CamcorderEngineAudioProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CamcorderEngineAudioProcessor();
}
