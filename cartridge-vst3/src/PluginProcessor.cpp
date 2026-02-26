#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

CartridgeEngineAudioProcessor::CartridgeEngineAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout()),
      rng(0xfeedc0de)
{
}

juce::AudioProcessorValueTreeState::ParameterLayout CartridgeEngineAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto n01 = juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f);

    p.push_back(std::make_unique<juce::AudioParameterFloat>("quality", "Quality", n01, 0.55f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("codec", "Codec", n01, 0.25f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("grit", "Grit", n01, 0.25f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("noise", "Noise", n01, 0.10f));

    p.push_back(std::make_unique<juce::AudioParameterBool>("dither", "Dither", true));
    p.push_back(std::make_unique<juce::AudioParameterBool>("noiseShaping", "Noise Shaping", false));

    p.push_back(std::make_unique<juce::AudioParameterInt>("bits", "Bits", 2, 16, 10));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("rate", "Rate", juce::NormalisableRange<float>(6000.0f, 48000.0f, 1.0f), 24000.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("jitter", "Jitter", n01, 0.05f));

    p.push_back(std::make_unique<juce::AudioParameterFloat>("lpHz", "LP", juce::NormalisableRange<float>(2500.0f, 18000.0f, 1.0f), 9000.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("hpHz", "HP", juce::NormalisableRange<float>(20.0f, 240.0f, 1.0f), 70.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("preEmph", "Pre Emph", n01, 0.2f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("mulaw", "Mu-Law", n01, 0.25f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("blockMs", "Block", juce::NormalisableRange<float>(0.0f, 60.0f, 0.1f), 8.0f));

    p.push_back(std::make_unique<juce::AudioParameterFloat>("sat", "Sat", n01, 0.18f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("edge", "Edge", n01, 0.15f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("noiseTrack", "Noise Track", n01, 0.6f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("dcDrift", "DC Drift", n01, 0.15f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("speaker", "Speaker", n01, 0.45f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("hum", "Hum", n01, 0.08f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("whine", "Whine", n01, 0.10f));

    p.push_back(std::make_unique<juce::AudioParameterFloat>("bleepsMix", "Bleeps Mix", n01, 0.18f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("bleepsRate", "Bleeps Rate", juce::NormalisableRange<float>(0.0f, 18.0f, 0.01f), 3.0f));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("bleepsWave", "Bleeps Wave", juce::StringArray { "Random", "Pulse", "Saw", "Tri" }, 0));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("bleepsVibrato", "Bleeps Vibrato", n01, 0.35f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("bleepsPitch", "Bleeps Pitch", n01, 0.55f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("bleepsEnable", "Bleeps", false));

    p.push_back(std::make_unique<juce::AudioParameterFloat>("microDelayMs", "Micro Delay ms", juce::NormalisableRange<float>(0.0f, 30.0f, 0.1f), 8.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("microDelayMix", "Micro Delay Mix", n01, 0.18f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("verb", "Verb", n01, 0.16f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("verbMs", "Verb ms", juce::NormalisableRange<float>(10.0f, 120.0f, 0.1f), 45.0f));

    p.push_back(std::make_unique<juce::AudioParameterFloat>("wet", "Wet", n01, 0.90f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("ceiling", "Ceiling", juce::NormalisableRange<float>(0.2f, 1.0f, 0.001f), 0.92f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("limiter", "Limiter", n01, 0.55f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("outGain", "Out", juce::NormalisableRange<float>(0.0f, 1.5f, 0.01f), 0.90f));

    return { p.begin(), p.end() };
}

const juce::String CartridgeEngineAudioProcessor::getName() const { return JucePlugin_Name; }
bool CartridgeEngineAudioProcessor::acceptsMidi() const { return false; }
bool CartridgeEngineAudioProcessor::producesMidi() const { return false; }
bool CartridgeEngineAudioProcessor::isMidiEffect() const { return false; }
double CartridgeEngineAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int CartridgeEngineAudioProcessor::getNumPrograms() { return 1; }
int CartridgeEngineAudioProcessor::getCurrentProgram() { return 0; }
void CartridgeEngineAudioProcessor::setCurrentProgram(int) {}
const juce::String CartridgeEngineAudioProcessor::getProgramName(int) { return {}; }
void CartridgeEngineAudioProcessor::changeProgramName(int, const juce::String&) {}

float CartridgeEngineAudioProcessor::clampf(float x, float lo, float hi)
{
    return juce::jlimit(lo, hi, x);
}

float CartridgeEngineAudioProcessor::softClip(float x)
{
    return std::tanh(x);
}

float CartridgeEngineAudioProcessor::mulawEncode(float x, float mu)
{
    const auto s = std::copysign(1.0f, x);
    const auto ax = std::min(1.0f, std::abs(x));
    const auto y = std::log1p(mu * ax) / std::log1p(mu);
    return s * y;
}

float CartridgeEngineAudioProcessor::mulawDecode(float y, float mu)
{
    const auto s = std::copysign(1.0f, y);
    const auto ay = std::min(1.0f, std::abs(y));
    const auto x = std::expm1(ay * std::log1p(mu)) / mu;
    return s * x;
}

float CartridgeEngineAudioProcessor::nextWhite()
{
    return unif(rng);
}

float CartridgeEngineAudioProcessor::nextSigned()
{
    return nextWhite() * 2.0f - 1.0f;
}

void CartridgeEngineAudioProcessor::resetState(double sampleRate)
{
    st = {};
    st.delay.assign((size_t) juce::jmax(1, (int) std::floor(sampleRate * 0.05)), 0.0f);
    st.c1.assign((size_t) juce::jmax(1, (int) std::floor(sampleRate * 0.14)), 0.0f);
    st.c2.assign((size_t) juce::jmax(1, (int) std::floor(sampleRate * 0.14)), 0.0f);
    st.c3.assign((size_t) juce::jmax(1, (int) std::floor(sampleRate * 0.14)), 0.0f);
    st.ap1.assign((size_t) juce::jmax(1, (int) std::floor(sampleRate * 0.06)), 0.0f);
    st.ap2.assign((size_t) juce::jmax(1, (int) std::floor(sampleRate * 0.06)), 0.0f);
}

void CartridgeEngineAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    resetState(sampleRate);
    updateToneFilters(sampleRate);
}

void CartridgeEngineAudioProcessor::releaseResources() {}

bool CartridgeEngineAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in != out)
        return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void CartridgeEngineAudioProcessor::updateToneFilters(double sampleRate)
{
    const auto lpHz = clampf(apvts.getRawParameterValue("lpHz")->load(), 2500.0f, 18000.0f);
    const auto hpHz = clampf(apvts.getRawParameterValue("hpHz")->load(), 20.0f, 240.0f);
    const auto spk = clampf(apvts.getRawParameterValue("speaker")->load(), 0.0f, 1.0f);

    auto lpPre = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, lpHz, 0.707f);
    auto hp = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, hpHz, 0.707f);
    auto dip = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 650.0f, 0.9f, juce::Decibels::decibelsToGain(-spk * 4.5f));
    auto hump = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 1900.0f, 1.2f, juce::Decibels::decibelsToGain(spk * 6.5f));
    auto spLpHz = 12000.0f - spk * 9000.0f;
    auto spLp = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, spLpHz, 0.85f);

    tone.lpPre.coefficients = lpPre;
    tone.hpPost.coefficients = hp;
    tone.dip.coefficients = dip;
    tone.hump.coefficients = hump;
    tone.spLp1.coefficients = spLp;
    tone.spLp2.coefficients = spLp;
}

float CartridgeEngineAudioProcessor::bleepEnv(float t) const
{
    const auto a = std::min(1.0f, t / 0.12f);
    const auto b = std::min(1.0f, (1.0f - t) / 0.2f);
    return std::sin(a * juce::MathConstants<float>::halfPi) * std::sin(b * juce::MathConstants<float>::halfPi);
}

float CartridgeEngineAudioProcessor::bleepOsc(float phase, int wave, float duty) const
{
    const auto p = phase - std::floor(phase);
    if (wave == 2)
        return 2.0f * (p - 0.5f);
    if (wave == 3)
        return 1.0f - 4.0f * std::abs(p - 0.5f);
    return p < duty ? 1.0f : -1.0f;
}

void CartridgeEngineAudioProcessor::triggerBleep(float pitch, float vibrato, int waveSel, double sr)
{
    auto& b = st.bleep;
    const auto baseLo = 220.0f + pitch * 320.0f;
    const auto baseHi = 700.0f + pitch * 1800.0f;
    const auto f = baseLo + nextWhite() * (baseHi - baseLo);
    const auto durMs = 35.0f + nextWhite() * (55.0f + pitch * 120.0f);

    b.total = juce::jmax(8, (int) std::floor((durMs / 1000.0f) * (float) sr));
    b.remain = b.total;
    b.active = true;
    b.phase = nextWhite();
    b.vibPhase = nextWhite();
    b.freq = f;

    const auto vibChance = std::min(0.85f, 0.15f + vibrato * 0.75f);
    const auto doVib = nextWhite() < vibChance;
    b.vibRate = 4.0f + nextWhite() * 7.0f;
    b.vibDepth = doVib ? (0.003f + 0.02f * vibrato * vibrato) * (0.6f + 0.6f * nextWhite()) : 0.0f;
    b.amp = (0.12f + 0.32f * nextWhite()) * (0.55f + 0.6f * pitch);

    if (waveSel == 0)
    {
        const auto r = nextWhite();
        b.wave = r < 0.45f ? 1 : (r < 0.75f ? 3 : 2);
    }
    else
        b.wave = waveSel;

    b.duty = 0.25f + nextWhite() * 0.55f;
}

float CartridgeEngineAudioProcessor::processBleep(double sr, bool enable, float mix, float rate, int waveSel, float vibrato, float pitch)
{
    if (!enable || mix <= 0.0001f || rate <= 0.0001f)
        return 0.0f;

    auto& b = st.bleep;
    const auto pPerSample = (0.15f + rate) / (float) sr;

    if (! b.active && nextWhite() < pPerSample)
        triggerBleep(pitch, vibrato, waveSel, sr);

    float y = 0.0f;
    if (b.active && b.remain > 0)
    {
        const auto t = 1.0f - (float) b.remain / (float) b.total;
        const auto env = bleepEnv(t);
        const auto vib = b.vibDepth > 0.0f ? std::sin(b.vibPhase * juce::MathConstants<float>::twoPi) * b.vibDepth : 0.0f;
        const auto f = b.freq * (1.0f + vib);
        b.phase += f / (float) sr;
        b.vibPhase += b.vibRate / (float) sr;
        const auto osc = bleepOsc(b.phase, b.wave, b.duty);
        const auto edge = (b.wave == 1 ? 0.85f : 0.65f) + vibrato * 0.15f;
        y = softClip(osc * (b.amp * 2.6f) * env * edge);
        --b.remain;
        if (b.remain <= 0)
            b.active = false;
    }

    return y * mix;
}

float CartridgeEngineAudioProcessor::combProcess(std::vector<float>& buf, int& idx, float input, int delaySamps, float fb, float damp, float& lpState)
{
    const auto len = (int) buf.size();
    const auto read = (idx - delaySamps + len) % len;
    const auto y = buf[(size_t) read];
    const auto lp = y + damp * (lpState - y);
    buf[(size_t) idx] = input + lp * fb;
    lpState = lp;
    idx = (idx + 1) % len;
    return y;
}

float CartridgeEngineAudioProcessor::allpassProcess(std::vector<float>& buf, int& idx, float input, int delaySamps, float g)
{
    const auto len = (int) buf.size();
    const auto read = (idx - delaySamps + len) % len;
    const auto b = buf[(size_t) read];
    const auto y = -g * input + b;
    buf[(size_t) idx] = input + y * g;
    idx = (idx + 1) % len;
    return y;
}

void CartridgeEngineAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi);
    juce::ScopedNoDenormals nd;

    const auto inCh = getTotalNumInputChannels();
    const auto outCh = getTotalNumOutputChannels();
    for (int ch = inCh; ch < outCh; ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());

    const auto sr = getSampleRate();
    if (st.delay.empty())
        resetState(sr);

    updateToneFilters(sr);

    const auto bits = juce::jlimit(2, 16, (int) std::lround(apvts.getRawParameterValue("bits")->load()));
    const auto targetRate = clampf(apvts.getRawParameterValue("rate")->load(), 6000.0f, (float) sr);
    const auto jitter = clampf(apvts.getRawParameterValue("jitter")->load(), 0.0f, 1.0f);
    const auto dither = apvts.getRawParameterValue("dither")->load() > 0.5f;
    const auto noiseShaping = apvts.getRawParameterValue("noiseShaping")->load() > 0.5f;

    const auto preEmph = clampf(apvts.getRawParameterValue("preEmph")->load(), 0.0f, 1.0f);
    const auto mulaw = clampf(apvts.getRawParameterValue("mulaw")->load(), 0.0f, 1.0f);
    const auto blockMs = juce::jmax(0.0f, apvts.getRawParameterValue("blockMs")->load());

    const auto sat = clampf(apvts.getRawParameterValue("sat")->load(), 0.0f, 1.0f);
    const auto edge = clampf(apvts.getRawParameterValue("edge")->load(), 0.0f, 1.0f);
    const auto dcDrift = clampf(apvts.getRawParameterValue("dcDrift")->load(), 0.0f, 1.0f);
    const auto hum = clampf(apvts.getRawParameterValue("hum")->load(), 0.0f, 1.0f);
    const auto whine = clampf(apvts.getRawParameterValue("whine")->load(), 0.0f, 1.0f);
    const auto noise = clampf(apvts.getRawParameterValue("noise")->load(), 0.0f, 1.0f);
    const auto noiseTrack = clampf(apvts.getRawParameterValue("noiseTrack")->load(), 0.0f, 1.0f);

    const auto bEnable = apvts.getRawParameterValue("bleepsEnable")->load() > 0.5f;
    const auto bMix = clampf(apvts.getRawParameterValue("bleepsMix")->load(), 0.0f, 1.0f);
    const auto bRate = juce::jmax(0.0f, apvts.getRawParameterValue("bleepsRate")->load());
    const auto bWave = juce::jlimit(0, 3, (int) std::lround(apvts.getRawParameterValue("bleepsWave")->load()));
    const auto bVib = clampf(apvts.getRawParameterValue("bleepsVibrato")->load(), 0.0f, 1.0f);
    const auto bPitch = clampf(apvts.getRawParameterValue("bleepsPitch")->load(), 0.0f, 1.0f);

    const auto microDelayMs = juce::jmax(0.0f, apvts.getRawParameterValue("microDelayMs")->load());
    const auto microDelayMix = clampf(apvts.getRawParameterValue("microDelayMix")->load(), 0.0f, 1.0f);
    const auto verb = clampf(apvts.getRawParameterValue("verb")->load(), 0.0f, 1.0f);
    const auto verbMs = clampf(apvts.getRawParameterValue("verbMs")->load(), 10.0f, 120.0f);

    const auto limiter = clampf(apvts.getRawParameterValue("limiter")->load(), 0.0f, 1.0f);
    const auto ceiling = clampf(apvts.getRawParameterValue("ceiling")->load(), 0.2f, 1.0f);
    const auto wet = clampf(apvts.getRawParameterValue("wet")->load(), 0.0f, 1.0f);
    const auto outGain = clampf(apvts.getRawParameterValue("outGain")->load(), 0.0f, 1.5f);

    const auto basePeriod = juce::jmax(1, (int) std::lround((float) sr / targetRate));
    const auto qLevels = std::pow(2.0f, (float) juce::jmax(1, bits - 1));
    const auto qStep = 1.0f / qLevels;
    const auto blockSamples = blockMs <= 0.0f ? 0 : juce::jmax(1, (int) std::floor((blockMs / 1000.0f) * (float) sr));

    const auto satAmt = 1.0f + sat * 6.0f;
    const auto preEdge = 1.0f + edge * 10.0f;
    const auto edgeAsym = edge * 0.12f;
    const auto humHz = nextWhite() < 0.5f ? 50.0f : 60.0f;
    const auto whineHz = 700.0f + 500.0f * whine;
    const auto humDepth = hum * 0.04f;
    const auto whineDepth = whine * 0.03f;
    const auto hissBase = noise * 0.02f;

    const auto envAtk = std::exp(-1.0f / ((float) sr * 0.004f));
    const auto envRel = std::exp(-1.0f / ((float) sr * 0.08f));
    const auto limAtk = std::exp(-1.0f / ((float) sr * 0.0010f));
    const auto limRel = std::exp(-1.0f / ((float) sr * 0.08f));
    const auto dcStep = 0.000002f + dcDrift * dcDrift * 0.00003f;

    const auto dSamps = juce::jlimit(0, (int) st.delay.size() - 1, (int) std::floor((microDelayMs / 1000.0f) * (float) sr));
    const auto combBase = (int) std::floor((verbMs / 1000.0f) * (float) sr);
    const auto d1 = juce::jlimit(32, (int) st.c1.size() - 1, (int) std::floor(combBase * 0.55f));
    const auto d2 = juce::jlimit(32, (int) st.c2.size() - 1, (int) std::floor(combBase * 0.78f));
    const auto d3 = juce::jlimit(32, (int) st.c3.size() - 1, (int) std::floor(combBase * 1.05f));
    const auto fb = 0.12f + verb * 0.72f;
    const auto damp = 0.22f + verb * 0.55f;
    const auto apD1 = juce::jlimit(16, (int) st.ap1.size() - 1, (int) std::floor(combBase * 0.33f));
    const auto apD2 = juce::jlimit(16, (int) st.ap2.size() - 1, (int) std::floor(combBase * 0.27f));
    const auto apG = 0.55f;

    auto* l = buffer.getWritePointer(0);
    auto* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const auto inL = l[i];
        const auto inR = r != nullptr ? r[i] : inL;
        const auto x = 0.5f * (inL + inR);

        auto y = tone.lpPre.processSample(x + processBleep(sr, bEnable, bMix, bRate, bWave, bVib, bPitch));
        const auto dry0 = y;

        const auto diff = y - st.preEmphZ;
        st.preEmphZ = y;
        y += diff * (0.6f * preEmph);

        st.dc = clampf(st.dc + nextSigned() * dcStep, -0.08f, 0.08f);
        y += st.dc;

        if (blockSamples > 0)
        {
            if (st.blockRemain <= 0)
            {
                st.blockTotal = blockSamples;
                st.blockRemain = st.blockTotal;
                st.blockHold = y;
            }
            const auto t = 1.0f - (float) st.blockRemain / (float) st.blockTotal;
            y = st.blockHold + (y - st.blockHold) * (0.25f + 0.75f * t);
            --st.blockRemain;
        }

        y = softClip((y + edgeAsym) * preEdge) - edgeAsym * 0.6f;

        if (st.holdCount <= 0)
        {
            st.hold = y;
            const auto j = nextSigned() * 0.45f * jitter;
            st.holdPeriod = juce::jmax(1, (int) std::lround((float) basePeriod * (1.0f + j)));
            st.holdCount = st.holdPeriod;
        }
        y = st.hold;
        --st.holdCount;

        if (mulaw > 0.0001f)
        {
            const auto enc = mulawEncode(y);
            const auto dec = mulawDecode(enc);
            y = y * (1.0f - mulaw) + dec * mulaw;
        }

        if (noiseShaping)
            y += st.nsErr * 0.65f;
        if (dither)
            y += nextSigned() * (qStep * 0.35f);
        const auto q = std::round(y * qLevels) / qLevels;
        if (noiseShaping)
            st.nsErr = y - q;
        y = q;

        const auto a = std::abs(y);
        const auto c = a > st.env ? envAtk : envRel;
        st.env = a + c * (st.env - a);
        const auto inv = 1.0f - clampf(st.env * 3.2f, 0.0f, 1.0f);
        const auto hiss = hissBase * (1.0f - noiseTrack + noiseTrack * (0.35f + 0.65f * inv));

        st.humPhase += juce::MathConstants<float>::twoPi * humHz / (float) sr;
        if (st.humPhase > juce::MathConstants<float>::twoPi)
            st.humPhase -= juce::MathConstants<float>::twoPi;
        st.whinePhase += juce::MathConstants<float>::twoPi * whineHz / (float) sr;
        if (st.whinePhase > juce::MathConstants<float>::twoPi)
            st.whinePhase -= juce::MathConstants<float>::twoPi;

        y += std::sin(st.humPhase) * humDepth;
        y += std::sin(st.whinePhase) * whineDepth;
        y += nextSigned() * hiss;

        y = softClip(y * satAmt) / juce::jmax(0.0001f, softClip(satAmt));

        if (dSamps > 0 && microDelayMix > 0.0001f)
        {
            const auto len = (int) st.delay.size();
            const auto read = (st.delayIndex - dSamps + len) % len;
            const auto d = st.delay[(size_t) read];
            st.delay[(size_t) st.delayIndex] = y;
            st.delayIndex = (st.delayIndex + 1) % len;
            y = y * (1.0f - microDelayMix) + d * microDelayMix;
        }
        else
        {
            st.delay[(size_t) st.delayIndex] = y;
            st.delayIndex = (st.delayIndex + 1) % (int) st.delay.size();
        }

        if (verb > 0.0001f)
        {
            const auto c1 = combProcess(st.c1, st.ci1, y, d1, fb, damp, st.c1lp);
            const auto c2 = combProcess(st.c2, st.ci2, y, d2, fb, damp, st.c2lp);
            const auto c3 = combProcess(st.c3, st.ci3, y, d3, fb, damp, st.c3lp);
            auto rv = (c1 + c2 + c3) * 0.33f;
            rv = allpassProcess(st.ap1, st.api1, rv, apD1, apG);
            rv = allpassProcess(st.ap2, st.api2, rv, apD2, apG);
            y = y * (1.0f - verb * 0.65f) + rv * (verb * 0.65f);
        }

        y = tone.hpPost.processSample(y);
        y = tone.dip.processSample(y);
        y = tone.hump.processSample(y);
        y = tone.spLp1.processSample(y);
        y = tone.spLp2.processSample(y);

        auto post = y * outGain;
        const auto aa = std::abs(post);
        const auto lc = aa > st.limEnv ? limAtk : limRel;
        st.limEnv = aa + lc * (st.limEnv - aa);
        const auto want = st.limEnv > ceiling ? ceiling / (st.limEnv + 1.0e-6f) : 1.0f;
        const auto g = 1.0f - limiter + limiter * want;
        const auto limited = clampf(post * g, -ceiling, ceiling);

        const auto wetOut = limited;
        const auto out = clampf(dry0 * (1.0f - wet) + wetOut * wet, -1.0f, 1.0f);

        l[i] = out;
        if (r != nullptr)
            r[i] = out;
    }
}

bool CartridgeEngineAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* CartridgeEngineAudioProcessor::createEditor()
{
    return new CartridgeEngineAudioProcessorEditor(*this);
}

void CartridgeEngineAudioProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, dest);
}

void CartridgeEngineAudioProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CartridgeEngineAudioProcessor();
}
