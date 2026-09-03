#include <lost_audio/core/OpenMicProcessor.h>

#include <algorithm>
#include <cmath>

namespace lost_audio::core
{
namespace
{
constexpr float pi = 3.14159265358979323846f;

struct ModelShape
{
    float hp, presenceHz, presenceDb, airHz, airDb, drive;
};

constexpr std::array<ModelShape, 5> micShapes {{
    { 82.0f, 3100.0f, 2.4f, 9200.0f, -1.2f, 1.05f },
    { 58.0f, 4400.0f, 1.7f, 12500.0f, 1.8f, 0.82f },
    { 125.0f, 2300.0f, 4.2f, 6900.0f, -5.0f, 1.55f },
    { 105.0f, 1850.0f, 3.1f, 7800.0f, -3.0f, 1.22f },
    { 48.0f, 1450.0f, 1.5f, 7200.0f, -3.5f, 0.92f }
}};

constexpr std::array<ModelShape, 5> paShapes {{
    { 70.0f, 920.0f, 1.8f, 11800.0f, -1.0f, 1.00f },
    { 105.0f, 1350.0f, 2.4f, 9800.0f, -2.2f, 1.08f },
    { 210.0f, 2550.0f, 5.1f, 7200.0f, -4.5f, 1.32f },
    { 92.0f, 760.0f, 3.8f, 6400.0f, -6.0f, 1.62f },
    { 78.0f, 1180.0f, 5.0f, 5300.0f, -8.0f, 1.95f }
}};

struct VenueShape
{
    float earlyMs, lateMs, dampingHz, bodyHz, bodyDb, crowdDarkness;
};

constexpr std::array<VenueShape, 6> venueShapes {{
    { 17.0f, 91.0f, 6200.0f, 440.0f, 1.2f, 0.55f },
    { 11.0f, 67.0f, 5100.0f, 330.0f, 2.2f, 0.68f },
    { 8.0f, 43.0f, 7600.0f, 610.0f, 1.4f, 0.30f },
    { 31.0f, 181.0f, 8200.0f, 270.0f, 2.8f, 0.18f },
    { 47.0f, 133.0f, 11800.0f, 520.0f, 0.7f, 0.12f },
    { 23.0f, 126.0f, 5800.0f, 390.0f, 2.0f, 0.48f }
}};

float coefficient(float milliseconds, double sampleRate) noexcept
{
    const auto seconds = std::max(0.001f, milliseconds * 0.001f);
    return std::exp(-1.0f / (seconds * static_cast<float>(sampleRate)));
}
} // namespace

OpenMicMacroTargets mapOpenMicMacros(OpenMicModel mic,
                                     OpenMicVenue venue,
                                     OpenMicPA pa,
                                     float hotMic,
                                     float distance,
                                     float room) noexcept
{
    hotMic = std::clamp(hotMic, 0.0f, 1.0f);
    distance = std::clamp(distance, 0.0f, 1.0f);
    room = std::clamp(room, 0.0f, 1.0f);
    const auto micIndex = std::clamp(static_cast<int>(mic), 0, 4);
    const auto venueIndex = std::clamp(static_cast<int>(venue), 0, 5);
    const auto paIndex = std::clamp(static_cast<int>(pa), 0, 4);

    OpenMicMacroTargets result;
    result.proximity = std::clamp(0.78f - distance * 0.66f, 0.0f, 1.0f);
    result.micDrive = std::clamp(0.06f + hotMic * 0.58f + micShapes[static_cast<std::size_t>(micIndex)].drive * 0.04f, 0.0f, 1.0f);
    result.paDrive = std::clamp(0.05f + hotMic * 0.42f + paShapes[static_cast<std::size_t>(paIndex)].drive * 0.05f, 0.0f, 1.0f);
    result.monitorLevel = std::clamp(0.14f + hotMic * 0.62f, 0.0f, 1.0f);
    result.stageBleed = std::clamp(0.035f + distance * 0.24f + room * 0.08f, 0.0f, 1.0f);
    result.roomAmount = std::clamp(0.08f + room * 0.77f, 0.0f, 1.0f);
    result.wallAbsorption = std::clamp(distance * (0.18f + venueShapes[static_cast<std::size_t>(venueIndex)].crowdDarkness * 0.42f), 0.0f, 1.0f);
    result.electricalNoise = std::clamp(0.004f + hotMic * 0.024f + static_cast<float>(paIndex) * 0.005f, 0.0f, 0.12f);
    return result;
}

float OpenMicProcessor::clamp(float value, float lo, float hi) noexcept
{
    return std::clamp(value, lo, hi);
}

float OpenMicProcessor::softClip(float value) noexcept
{
    return std::tanh(value);
}

std::uint32_t OpenMicProcessor::nextRandom(std::uint32_t& state) noexcept
{
    state ^= state << 13u;
    state ^= state >> 17u;
    state ^= state << 5u;
    return state;
}

float OpenMicProcessor::randomSigned(std::uint32_t& state) noexcept
{
    return static_cast<float>(nextRandom(state) & 0x00ffffffu) / 8388607.5f - 1.0f;
}

float OpenMicProcessor::Biquad::process(float x) noexcept
{
    const auto y = b0 * x + z1;
    z1 = b1 * x - a1 * y + z2;
    z2 = b2 * x - a2 * y;
    return y;
}

static std::array<float, 5> makeBiquad(double sampleRate, float hz, float q, int type, float gainDb) noexcept
{
    hz = std::clamp(hz, 10.0f, static_cast<float>(sampleRate * 0.46));
    q = std::max(0.1f, q);
    const auto w0 = 2.0f * pi * hz / static_cast<float>(sampleRate);
    const auto c = std::cos(w0);
    const auto s = std::sin(w0);
    const auto alpha = s / (2.0f * q);
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a0 = 1.0f, a1 = 0.0f, a2 = 0.0f;
    if (type == 0)
    {
        b0 = (1.0f + c) * 0.5f; b1 = -(1.0f + c); b2 = b0;
        a0 = 1.0f + alpha; a1 = -2.0f * c; a2 = 1.0f - alpha;
    }
    else if (type == 1)
    {
        b0 = (1.0f - c) * 0.5f; b1 = 1.0f - c; b2 = b0;
        a0 = 1.0f + alpha; a1 = -2.0f * c; a2 = 1.0f - alpha;
    }
    else if (type == 2)
    {
        const auto a = std::pow(10.0f, gainDb / 40.0f);
        b0 = 1.0f + alpha * a; b1 = -2.0f * c; b2 = 1.0f - alpha * a;
        a0 = 1.0f + alpha / a; a1 = -2.0f * c; a2 = 1.0f - alpha / a;
    }
    else
    {
        b0 = alpha; b1 = 0.0f; b2 = -alpha;
        a0 = 1.0f + alpha; a1 = -2.0f * c; a2 = 1.0f - alpha;
    }
    return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
}

static void assign(std::array<float, 5> c, float& b0, float& b1, float& b2, float& a1, float& a2) noexcept
{
    b0 = c[0]; b1 = c[1]; b2 = c[2]; a1 = c[3]; a2 = c[4];
}

void OpenMicProcessor::Biquad::setHighPass(double sr, float hz, float q) noexcept { assign(makeBiquad(sr, hz, q, 0, 0.0f), b0, b1, b2, a1, a2); }
void OpenMicProcessor::Biquad::setLowPass(double sr, float hz, float q) noexcept { assign(makeBiquad(sr, hz, q, 1, 0.0f), b0, b1, b2, a1, a2); }
void OpenMicProcessor::Biquad::setPeak(double sr, float hz, float q, float db) noexcept { assign(makeBiquad(sr, hz, q, 2, db), b0, b1, b2, a1, a2); }
void OpenMicProcessor::Biquad::setBandPass(double sr, float hz, float q) noexcept { assign(makeBiquad(sr, hz, q, 3, 0.0f), b0, b1, b2, a1, a2); }

void OpenMicProcessor::prepare(double sampleRate, std::size_t channels)
{
    sampleRate_ = std::clamp(sampleRate, 8000.0, 384000.0);
    preparedChannels_ = std::clamp<std::size_t>(channels, 1, maxChannels);
    const auto feedbackSize = std::max<std::size_t>(2048, static_cast<std::size_t>(sampleRate_ * 0.12));
    const auto roomSize = std::max<std::size_t>(4096, static_cast<std::size_t>(sampleRate_ * 0.42));
    for (auto& channel : channels_)
    {
        channel.feedbackDelay.assign(feedbackSize, 0.0f);
        channel.roomDelay.assign(roomSize, 0.0f);
    }
    reset();
}

void OpenMicProcessor::reset(std::uint32_t seed) noexcept
{
    feedbackEnvelope_ = feedbackActivity_ = crowdActivity_ = roomActivity_ = limiterActivity_ = 0.0f;
    crowdEventRemaining_ = 0; crowdEventTotal_ = 1; crowdEventStrength_ = 0.0f;
    safetyEngaged_ = false;
    inputPeak_.fill(0.0f);
    outputPeak_.fill(0.0f);
    for (std::size_t c = 0; c < maxChannels; ++c)
    {
        random_[c] = seed ^ static_cast<std::uint32_t>(0x9e3779b9u * (c + 1u));
        auto& channel = channels_[c];
        std::fill(channel.feedbackDelay.begin(), channel.feedbackDelay.end(), 0.0f);
        std::fill(channel.roomDelay.begin(), channel.roomDelay.end(), 0.0f);
        channel.feedbackWrite = channel.roomWrite = 0;
        channel.crowdEnvelope = channel.crowdTarget = channel.crowdTransient = channel.crowdVoicePhase = 0.0f;
        channel.crowdCountdown = 1;
        channel.humPhase = 0.25f * static_cast<float>(c);
        channel.limiterEnvelope = 0.0f;
        channel.micHp.reset(); channel.micPresence.reset(); channel.micAir.reset();
        channel.paHp.reset(); channel.paBody.reset(); channel.paHorn.reset(); channel.paLp.reset();
        channel.feedbackBand.reset(); channel.feedbackTone.reset();
        channel.crowdBand.reset(); channel.crowdAir.reset();
    }
}

void OpenMicProcessor::triggerCrowdEvent(OpenMicCrowdEvent type, float strength, int durationSamples) noexcept
{
    crowdEventType_ = type; crowdEventStrength_ = clamp(strength, 0.0f, 1.0f);
    crowdEventTotal_ = crowdEventRemaining_ = std::max(1, durationSamples);
    for (auto& channel : channels_) channel.crowdTransient = 0.0f;
}

float OpenMicProcessor::readDelay(const std::vector<float>& delay, std::size_t write, float samples) const noexcept
{
    if (delay.empty()) return 0.0f;
    samples = clamp(samples, 1.0f, static_cast<float>(delay.size() - 2));
    auto pos = static_cast<float>(write) - samples;
    while (pos < 0.0f) pos += static_cast<float>(delay.size());
    const auto i0 = static_cast<std::size_t>(pos) % delay.size();
    const auto i1 = (i0 + 1u) % delay.size();
    const auto fraction = pos - std::floor(pos);
    return delay[i0] + (delay[i1] - delay[i0]) * fraction;
}

void OpenMicProcessor::updateFilters(const OpenMicParameters& p) noexcept
{
    const auto mic = micShapes[static_cast<std::size_t>(std::clamp(static_cast<int>(p.mic), 0, 4))];
    const auto pa = paShapes[static_cast<std::size_t>(std::clamp(static_cast<int>(p.pa), 0, 4))];
    const auto venue = venueShapes[static_cast<std::size_t>(std::clamp(static_cast<int>(p.venue), 0, 5))];
    const auto wall = clamp(p.wallAbsorption, 0.0f, 1.0f);
    for (auto& channel : channels_)
    {
        channel.micHp.setHighPass(sampleRate_, mic.hp * (1.0f - 0.34f * p.proximity), 0.707f);
        channel.micPresence.setPeak(sampleRate_, mic.presenceHz, 0.82f, mic.presenceDb);
        channel.micAir.setPeak(sampleRate_, mic.airHz, 0.7f, mic.airDb - wall * 5.0f);
        channel.paHp.setHighPass(sampleRate_, pa.hp, 0.72f);
        channel.paBody.setPeak(sampleRate_, venue.bodyHz, 1.0f, venue.bodyDb);
        channel.paHorn.setPeak(sampleRate_, pa.presenceHz, 1.4f, pa.presenceDb);
        channel.paLp.setLowPass(sampleRate_, pa.airHz * (1.0f - wall * 0.58f), 0.72f);
        channel.feedbackBand.setBandPass(sampleRate_, clamp(p.feedbackFrequency, 180.0f, 6200.0f), clamp(p.feedbackQ, 1.0f, 42.0f));
        channel.feedbackTone.setLowPass(sampleRate_, 1300.0f + clamp(p.feedbackTone, 0.0f, 1.0f) * 9500.0f, 0.707f);
        channel.crowdBand.setBandPass(sampleRate_, 620.0f + p.crowdMood * 1050.0f, 0.65f);
        channel.crowdAir.setLowPass(sampleRate_, venue.dampingHz * (0.52f + 0.48f * p.crowdMood), 0.7f);
    }
}

float OpenMicProcessor::inputPeak(std::size_t channel) const noexcept { return inputPeak_[std::min(channel, maxChannels - 1)]; }
float OpenMicProcessor::outputPeak(std::size_t channel) const noexcept { return outputPeak_[std::min(channel, maxChannels - 1)]; }

void OpenMicProcessor::process(float* const* data,
                               std::size_t numChannels,
                               std::size_t numSamples,
                               const OpenMicParameters& p,
                               const float* const* externalAudienceAudio) noexcept
{
    const auto activeChannels = std::min({ numChannels, preparedChannels_, maxChannels });
    if (activeChannels == 0 || data == nullptr) return;
    updateFilters(p);
    inputPeak_.fill(0.0f); outputPeak_.fill(0.0f); safetyEngaged_ = false;

    const auto micShape = micShapes[static_cast<std::size_t>(std::clamp(static_cast<int>(p.mic), 0, 4))];
    const auto paShape = paShapes[static_cast<std::size_t>(std::clamp(static_cast<int>(p.pa), 0, 4))];
    const auto venue = venueShapes[static_cast<std::size_t>(std::clamp(static_cast<int>(p.venue), 0, 5))];
    const auto targetFeedback = p.feedbackArmed ? clamp(p.feedbackAmount, 0.0f, 1.0f) : 0.0f;
    const auto fbCoeff = coefficient(targetFeedback > feedbackEnvelope_ ? p.feedbackBuildMs : p.feedbackReleaseMs, sampleRate_);
    const auto feedbackDelaySamples = clamp(p.feedbackDelayMs * 0.001f * static_cast<float>(sampleRate_), 1.0f, static_cast<float>(channels_[0].feedbackDelay.size() - 2));
    const auto earlySamples = venue.earlyMs * 0.001f * static_cast<float>(sampleRate_);
    const auto lateSamples = venue.lateMs * 0.001f * static_cast<float>(sampleRate_);
    const auto mix = clamp(p.mix, 0.0f, 1.0f);
    const auto dryGain = std::cos(mix * pi * 0.5f);
    const auto wetGain = std::sin(mix * pi * 0.5f);
    const auto ceiling = clamp(p.ceiling, 0.25f, 0.99f);
    const auto limiterAttack = coefficient(2.2f, sampleRate_);
    const auto limiterRelease = coefficient(105.0f, sampleRate_);
    float activity = 0.0f, crowdActivity = 0.0f, roomActivity = 0.0f, limiterActivity = 0.0f;

    for (std::size_t i = 0; i < numSamples; ++i)
    {
        feedbackEnvelope_ = targetFeedback + fbCoeff * (feedbackEnvelope_ - targetFeedback);
        for (std::size_t c = 0; c < activeChannels; ++c)
        {
            auto& channel = channels_[c];
            const auto input = data[c][i];
            inputPeak_[c] = std::max(inputPeak_[c], std::abs(input));

            auto mic = channel.micHp.process(input * clamp(p.inputGain, 0.0f, 2.0f));
            mic = channel.micPresence.process(mic);
            mic = channel.micAir.process(mic);
            const auto proximityBass = input * clamp(p.proximity, 0.0f, 1.0f) * 0.22f;
            mic = softClip((mic + proximityBass) * (1.0f + p.micDrive * micShape.drive * 2.8f));

            auto pa = channel.paHp.process(mic);
            pa = channel.paBody.process(pa);
            pa = channel.paHorn.process(pa);
            pa = channel.paLp.process(pa);
            pa = softClip(pa * (1.0f + p.paDrive * paShape.drive * 3.2f));

            const auto delayed = readDelay(channel.feedbackDelay, channel.feedbackWrite, feedbackDelaySamples);
            auto feedback = channel.feedbackBand.process(delayed);
            feedback = channel.feedbackTone.process(feedback);
            const auto seed = randomSigned(random_[c]) * (0.000018f + p.electricalNoise * 0.00014f);
            const auto loopGain = feedbackEnvelope_ * (0.54f + 0.40f * p.monitorLevel);
            const auto loopInput = pa * p.monitorLevel * 0.20f + feedback * loopGain + (p.feedbackArmed ? seed : 0.0f);
            const auto boundedLoop = clamp(softClip(loopInput * 2.1f) * 0.58f, -0.62f, 0.62f);
            channel.feedbackDelay[channel.feedbackWrite] = boundedLoop;
            channel.feedbackWrite = (channel.feedbackWrite + 1u) % channel.feedbackDelay.size();
            activity = std::max(activity, std::abs(feedback) * feedbackEnvelope_);

            if (--channel.crowdCountdown <= 0)
            {
                channel.crowdTarget = clamp((randomSigned(random_[c]) * 0.5f + 0.5f) * (0.35f + p.crowdMood * 0.65f), 0.0f, 1.0f);
                channel.crowdCountdown = static_cast<int>(sampleRate_ * (0.035 + (randomSigned(random_[c]) * 0.5 + 0.5) * 0.18));
            }
            channel.crowdEnvelope += (channel.crowdTarget - channel.crowdEnvelope) * 0.0007f;
            const auto bedLevel = p.venueBedEnabled ? clamp(p.venueBedLevel, 0.0f, 1.0f) : 0.0f;
            const auto random = randomSigned(random_[c]);
            const auto hasExternalAudience = externalAudienceAudio != nullptr && externalAudienceAudio[c] != nullptr;
            auto crowd = hasExternalAudience ? externalAudienceAudio[c][i] : random;
            crowd = channel.crowdAir.process(channel.crowdBand.process(crowd));
            if (!hasExternalAudience)
                crowd *= channel.crowdEnvelope * clamp(p.crowdLevel,0.0f,1.0f) * 0.22f;
            float crowdEvent = 0.0f;
            if (crowdEventRemaining_ > 0 && !hasExternalAudience)
            {
                const auto progress = 1.0f - (float)crowdEventRemaining_ / (float)crowdEventTotal_;
                const auto envelope = std::min(1.0f, progress * 18.0f) * std::pow(std::max(0.0f, 1.0f - progress), .48f) * crowdEventStrength_;
                channel.crowdVoicePhase += (118.0f + 37.0f * (float)c + 54.0f * crowdEventStrength_) / (float)sampleRate_;
                if (channel.crowdVoicePhase >= 1.0f) channel.crowdVoicePhase -= 1.0f;
                if (crowdEventType_ == OpenMicCrowdEvent::applause)
                {
                    if (random > .90f - .055f * crowdEventStrength_) channel.crowdTransient = .35f + .65f * std::abs(random);
                    channel.crowdTransient *= .955f; crowdEvent = channel.crowdTransient * random;
                }
                else if (crowdEventType_ == OpenMicCrowdEvent::cheer)
                    crowdEvent = .48f * std::sin(channel.crowdVoicePhase * 2.0f * pi) + .36f * random;
                else if (crowdEventType_ == OpenMicCrowdEvent::heckle)
                    crowdEvent = (channel.crowdVoicePhase < .36f ? .55f : -.22f) + random * .24f;
                else crowdEvent = random * (.55f + .35f * std::sin(channel.crowdVoicePhase * 2.0f * pi));
                crowdEvent = channel.crowdAir.process(channel.crowdBand.process(crowdEvent)) * envelope * .34f;
            }

            channel.humPhase += (60.0f / static_cast<float>(sampleRate_)) * (c == 0 ? 1.0f : 1.0007f);
            if (channel.humPhase >= 1.0f) channel.humPhase -= 1.0f;
            const auto hum = std::sin(channel.humPhase * 2.0f * pi) * (0.0025f + p.electricalNoise * 0.035f) * bedLevel;
            const auto audience = crowd + crowdEvent;
            crowdActivity = std::max(crowdActivity, std::abs(audience) * 4.0f);
            const auto spill = audience + hum + (audience + hum) * p.stageBleed * 0.35f;
            const auto direct = pa + feedback * feedbackEnvelope_ * 0.72f + spill;

            const auto early = readDelay(channel.roomDelay, channel.roomWrite, earlySamples + static_cast<float>(c) * 3.1f);
            const auto late = readDelay(channel.roomDelay, channel.roomWrite, lateSamples - static_cast<float>(c) * 4.7f);
            const auto roomFeedback = clamp(0.18f + p.roomAmount * 0.46f, 0.0f, 0.68f);
            channel.roomDelay[channel.roomWrite] = clamp(direct + late * roomFeedback, -1.1f, 1.1f);
            channel.roomWrite = (channel.roomWrite + 1u) % channel.roomDelay.size();
            const auto room = (early * 0.56f + late * 0.34f) * p.roomAmount;
            roomActivity = std::max(roomActivity, std::abs(room) * 3.0f);
            auto wet = direct * (1.0f - p.roomAmount * 0.22f) + room;

            if (activeChannels == 2)
            {
                const auto cross = readDelay(channels_[1u - c].roomDelay, channels_[1u - c].roomWrite, lateSamples * 0.83f);
                wet += cross * p.roomAmount * p.stereoWidth * 0.18f;
            }

            auto out = (input * dryGain + wet * wetGain) * clamp(p.outputGain, 0.0f, 1.5f);
            const auto magnitude = std::abs(out);
            const auto envCoeff = magnitude > channel.limiterEnvelope ? limiterAttack : limiterRelease;
            channel.limiterEnvelope = magnitude + envCoeff * (channel.limiterEnvelope - magnitude);
            const auto reduction = channel.limiterEnvelope > ceiling ? ceiling / (channel.limiterEnvelope + 1.0e-8f) : 1.0f;
            const auto limited = clamp(out * reduction, -ceiling, ceiling);
            if (reduction < 0.999f) safetyEngaged_ = true;
            limiterActivity = std::max(limiterActivity, 1.0f - reduction);
            out += (limited - out) * clamp(p.limiterAmount, 0.0f, 1.0f);
            out = clamp(out, -1.0f, 1.0f);
            data[c][i] = out;
            outputPeak_[c] = std::max(outputPeak_[c], std::abs(out));
        }
        if (crowdEventRemaining_ > 0) --crowdEventRemaining_;
    }
    feedbackActivity_ = 0.88f * feedbackActivity_ + 0.12f * clamp(activity * 3.0f, 0.0f, 1.0f);
    crowdActivity_ = 0.86f * crowdActivity_ + 0.14f * clamp(crowdActivity, 0.0f, 1.0f);
    roomActivity_ = 0.86f * roomActivity_ + 0.14f * clamp(roomActivity, 0.0f, 1.0f);
    limiterActivity_ = 0.82f * limiterActivity_ + 0.18f * clamp(limiterActivity, 0.0f, 1.0f);
}
} // namespace lost_audio::core
