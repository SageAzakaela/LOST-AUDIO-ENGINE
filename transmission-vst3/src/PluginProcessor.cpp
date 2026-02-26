#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <BinaryData.h>

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
        auto i0 = (int) pos;
        auto i1 = juce::jmin(i0 + 1, (int) in.size() - 1);
        auto frac = (float) (pos - (double) i0);
        out[(size_t) i] = in[(size_t) i0] + (in[(size_t) i1] - in[(size_t) i0]) * frac;
        pos += step;
        if (pos > (double) in.size() - 1.0)
            pos = (double) in.size() - 1.0;
    }
    return out;
}

std::vector<float> decodeMp3ToMono(const void* data, size_t bytes, double targetSampleRate)
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
}

TransmissionEngineAudioProcessor::TransmissionEngineAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout()),
      rng(0xdecafbad)
{
}

juce::AudioProcessorValueTreeState::ParameterLayout TransmissionEngineAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto norm = juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f);
    params.push_back(std::make_unique<juce::AudioParameterFloat>("bandwidth", "Bandwidth", norm, 0.45f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("drive", "Drive", norm, 0.35f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("badConnection", "Bad Connection", norm, 0.25f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("noiseProfile", "Noise Profile", norm, 0.20f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("hpHz", "High-pass", juce::NormalisableRange<float>(40.0f, 1200.0f, 1.0f), 380.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("lpHz", "Low-pass", juce::NormalisableRange<float>(1200.0f, 12000.0f, 1.0f), 5200.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("midGainDb", "Mid Gain", juce::NormalisableRange<float>(-6.0f, 8.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("midFreq", "Mid Freq", juce::NormalisableRange<float>(600.0f, 3500.0f, 1.0f), 1550.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("midQ", "Mid Q", juce::NormalisableRange<float>(0.4f, 5.0f, 0.01f), 1.2f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("boxDipDb", "Box Dip", juce::NormalisableRange<float>(0.0f, 6.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("comp", "Compression", norm, 0.25f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("asym", "Asymmetry", norm, 0.10f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("crush", "Crush", norm, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("wowDepth", "Wow", norm, 0.25f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("dropRate", "Drop Rate", norm, 0.25f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("dropDepth", "Drop Depth", norm, 0.35f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("crackle", "Crackle", norm, 0.25f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("lfoRate", "LFO Rate", juce::NormalisableRange<float>(0.1f, 6.0f, 0.01f), 0.7f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("noiseColor", "Noise Color", norm, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("hiss", "Hiss", norm, 0.2f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("outGain", "Output Gain", juce::NormalisableRange<float>(0.0f, 1.5f, 0.01f), 0.92f));
    params.push_back(std::make_unique<juce::AudioParameterInt>("passes", "Passes", 1, 6, 1));
    params.push_back(std::make_unique<juce::AudioParameterBool>("tuningEnable", "Tuning Enable", false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("tuningMode", "Tuning Mode", juce::StringArray { "Edges", "Search" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("tuningSource", "Tuning Source", juce::StringArray { "Synth", "Dispatch", "Tuning 1", "Tuning 2", "Tuning 3", "Tuning 4", "Tuning 5", "Random Tuning" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("tuningAmount", "Tuning Amount", norm, 0.35f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("tuningSnippetMs", "Tuning Snippet", juce::NormalisableRange<float>(40.0f, 500.0f, 1.0f), 140.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("tuningCutDepth", "Tuning Cut", norm, 0.55f));

    return { params.begin(), params.end() };
}

const juce::String TransmissionEngineAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool TransmissionEngineAudioProcessor::acceptsMidi() const
{
    return false;
}

bool TransmissionEngineAudioProcessor::producesMidi() const
{
    return false;
}

bool TransmissionEngineAudioProcessor::isMidiEffect() const
{
    return false;
}

double TransmissionEngineAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int TransmissionEngineAudioProcessor::getNumPrograms()
{
    return 1;
}

int TransmissionEngineAudioProcessor::getCurrentProgram()
{
    return 0;
}

void TransmissionEngineAudioProcessor::setCurrentProgram(int)
{
}

const juce::String TransmissionEngineAudioProcessor::getProgramName(int)
{
    return {};
}

void TransmissionEngineAudioProcessor::changeProgramName(int, const juce::String&)
{
}

void TransmissionEngineAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);

    for (auto& passStates : channelStates)
        for (auto& ch : passStates)
            ch = {};

    for (auto& passChains : filters)
    {
        for (auto& chain : passChains)
        {
            chain.hp1.reset();
            chain.hp2.reset();
            chain.lp1.reset();
            chain.lp2.reset();
            chain.dip.reset();
            chain.mid.reset();
        }
    }

    lfoPhase = 0.0f;
    dropoutRemaining = 0;
    dropoutTotal = 0;
    dropoutDepth = 1.0f;
    crackleRemaining = 0;
    crackleTotal = 0;
    tuningRemaining = 0;
    tuningTotal = 0;
    tuningF0 = 1200.0f;
    tuningF1 = 3200.0f;
    tuningPhase = 0.0f;
    tuningPlayPos = 0.0f;
    tuningPlayStep = 1.0f;
    tuningSampleIndex = 0;
    tuningCooldownRemaining = 0;
    transportWasPlaying = false;
    initEmbeddedTuningSamples(sampleRate);

    updateFilters(sampleRate);
}

void TransmissionEngineAudioProcessor::releaseResources()
{
}

bool TransmissionEngineAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (in != out)
        return false;

    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void TransmissionEngineAudioProcessor::updateFilters(double sampleRate)
{
    const auto hpHz = apvts.getRawParameterValue("hpHz")->load();
    const auto lpHz = apvts.getRawParameterValue("lpHz")->load();
    const auto midGainDb = apvts.getRawParameterValue("midGainDb")->load();
    const auto midFreq = apvts.getRawParameterValue("midFreq")->load();
    const auto midQ = apvts.getRawParameterValue("midQ")->load();
    const auto boxDipDb = apvts.getRawParameterValue("boxDipDb")->load();
    const auto bandwidth = apvts.getRawParameterValue("bandwidth")->load();
    const auto q = 0.9f + (1.0f - bandwidth) * 0.35f;

    auto hp = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, juce::jlimit(40.0f, 20000.0f, hpHz), q);
    auto lp = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, juce::jlimit(20.0f, 20000.0f, lpHz), q);
    auto dip = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 680.0f, 0.8f, juce::Decibels::decibelsToGain(-std::abs(boxDipDb)));
    auto mid = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, juce::jlimit(200.0f, 10000.0f, midFreq), juce::jmax(0.2f, midQ), juce::Decibels::decibelsToGain(midGainDb));

    for (auto& passChains : filters)
    {
        for (auto& chain : passChains)
        {
            chain.hp1.coefficients = hp;
            chain.hp2.coefficients = hp;
            chain.lp1.coefficients = lp;
            chain.lp2.coefficients = lp;
            chain.dip.coefficients = dip;
            chain.mid.coefficients = mid;
        }
    }
}

float TransmissionEngineAudioProcessor::nextWhite()
{
    return unif(rng) * 2.0f - 1.0f;
}

float TransmissionEngineAudioProcessor::pinkFromWhite(PinkNoiseState& s, float white) const
{
    s.p0 = 0.99886f * s.p0 + white * 0.0555179f;
    s.p1 = 0.99332f * s.p1 + white * 0.0750759f;
    s.p2 = 0.96900f * s.p2 + white * 0.1538520f;
    s.p3 = 0.86650f * s.p3 + white * 0.3104856f;
    s.p4 = 0.55000f * s.p4 + white * 0.5329522f;
    s.p5 = -0.7616f * s.p5 - white * 0.0168980f;
    const auto pink = s.p0 + s.p1 + s.p2 + s.p3 + s.p4 + s.p5 + s.p6 + white * 0.5362f;
    s.p6 = white * 0.115926f;
    return pink * 0.11f;
}

void TransmissionEngineAudioProcessor::maybeTriggerDropout(float sampleRate, float dropRate, float dropDepth)
{
    if (dropoutRemaining > 0)
        return;

    const auto ratePerSec = 0.15f + 1.9f * dropRate * dropRate;
    const auto p = ratePerSec / sampleRate;
    if (unif(rng) < p)
    {
        const auto ms = 18.0f + 140.0f * dropRate;
        dropoutTotal = juce::jmax(1, int((ms / 1000.0f) * sampleRate));
        dropoutRemaining = dropoutTotal;
        const auto minGain = 1.0f - clampf(dropDepth, 0.0f, 1.0f) * 0.95f;
        dropoutDepth = clampf(minGain * (0.78f + 0.22f * unif(rng)), 0.02f, 1.0f);
    }
}

void TransmissionEngineAudioProcessor::maybeTriggerCrackle(float sampleRate, float crackleAmount)
{
    if (crackleRemaining > 0)
        return;

    const auto ratePerSec = 0.35f + 7.5f * crackleAmount * crackleAmount;
    const auto p = ratePerSec / sampleRate;
    if (unif(rng) < p)
    {
        const auto ms = 2.0f + 10.0f * crackleAmount;
        crackleTotal = juce::jmax(1, int((ms / 1000.0f) * sampleRate));
        crackleRemaining = crackleTotal;
    }
}

void TransmissionEngineAudioProcessor::initEmbeddedTuningSamples(double sampleRate)
{
    embeddedTuningSamples.clear();
    embeddedTuningSamples.reserve(6);
    embeddedTuningSamples.push_back(decodeMp3ToMono(BinaryData::dispatch_mp3, (size_t) BinaryData::dispatch_mp3Size, sampleRate));
    embeddedTuningSamples.push_back(decodeMp3ToMono(BinaryData::tuning1_mp3, (size_t) BinaryData::tuning1_mp3Size, sampleRate));
    embeddedTuningSamples.push_back(decodeMp3ToMono(BinaryData::tuning2_mp3, (size_t) BinaryData::tuning2_mp3Size, sampleRate));
    embeddedTuningSamples.push_back(decodeMp3ToMono(BinaryData::tuning3_mp3, (size_t) BinaryData::tuning3_mp3Size, sampleRate));
    embeddedTuningSamples.push_back(decodeMp3ToMono(BinaryData::tuning4_mp3, (size_t) BinaryData::tuning4_mp3Size, sampleRate));
    embeddedTuningSamples.push_back(decodeMp3ToMono(BinaryData::tuning5_mp3, (size_t) BinaryData::tuning5_mp3Size, sampleRate));
}

void TransmissionEngineAudioProcessor::triggerTuningEvent(float sampleRate, float snippetMs, int sourceMode)
{
    tuningTotal = juce::jmax(8, int((snippetMs / 1000.0f) * sampleRate));
    tuningRemaining = tuningTotal;
    tuningF0 = 250.0f + unif(rng) * 3500.0f;
    tuningF1 = 600.0f + unif(rng) * 7000.0f;
    if (unif(rng) < 0.5f)
        std::swap(tuningF0, tuningF1);
    tuningPhase = juce::MathConstants<float>::twoPi * unif(rng);
    if (sourceMode == 7 && embeddedTuningSamples.size() >= 6)
        tuningSampleIndex = 1 + (int) std::floor(unif(rng) * 5.0f); // tuning1..5
    else if (sourceMode >= 1 && sourceMode <= 6)
        tuningSampleIndex = sourceMode - 1; // dispatch,tuning1..5
    else
        tuningSampleIndex = 0;

    const auto* selected = tuningSampleIndex >= 0 && tuningSampleIndex < (int) embeddedTuningSamples.size() ? &embeddedTuningSamples[(size_t) tuningSampleIndex] : nullptr;
    if (selected != nullptr && !selected->empty())
        tuningPlayPos = unif(rng) * (float) selected->size();
    else
        tuningPlayPos = 0.0f;
    tuningPlayStep = 0.92f + 0.16f * unif(rng);
}

float TransmissionEngineAudioProcessor::sampleEmbeddedTuning(int sampleIndex, float position) const
{
    if (sampleIndex < 0 || sampleIndex >= (int) embeddedTuningSamples.size())
        return 0.0f;
    const auto& s = embeddedTuningSamples[(size_t) sampleIndex];
    if (s.size() < 4)
        return 0.0f;
    auto p = std::fmod(position, (float) s.size());
    if (p < 0.0f)
        p += (float) s.size();
    const auto i0 = (int) p;
    const auto i1 = (i0 + 1) % (int) s.size();
    const auto frac = p - (float) i0;
    return s[(size_t) i0] + (s[(size_t) i1] - s[(size_t) i0]) * frac;
}

float TransmissionEngineAudioProcessor::nextTuningSample(double sampleRate, const RuntimeParams& params, bool isPlaying, float& duckOut)
{
    duckOut = 1.0f;
    if (!params.tuningEnable)
    {
        tuningRemaining = 0;
        tuningCooldownRemaining = 0;
        return 0.0f;
    }

    if (tuningRemaining <= 0 && params.tuningMode == 1 && isPlaying && tuningCooldownRemaining <= 0)
    {
        const auto ratePerSec = 0.02f + 1.4f * params.tuningAmount * params.tuningAmount;
        const auto p = ratePerSec / (float) sampleRate;
        if (unif(rng) < p)
            triggerTuningEvent((float) sampleRate, params.tuningSnippetMs, params.tuningSource);
    }

    if (tuningRemaining <= 0)
    {
        if (tuningCooldownRemaining > 0)
            --tuningCooldownRemaining;
        return 0.0f;
    }

    const auto t = 1.0f - (float) tuningRemaining / (float) tuningTotal;
    const auto env = std::sin(juce::jlimit(0.0f, 1.0f, t / 0.08f) * juce::MathConstants<float>::halfPi)
                   * std::sin(juce::jlimit(0.0f, 1.0f, (1.0f - t) / 0.12f) * juce::MathConstants<float>::halfPi);
    float tune = 0.0f;
    if (params.tuningSource == 0)
    {
        const auto hz = tuningF0 + (tuningF1 - tuningF0) * t;
        tuningPhase += juce::MathConstants<float>::twoPi * hz / (float) sampleRate;
        const auto osc = std::sin(tuningPhase);
        tune = softClip((osc * 0.92f + nextWhite() * 0.35f) * (0.6f + 1.6f * params.tuningAmount)) * env;
    }
    else
    {
        const auto dry = sampleEmbeddedTuning(tuningSampleIndex, tuningPlayPos);
        tune = softClip((dry + nextWhite() * 0.015f) * (0.9f + 1.8f * params.tuningAmount)) * env;
        tuningPlayPos += tuningPlayStep * (tuningSampleIndex == 0 ? 1.1f : 1.0f);
    }

    --tuningRemaining;
    tuningCooldownRemaining = juce::jmax(tuningCooldownRemaining, juce::jmax(1, int(0.04 * sampleRate)));
    if (tuningRemaining <= 0)
        tuningCooldownRemaining = juce::jmax(tuningCooldownRemaining, juce::jmax(1, int(0.22 * sampleRate)));
    if (params.tuningMode == 1)
    {
        duckOut = 1.0f - params.tuningCutDepth * env;
        return tune * (0.75f + 0.65f * params.tuningAmount);
    }

    return tune * (0.55f + 0.75f * params.tuningAmount);
}

float TransmissionEngineAudioProcessor::processChannelSample(int passIndex, int channel, float x, double sampleRate, const RuntimeParams& params, bool includeEvents)
{
    auto& st = channelStates[(size_t) passIndex][(size_t) channel];
    auto& chain = filters[(size_t) passIndex][(size_t) channel];

    const auto preDrive = 1.0f + params.drive * 10.5f;
    const auto preBias = params.asym * 0.18f;
    auto y = softClip((x + preBias) * preDrive) - preBias * 0.45f;

    y = chain.hp1.processSample(y);
    y = chain.hp2.processSample(y);
    y = chain.lp1.processSample(y);
    y = chain.lp2.processSample(y);
    y = chain.dip.processSample(y);
    y = chain.mid.processSample(y);

    const auto postPre = 1.0f + params.drive * 12.0f;
    const auto postBias = params.asym * 0.22f;
    const auto compAmt = params.comp * 0.75f;
    const auto thr = 0.18f + (1.0f - params.drive) * 0.16f;
    const auto attack = std::exp(-1.0f / (float(sampleRate) * (0.003f + 0.01f * (1.0f - params.drive))));
    const auto release = std::exp(-1.0f / (float(sampleRate) * (0.06f + 0.16f * (1.0f - params.drive))));

    y = (y + postBias) * postPre;
    const auto a = std::abs(y);
    const auto coeff = a > st.env ? attack : release;
    st.env = a + coeff * (st.env - a);
    auto g = 1.0f;
    if (st.env > thr)
        g = 1.0f / (1.0f + compAmt * (st.env - thr) * 4.2f);
    y = softClip(y * g) - postBias * 0.5f;

    const auto crushAmt = clampf(params.crush, 0.0f, 1.0f);
    if (crushAmt > 0.0001f)
    {
        const auto bits = int(std::round(16.0f - crushAmt * 12.0f));
        const auto quant = std::pow(2.0f, float(juce::jmax(1, bits - 1)));
        const auto downsample = juce::jmax(1, int(std::round(1.0f + crushAmt * 15.0f)));

        if (st.crushPhase == 0)
            st.crushHold = clampf(std::round(y * quant) / quant, -1.0f, 1.0f);

        st.crushPhase = (st.crushPhase + 1) % downsample;
        y = st.crushHold;
    }
    else
    {
        st.crushPhase = 0;
        st.crushHold = y;
    }

    if (includeEvents)
    {
        maybeTriggerDropout((float) sampleRate, clampf(params.dropRate, 0.0f, 1.0f), clampf(params.dropDepth, 0.0f, 1.0f));
        maybeTriggerCrackle((float) sampleRate, clampf(params.crackle, 0.0f, 1.0f));
        lfoPhase += juce::MathConstants<float>::twoPi * params.lfoRate / (float) sampleRate;
        if (lfoPhase > juce::MathConstants<float>::twoPi)
            lfoPhase -= juce::MathConstants<float>::twoPi;
    }

    const auto wow = includeEvents ? (1.0f - clampf(params.wowDepth, 0.0f, 1.0f) * 0.45f * (0.5f + 0.5f * std::sin(lfoPhase))) : 1.0f;

    auto drop = 1.0f;
    if (includeEvents && dropoutRemaining > 0)
    {
        const auto t = 1.0f - (float) dropoutRemaining / (float) dropoutTotal;
        const auto fade = t < 0.2f ? t / 0.2f : (t > 0.85f ? (1.0f - t) / 0.15f : 1.0f);
        drop = 1.0f - (1.0f - dropoutDepth) * fade;
        --dropoutRemaining;
    }

    auto crackle = 0.0f;
    if (includeEvents && crackleRemaining > 0)
    {
        const auto t = 1.0f - (float) crackleRemaining / (float) crackleTotal;
        const auto env = (t < 0.15f ? t / 0.15f : 1.0f) * (t > 0.7f ? (1.0f - t) / 0.3f : 1.0f);
        crackle = nextWhite() * (0.10f + 0.30f * params.crackle) * env;
        --crackleRemaining;
    }

    const auto white = nextWhite();
    const auto pink = pinkFromWhite(st.pink, white);
    const auto n = (1.0f - params.noiseColor) * white + params.noiseColor * pink;
    const auto hissHp = n - st.prevNoise;
    st.prevNoise = n;
    const auto noiseLevel = params.noiseProfile * 0.12f;
    const auto noiseOut = includeEvents ? (n * noiseLevel + hissHp * (noiseLevel * (params.hiss * 0.6f))) : 0.0f;

    y = y * wow * drop + crackle + noiseOut;
    return clampf(y * params.outGain, -1.0f, 1.0f);
}

void TransmissionEngineAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto totalInputChannels = getTotalNumInputChannels();
    const auto totalOutputChannels = getTotalNumOutputChannels();
    for (auto i = totalInputChannels; i < totalOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    updateFilters(getSampleRate());

    RuntimeParams params;
    params.drive = apvts.getRawParameterValue("drive")->load();
    params.asym = apvts.getRawParameterValue("asym")->load();
    params.comp = apvts.getRawParameterValue("comp")->load();
    params.crush = apvts.getRawParameterValue("crush")->load();
    params.wowDepth = apvts.getRawParameterValue("wowDepth")->load();
    params.dropRate = apvts.getRawParameterValue("dropRate")->load();
    params.dropDepth = apvts.getRawParameterValue("dropDepth")->load();
    params.crackle = apvts.getRawParameterValue("crackle")->load();
    params.lfoRate = apvts.getRawParameterValue("lfoRate")->load();
    params.noiseProfile = apvts.getRawParameterValue("noiseProfile")->load();
    params.noiseColor = apvts.getRawParameterValue("noiseColor")->load();
    params.hiss = apvts.getRawParameterValue("hiss")->load();
    params.outGain = apvts.getRawParameterValue("outGain")->load();
    params.tuningEnable = apvts.getRawParameterValue("tuningEnable")->load() > 0.5f;
    params.tuningMode = int(apvts.getRawParameterValue("tuningMode")->load());
    params.tuningSource = juce::jlimit(0, 7, (int) apvts.getRawParameterValue("tuningSource")->load());
    params.tuningAmount = apvts.getRawParameterValue("tuningAmount")->load();
    params.tuningSnippetMs = apvts.getRawParameterValue("tuningSnippetMs")->load();
    params.tuningCutDepth = apvts.getRawParameterValue("tuningCutDepth")->load();
    params.passes = juce::jlimit(1, 6, (int) apvts.getRawParameterValue("passes")->load());

    bool isPlaying = false;
    if (auto* playHead = getPlayHead())
    {
        juce::AudioPlayHead::CurrentPositionInfo pos;
        if (playHead->getCurrentPosition(pos))
            isPlaying = pos.isPlaying;
    }

    const auto transportStarted = isPlaying && !transportWasPlaying;
    const auto transportStopped = !isPlaying && transportWasPlaying;
    transportWasPlaying = isPlaying;

    if (!params.tuningEnable)
    {
        tuningRemaining = 0;
        tuningCooldownRemaining = 0;
    }
    else if (params.tuningMode == 0 && (transportStarted || transportStopped))
    {
        triggerTuningEvent((float) getSampleRate(), params.tuningSnippetMs, params.tuningSource);
    }

    std::array<float*, 2> channels {};
    for (int ch = 0; ch < totalInputChannels; ++ch)
        channels[(size_t) ch] = buffer.getWritePointer(ch);

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float tuningDuck = 1.0f;
        const auto tuningAdd = nextTuningSample(getSampleRate(), params, isPlaying, tuningDuck);

        for (int ch = 0; ch < totalInputChannels; ++ch)
        {
            auto y = channels[(size_t) ch][i];
            for (int p = 0; p < params.passes; ++p)
                y = processChannelSample(p, ch, y, getSampleRate(), params, p == 0);

            y = y * tuningDuck + tuningAdd;
            channels[(size_t) ch][i] = clampf(y, -1.0f, 1.0f);
        }
    }
}

bool TransmissionEngineAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* TransmissionEngineAudioProcessor::createEditor()
{
    return new TransmissionEngineAudioProcessorEditor(*this);
}

void TransmissionEngineAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void TransmissionEngineAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TransmissionEngineAudioProcessor();
}
