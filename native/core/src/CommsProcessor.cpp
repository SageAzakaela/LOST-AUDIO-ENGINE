#include <lost_audio/core/CommsProcessor.h>

#include <algorithm>
#include <cmath>

namespace lost_audio::core
{
namespace
{
constexpr float pi = 3.14159265358979323846f;
float clamp01(float value) noexcept { return std::clamp(value, 0.0f, 1.0f); }
float softClip(float value) noexcept { return std::tanh(value); }

struct MacroBase
{
    float hp, hpRange, lp, lpRange, hump, humpRange, mid, midRange;
    float compression, output, ceiling, device, line, duplex, rattle, bits, rate, rateRange;
};

MacroBase baseFor(CommsMode mode) noexcept
{
    switch (mode)
    {
        case CommsMode::cellular:      return { 170, 390, 7200, 4600, 1.4f, 3.8f, 2050, 550, 0.58f, 0.98f, 0.92f, 0.20f, 0.04f, 0.08f, 0.04f, 13, 32000, 25000 };
        case CommsMode::intercom:      return { 310, 420, 4100, 1900, 4.6f, 5.8f, 1950, 420, 0.65f, 0.94f, 0.90f, 0.68f, 0.28f, 0.48f, 0.50f, 15, 44000, 30000 };
        case CommsMode::publicAddress: return { 120, 330, 9800, 5200, 1.2f, 3.0f, 1420, 400, 0.46f, 0.94f, 0.90f, 0.58f, 0.10f, 0.04f, 0.46f, 16, 48000, 28000 };
        case CommsMode::alarmPanel:    return { 230, 390, 8200, 4300, 1.4f, 3.2f, 1740, 620, 0.48f, 0.94f, 0.90f, 0.62f, 0.16f, 0.02f, 0.28f, 15, 46000, 30000 };
        case CommsMode::landline:
        default:                       return { 250, 330, 3900, 1500, 3.0f, 5.0f, 1820, 360, 0.52f, 0.94f, 0.92f, 0.52f, 0.30f, 0.02f, 0.16f, 14, 44000, 30000 };
    }
}
}

CommsMacroTargets mapCommsMacros(CommsMode mode, float bandwidth, float drive, float glitch,
                                  float noise, float character, float distance) noexcept
{
    const auto bw = clamp01(bandwidth);
    const auto drv = std::pow(clamp01(drive), 1.25f);
    const auto failure = std::pow(clamp01(glitch), 1.35f);
    const auto bed = std::pow(clamp01(noise), 1.2f);
    const auto device = std::pow(clamp01(character), 0.9f);
    const auto perspective = clamp01(distance);
    const auto narrow = std::pow(1.0f - bw, 1.35f);
    const auto base = baseFor(mode);

    CommsMacroTargets result;
    result.highPassHz = std::round(base.hp + narrow * base.hpRange);
    result.lowPassHz = std::round(base.lp - narrow * base.lpRange);
    result.midHumpDb = std::round((base.hump + narrow * base.humpRange) * 20.0f) / 20.0f;
    result.midFrequencyHz = std::round(base.mid + (0.55f - narrow) * base.midRange);
    result.compression = clamp01(base.compression + drv * 0.38f);
    result.bits = (int) std::round((1.0f - failure) * (base.bits - 5.0f) + 5.0f);
    result.converterRateHz = std::round(base.rate - failure * base.rateRange);
    const auto packetScale = mode == CommsMode::cellular ? 0.72f : mode == CommsMode::alarmPanel ? 0.35f : 0.25f;
    result.packetLoss = clamp01(failure * packetScale);
    result.packetLengthMs = std::round(10.0f + failure * (mode == CommsMode::cellular ? 120.0f : 75.0f));
    result.hum = clamp01(0.06f + bed * (mode == CommsMode::intercom ? 0.55f : 0.40f));
    result.hiss = clamp01(0.08f + bed * 0.55f);
    result.toneMix = clamp01((mode == CommsMode::alarmPanel ? 0.45f : 0.22f) + bed * 0.15f);
    result.transducer = clamp01(base.device * 0.5f + device * 0.72f);
    result.lineAge = clamp01(base.line + drv * 0.24f + bed * 0.18f);
    result.duplex = clamp01(base.duplex + failure * (mode == CommsMode::intercom ? 0.35f : 0.18f)
                            + device * (mode == CommsMode::intercom ? 0.12f : 0.03f));
    result.speakerRattle = clamp01(base.rattle * (0.35f + device * 0.9f)
                                  + drv * (mode == CommsMode::publicAddress || mode == CommsMode::intercom ? 0.28f : 0.12f));
    result.distance = perspective;
    result.ceiling = base.ceiling;
    result.outputGain = std::round((base.output + drv * 0.12f) * 100.0f) / 100.0f;

    const auto roomBase = mode == CommsMode::intercom ? 0.18f : mode == CommsMode::publicAddress ? 0.12f
                        : mode == CommsMode::alarmPanel ? 0.14f : 0.05f;
    result.roomMix = clamp01(roomBase + bed * 0.12f);
    const auto roomBaseMs = mode == CommsMode::intercom ? 420.0f : mode == CommsMode::publicAddress ? 540.0f
                          : mode == CommsMode::alarmPanel ? 360.0f : 220.0f;
    result.roomMs = std::round(roomBaseMs * (0.75f + 0.55f * bed));
    result.roomDamping = clamp01(mode == CommsMode::intercom ? 0.70f : mode == CommsMode::publicAddress ? 0.55f : 0.45f + bed * 0.1f);
    const auto echoBase = mode == CommsMode::publicAddress ? 0.14f : mode == CommsMode::intercom ? 0.08f
                        : mode == CommsMode::alarmPanel ? 0.05f : 0.03f;
    result.echoMix = clamp01(echoBase + failure * 0.08f);
    const auto echoBaseMs = mode == CommsMode::publicAddress ? 240.0f : mode == CommsMode::intercom ? 260.0f : 180.0f;
    result.echoMs = std::round(echoBaseMs * (0.85f + 0.35f * failure));
    result.echoFeedback = clamp01(0.12f + (mode == CommsMode::publicAddress ? 0.35f : 0.22f) * failure);
    result.echoTone = clamp01(mode == CommsMode::intercom ? 0.45f : 0.60f + bed * 0.15f);
    return result;
}

float CommsProcessor::Biquad::process(float input) noexcept
{
    const auto output = b0 * input + z1;
    z1 = b1 * input - a1 * output + z2;
    z2 = b2 * input - a2 * output;
    return output;
}

void CommsProcessor::Biquad::setHighPass(double sr, float frequency, float q) noexcept
{
    const auto f = std::clamp(frequency, 20.0f, (float) sr * 0.45f);
    const auto omega = 2.0f * pi * f / (float) sr;
    const auto cosine = std::cos(omega);
    const auto alpha = std::sin(omega) / (2.0f * std::max(0.08f, q));
    const auto a0 = 1.0f + alpha;
    b0 = ((1.0f + cosine) * 0.5f) / a0; b1 = -(1.0f + cosine) / a0; b2 = b0;
    a1 = (-2.0f * cosine) / a0; a2 = (1.0f - alpha) / a0;
}

void CommsProcessor::Biquad::setLowPass(double sr, float frequency, float q) noexcept
{
    const auto f = std::clamp(frequency, 20.0f, (float) sr * 0.45f);
    const auto omega = 2.0f * pi * f / (float) sr;
    const auto cosine = std::cos(omega);
    const auto alpha = std::sin(omega) / (2.0f * std::max(0.08f, q));
    const auto a0 = 1.0f + alpha;
    b0 = ((1.0f - cosine) * 0.5f) / a0; b1 = (1.0f - cosine) / a0; b2 = b0;
    a1 = (-2.0f * cosine) / a0; a2 = (1.0f - alpha) / a0;
}

void CommsProcessor::Biquad::setPeak(double sr, float frequency, float q, float gainDb) noexcept
{
    const auto f = std::clamp(frequency, 20.0f, (float) sr * 0.45f);
    const auto omega = 2.0f * pi * f / (float) sr;
    const auto cosine = std::cos(omega);
    const auto alpha = std::sin(omega) / (2.0f * std::max(0.08f, q));
    const auto a = std::pow(10.0f, gainDb / 40.0f);
    const auto a0 = 1.0f + alpha / a;
    b0 = (1.0f + alpha * a) / a0; b1 = (-2.0f * cosine) / a0; b2 = (1.0f - alpha * a) / a0;
    a1 = b1; a2 = (1.0f - alpha / a) / a0;
}

void CommsProcessor::FilterChain::reset() noexcept
{
    hp1.reset(); hp2.reset(); dip.reset(); hump.reset(); lp1.reset(); lp2.reset();
    bodyLow.reset(); bodyHigh.reset(); bodyNotch.reset();
}

void CommsProcessor::prepare(double sampleRate, std::size_t channelCount)
{
    sampleRate_ = std::clamp(sampleRate, 8000.0, 384000.0);
    preparedChannels_ = std::clamp(channelCount, std::size_t { 1 }, maxChannels);
    echo_.samples.assign((std::size_t) std::ceil(sampleRate_ * 2.55), 0.0f);
    for (auto& line : room_) line.samples.assign((std::size_t) std::ceil(sampleRate_ * 2.55), 0.0f);
    reset(seed_);
}

void CommsProcessor::reset(std::uint32_t seed) noexcept
{
    seed_ = seed == 0 ? 0x636f6d6du : seed;
    randomState_ = seed_;
    filters_.reset();
    std::fill(echo_.samples.begin(), echo_.samples.end(), 0.0f);
    echo_.writeIndex = 0; echo_.dampingState = 0.0f;
    for (auto& line : room_)
    {
        std::fill(line.samples.begin(), line.samples.end(), 0.0f);
        line.writeIndex = 0; line.dampingState = 0.0f;
    }
    envelope_ = limiterEnvelope_ = hold_ = 0.0f;
    holdCount_ = 0; holdPeriod_ = 1; dropoutRemaining_ = packetRemaining_ = 0; dropoutGain_ = 1.0f;
    humPhase_ = lineNoise_ = carbonNoise_ = 0.0f; duplexGain_ = 1.0f; duplexHold_ = 0;
    speakerLow_ = speakerBand_ = previousSignal_ = 0.0f;
    codecPreviousInput_ = codecPreviousOutput_ = echoToneState_ = distanceLowpass_ = 0.0f;
    tonePhaseA_ = tonePhaseB_ = warblePhase_ = outputPeak_ = 0.0f;
}

std::uint32_t CommsProcessor::nextU32(std::uint32_t& state) noexcept
{
    auto x = state == 0 ? 0x12345678u : state;
    x ^= x << 13u; x ^= x >> 17u; x ^= x << 5u; state = x; return x;
}

float CommsProcessor::nextFloat(std::uint32_t& state) noexcept
{
    return (float) ((double) nextU32(state) / 4294967295.0);
}

float CommsProcessor::nextSigned(std::uint32_t& state) noexcept
{
    return nextFloat(state) * 2.0f - 1.0f;
}

float CommsProcessor::readDelay(const DelayLine& line, float delaySamples) const noexcept
{
    if (line.samples.empty()) return 0.0f;
    auto read = (float) line.writeIndex - std::clamp(delaySamples, 1.0f, (float) line.samples.size() - 2.0f);
    while (read < 0.0f) read += (float) line.samples.size();
    const auto floorRead = std::floor(read);
    const auto first = (std::size_t) floorRead % line.samples.size();
    const auto second = (first + 1) % line.samples.size();
    const auto fraction = read - floorRead;
    return line.samples[first] + (line.samples[second] - line.samples[first]) * fraction;
}

void CommsProcessor::updateFilters(const CommsParameters& p) noexcept
{
    filters_.hp1.setHighPass(sampleRate_, p.highPassHz, 0.707f);
    filters_.hp2.setHighPass(sampleRate_, p.highPassHz, 0.707f);
    filters_.dip.setPeak(sampleRate_, 650.0f, 0.9f, -0.35f * p.midHumpDb);
    filters_.hump.setPeak(sampleRate_, p.midFrequencyHz, 1.25f, p.midHumpDb);
    filters_.lp1.setLowPass(sampleRate_, p.lowPassHz, 0.85f);
    filters_.lp2.setLowPass(sampleRate_, p.lowPassHz, 0.85f);

    float lowHz = 840.0f, lowDb = 4.8f, lowQ = 1.55f;
    float highHz = 2380.0f, highDb = 4.4f, highQ = 2.25f;
    float notchHz = 1320.0f, notchDb = -1.9f;
    switch (p.mode)
    {
        case CommsMode::cellular:      lowHz = 1080; lowDb = 2.4f; lowQ = 1.15f; highHz = 2850; highDb = 2.7f; highQ = 2.1f; notchHz = 1820; notchDb = -1.1f; break;
        case CommsMode::intercom:      lowHz = 710; lowDb = 6.8f; lowQ = 2.05f; highHz = 2180; highDb = 7.2f; highQ = 2.75f; notchHz = 1280; notchDb = -3.2f; break;
        case CommsMode::publicAddress: lowHz = 460; lowDb = 5.2f; lowQ = 1.45f; highHz = 1580; highDb = 5.8f; highQ = 1.75f; notchHz = 910; notchDb = -2.6f; break;
        case CommsMode::alarmPanel:    lowHz = 920; lowDb = 5.8f; lowQ = 2.3f; highHz = 2740; highDb = 4.6f; highQ = 2.5f; notchHz = 1640; notchDb = -1.8f; break;
        case CommsMode::landline: default: break;
    }
    const auto character = clamp01(p.transducer);
    filters_.bodyLow.setPeak(sampleRate_, lowHz, lowQ, lowDb * character);
    filters_.bodyHigh.setPeak(sampleRate_, highHz, highQ, highDb * character);
    filters_.bodyNotch.setPeak(sampleRate_, notchHz, 1.6f + character * 0.9f, notchDb * character);
}

void CommsProcessor::process(float* const* channels, std::size_t channelCount, std::size_t sampleCount,
                             const CommsParameters& raw) noexcept
{
    const auto count = std::min({ channelCount, preparedChannels_, maxChannels });
    if (count == 0 || sampleCount == 0 || channels == nullptr) return;
    auto p = raw;
    p.highPassHz = std::clamp(p.highPassHz, 40.0f, 1200.0f);
    p.lowPassHz = std::clamp(p.lowPassHz, 800.0f, 14000.0f);
    p.midHumpDb = std::clamp(p.midHumpDb, 0.0f, 14.0f);
    p.midFrequencyHz = std::clamp(p.midFrequencyHz, 600.0f, 5000.0f);
    updateFilters(p);

    const auto sr = (float) sampleRate_;
    const auto mode = p.mode;
    const auto drive = clamp01(p.drive);
    const auto compression = clamp01(p.compression);
    const auto lineAge = clamp01(p.lineAge);
    const auto target = mode == CommsMode::publicAddress ? 0.22f : mode == CommsMode::intercom ? 0.19f : 0.17f;
    const auto attack = std::exp(-1.0f / ((mode == CommsMode::publicAddress ? 0.003f : 0.008f) * sr));
    const auto release = std::exp(-1.0f / ((mode == CommsMode::intercom ? 0.22f : 0.14f) * sr));
    const auto driveGain = 1.0f + drive * (mode == CommsMode::publicAddress ? 7.0f : mode == CommsMode::intercom ? 5.5f : 4.5f);
    const auto asymmetry = (mode == CommsMode::landline || mode == CommsMode::intercom ? 0.045f : 0.018f) * drive;
    const auto driveCenter = softClip(asymmetry * driveGain);
    const auto levelMatch = 0.18f / std::max(1.0e-5f, softClip(0.18f * driveGain));
    const auto bits = std::clamp(p.bits, 4, 16);
    const auto quantLevels = std::max(1.0f, (float) ((1 << (bits - 1)) - 1));
    const auto converterRate = std::clamp(p.converterRateHz, 6000.0f, std::min(48000.0f, sr));
    const auto basePeriod = std::max(1, (int) std::round(sr / converterRate));
    const auto packetLength = std::max(1, (int) std::round(std::clamp(p.packetLengthMs, 8.0f, 160.0f) * 0.001f * sr));
    const auto packetLoss = clamp01(p.packetLoss);
    const auto humAmount = clamp01(p.hum);
    const auto hissAmount = clamp01(p.hiss);
    const auto humHz = mode == CommsMode::cellular ? 180.0f : mode == CommsMode::intercom ? 50.0f : 60.0f;
    const auto humDepth = humAmount * humAmount * (0.006f + lineAge * 0.018f) * (mode == CommsMode::cellular ? 0.45f : 1.0f);
    const auto hissDepth = hissAmount * hissAmount * (0.009f + lineAge * 0.022f);
    const auto duplexScale = mode == CommsMode::intercom ? 1.0f : mode == CommsMode::cellular ? 0.45f
                           : mode == CommsMode::publicAddress ? 0.22f : mode == CommsMode::landline ? 0.12f : 0.05f;
    const auto duplexAmount = clamp01(p.duplex) * duplexScale;
    const auto gateThreshold = 0.0015f + duplexAmount * duplexAmount * 0.035f;
    const auto gateFloor = 1.0f - duplexAmount * 0.94f;
    const auto gateAttack = 1.0f - std::exp(-1.0f / (0.0025f * sr));
    const auto gateRelease = 1.0f - std::exp(-1.0f / ((0.035f + duplexAmount * 0.12f) * sr));
    const auto gateHoldSamples = (int) std::round((0.018f + duplexAmount * 0.075f) * sr);
    const auto speakerHz = mode == CommsMode::landline ? 920.0f : mode == CommsMode::cellular ? 2550.0f
                         : mode == CommsMode::intercom ? 720.0f : mode == CommsMode::publicAddress ? 470.0f : 1080.0f;
    const auto speakerF = 2.0f * std::sin(pi * speakerHz / sr);
    const auto speakerDamp = mode == CommsMode::publicAddress ? 0.34f : mode == CommsMode::intercom ? 0.28f : 0.46f;
    const auto rattle = clamp01(p.speakerRattle) * (0.25f + clamp01(p.transducer) * 0.75f);
    const auto echoToneHz = 900.0f + clamp01(p.echoTone) * 7500.0f;
    const auto echoAlpha = std::exp(-2.0f * pi * echoToneHz / sr);
    const auto distance = clamp01(p.distance);
    const auto distanceCut = 14500.0f - std::pow(distance, 0.72f) * 11600.0f;
    const auto distanceAlpha = std::exp(-2.0f * pi * std::min(distanceCut, sr * 0.45f) / sr);
    const auto sceneScale = mode == CommsMode::publicAddress ? 0.42f : (mode == CommsMode::intercom || mode == CommsMode::alarmPanel) ? 0.28f : 0.14f;
    const auto echoWet = std::min(1.25f, clamp01(p.echoMix) * 0.95f + distance * sceneScale * 0.35f);
    const auto roomWet = std::min(1.35f, clamp01(p.roomMix) * 1.15f + distance * sceneScale);
    const auto directGain = std::max(0.28f, (1.0f - std::max(clamp01(p.echoMix), clamp01(p.roomMix)) * 0.5f) * (1.0f - distance * 0.62f));
    const auto roomDamping = clamp01(p.roomDamping);
    const auto roomFeedback = std::clamp(0.24f + (std::clamp(p.roomMs, 35.0f, 2500.0f) / 2500.0f) * 0.54f, 0.24f, 0.78f);
    const auto ceiling = std::clamp(p.ceiling, 0.2f, 1.0f);
    const auto finalMix = clamp01(p.mix);
    const auto limiterAttack = std::exp(-1.0f / (0.002f * sr));
    const auto limiterRelease = std::exp(-1.0f / (0.06f * sr));
    outputPeak_ = 0.0f;

    constexpr float roomRatios[] { 1.0f, 0.83f, 0.67f, 0.51f };
    for (std::size_t sample = 0; sample < sampleCount; ++sample)
    {
        auto dryMono = 0.0f;
        for (std::size_t channel = 0; channel < count; ++channel) dryMono += channels[channel][sample];
        dryMono = dryMono / (float) count;
        auto x = dryMono * std::clamp(p.inputGain, 0.0f, 15.85f);
        x = filters_.hp1.process(x); x = filters_.hp2.process(x); x = filters_.dip.process(x);
        x = filters_.hump.process(x); x = filters_.lp1.process(x); x = filters_.lp2.process(x);

        const auto magnitude = std::abs(x);
        const auto envelopeCoefficient = magnitude > envelope_ ? attack : release;
        envelope_ = magnitude + envelopeCoefficient * (envelope_ - magnitude);
        const auto wantedGain = std::clamp(target / (envelope_ + 1.0e-6f), 0.32f,
                                           mode == CommsMode::publicAddress ? 3.2f : 4.5f);
        const auto agc = 1.0f + (wantedGain - 1.0f) * compression;
        auto y = (softClip((x * agc + asymmetry) * driveGain) - driveCenter) * levelMatch;

        carbonNoise_ += (nextSigned(randomState_) - carbonNoise_)
                      * (mode == CommsMode::landline ? 0.18f : mode == CommsMode::intercom ? 0.11f : 0.07f);
        const auto carbonScale = mode == CommsMode::landline ? 1.0f : mode == CommsMode::intercom ? 0.75f
                               : mode == CommsMode::publicAddress ? 0.42f : 0.18f;
        y *= 1.0f + carbonNoise_ * lineAge * lineAge * carbonScale * 0.24f;

        if (holdCount_ <= 0)
        {
            const auto jitter = packetLoss > 0.0f ? nextSigned(randomState_) * packetLoss * 0.35f : 0.0f;
            holdPeriod_ = std::max(1, (int) std::round((float) basePeriod * (1.0f + jitter)));
            hold_ = y; holdCount_ = holdPeriod_;
        }
        y = hold_; --holdCount_;
        const auto codecMix = clamp01((14.0f - (float) bits) / 10.0f + lineAge * 0.16f);
        if (mode == CommsMode::cellular)
        {
            const auto prediction = codecPreviousInput_ * 0.78f;
            const auto residual = std::clamp(y - prediction, -1.0f, 1.0f);
            const auto quantized = std::round(residual * quantLevels) / quantLevels;
            const auto decoded = std::clamp(quantized + codecPreviousOutput_ * 0.78f, -1.0f, 1.0f);
            codecPreviousInput_ = y; codecPreviousOutput_ = decoded;
            y += (decoded - y) * codecMix;
        }
        else if (mode == CommsMode::landline)
        {
            const auto mu = 31.0f + lineAge * 224.0f;
            const auto clipped = std::clamp(y, -1.0f, 1.0f);
            const auto encoded = std::copysign(std::log1p(mu * std::abs(clipped)) / std::log1p(mu), clipped);
            const auto quantized = std::round(encoded * quantLevels) / quantLevels;
            const auto decoded = std::copysign(std::expm1(std::abs(quantized) * std::log1p(mu)) / mu, quantized);
            y += (decoded - y) * codecMix;
        }
        else
        {
            const auto quantized = std::round(std::clamp(y, -1.0f, 1.0f) * quantLevels) / quantLevels;
            y += (quantized - y) * codecMix;
        }

        if (packetRemaining_ <= 0)
        {
            packetRemaining_ = packetLength;
            dropoutRemaining_ = nextFloat(randomState_) < packetLoss * packetLoss ? packetLength : 0;
        }
        --packetRemaining_;
        const auto dropoutTarget = dropoutRemaining_ > 0 ? 0.0f : 1.0f;
        dropoutGain_ = std::clamp(dropoutGain_ + (dropoutTarget - dropoutGain_) / 48.0f, 0.0f, 1.0f);
        if (dropoutRemaining_ > 0) --dropoutRemaining_;
        y *= dropoutGain_;

        if (envelope_ >= gateThreshold) duplexHold_ = gateHoldSamples;
        else if (duplexHold_ > 0) --duplexHold_;
        const auto duplexTarget = duplexHold_ > 0 || envelope_ >= gateThreshold ? 1.0f : gateFloor;
        duplexGain_ += (duplexTarget - duplexGain_) * (duplexTarget > duplexGain_ ? gateAttack : gateRelease);
        y *= duplexGain_;

        speakerLow_ += speakerF * speakerBand_;
        const auto speakerHigh = y - speakerLow_ - speakerDamp * speakerBand_;
        speakerBand_ += speakerF * speakerHigh;
        const auto transient = y - previousSignal_; previousSignal_ = y;
        const auto body = std::clamp(speakerBand_ + transient * (mode == CommsMode::publicAddress ? 0.55f : 0.25f), -1.5f, 1.5f);
        y += body * rattle * (mode == CommsMode::publicAddress ? 0.24f : mode == CommsMode::intercom ? 0.20f : 0.12f);

        humPhase_ += 2.0f * pi * humHz / sr;
        if (humPhase_ > 2.0f * pi) humPhase_ -= 2.0f * pi;
        const auto humSignal = (std::sin(humPhase_) + 0.38f * std::sin(humPhase_ * 2.01f)) * humDepth;
        lineNoise_ += (nextSigned(randomState_) - lineNoise_) * (mode == CommsMode::landline ? 0.32f : 0.52f);
        const auto hissSignal = (0.72f * lineNoise_ + 0.28f * nextSigned(randomState_)) * hissDepth;
        y += humSignal + hissSignal;

        if (p.alarmTone && p.toneMix > 0.0001f)
        {
            const auto toneMix = clamp01(p.toneMix);
            warblePhase_ += 2.0f * pi * (mode == CommsMode::alarmPanel ? 2.1f : 2.7f) / sr;
            if (warblePhase_ > 2.0f * pi) warblePhase_ -= 2.0f * pi;
            const auto wobble = 0.5f + 0.5f * std::sin(warblePhase_);
            const auto baseA = mode == CommsMode::alarmPanel ? 960.0f : 880.0f;
            const auto baseB = mode == CommsMode::alarmPanel ? 1400.0f : 1200.0f;
            tonePhaseA_ += 2.0f * pi * baseA * (0.96f + 0.08f * wobble) / sr;
            tonePhaseB_ += 2.0f * pi * baseB * (1.03f - 0.06f * wobble) / sr;
            if (tonePhaseA_ > 2.0f * pi) tonePhaseA_ -= 2.0f * pi;
            if (tonePhaseB_ > 2.0f * pi) tonePhaseB_ -= 2.0f * pi;
            const auto tone = (std::sin(tonePhaseA_) + 0.6f * std::sin(tonePhaseB_)) * (0.14f + 0.25f * toneMix);
            y = y * (1.0f - 0.43f * toneMix) + tone * (0.38f * toneMix);
        }

        y = filters_.bodyLow.process(y); y = filters_.bodyHigh.process(y); y = filters_.bodyNotch.process(y);
        const auto echoTap = readDelay(echo_, std::clamp(p.echoMs, 10.0f, 2500.0f) * 0.001f * sr);
        echoToneState_ = (1.0f - echoAlpha) * echoTap + echoAlpha * echoToneState_;
        echo_.samples[echo_.writeIndex] = std::clamp(y + echoToneState_ * std::clamp(p.echoFeedback, 0.0f, 0.92f), -1.5f, 1.5f);
        echo_.writeIndex = (echo_.writeIndex + 1) % echo_.samples.size();

        auto roomSum = 0.0f;
        for (std::size_t lineIndex = 0; lineIndex < room_.size(); ++lineIndex)
        {
            auto& line = room_[lineIndex];
            const auto tap = readDelay(line, std::clamp(p.roomMs * roomRatios[lineIndex], 28.0f, 2500.0f) * 0.001f * sr);
            line.dampingState += (tap - line.dampingState) * (0.45f - roomDamping * 0.34f);
            line.samples[line.writeIndex] = std::clamp(y * 0.22f + line.dampingState * roomFeedback, -1.2f, 1.2f);
            line.writeIndex = (line.writeIndex + 1) % line.samples.size();
            roomSum += tap * (lineIndex % 2 == 0 ? 1.0f : -0.72f);
        }
        roomSum *= 0.28f;
        auto processed = y * directGain + echoTap * echoWet + roomSum * roomWet;
        distanceLowpass_ = (1.0f - distanceAlpha) * processed + distanceAlpha * distanceLowpass_;
        processed = distanceLowpass_ * (1.0f - distance * 0.28f) * std::clamp(p.outputGain, 0.0f, 1.5f);
        const auto postMagnitude = std::abs(processed);
        const auto limiterCoefficient = postMagnitude > limiterEnvelope_ ? limiterAttack : limiterRelease;
        limiterEnvelope_ = postMagnitude + limiterCoefficient * (limiterEnvelope_ - postMagnitude);
        if (limiterEnvelope_ > ceiling) processed *= ceiling / (limiterEnvelope_ + 1.0e-6f);
        processed = std::clamp(processed, -ceiling, ceiling);

        for (std::size_t channel = 0; channel < count; ++channel)
        {
            const auto output = std::clamp(channels[channel][sample] * (1.0f - finalMix) + processed * finalMix, -1.0f, 1.0f);
            channels[channel][sample] = output;
            outputPeak_ = std::max(outputPeak_, std::abs(output));
        }
    }
}
}
