#include "ProceduralAudio.h"

namespace engine {
namespace core {

float ADSR::evaluate(float t, float totalDuration) const {
    float sustainStart = attack + decay;
    float releaseStart = totalDuration - release;

    if (t < attack)
        return t / attack;
    if (t < sustainStart)
        return 1.0f - (1.0f - sustain) * ((t - attack) / decay);
    if (t < releaseStart)
        return sustain;
    float rt = (t - releaseStart) / release;
    return sustain * (1.0f - std::min(rt, 1.0f));
}

namespace ProceduralAudio {

    float LowPassFilter::process(float in) {
        float rc = 1.0f / (PA_TWO_PI * cutoff);
        float dt = 1.0f / PA_SAMPLE_RATE;
        float alpha = dt / (rc + dt);
        prev = prev + alpha * (in - prev);
        return prev;
    }

    AudioBuffer synthesize(const SynthParams& p) {
        int numFrames = static_cast<int>(p.duration * PA_SAMPLE_RATE);
        AudioBuffer buf;
        buf.sampleRate  = AUDIO_FREQUENCY;
        buf.numChannels = 2;
        buf.samples.resize(numFrames * 2);

        float phase    = 0.0f;
        LowPassFilter lpfL, lpfR;
        lpfL.cutoff = lpfR.cutoff = 8000.0f;

        for (int i = 0; i < numFrames; i++) {
            float t = static_cast<float>(i) / PA_SAMPLE_RATE;

            float freqRatio = (p.freqEnd != p.freqStart) ? (t / p.duration) : 0.0f;
            float freq      = p.freqStart + (p.freqEnd - p.freqStart) * freqRatio;

            if (p.vibratoDepth > 0.0f) {
                float semitones = p.vibratoDepth * std::sin(PA_TWO_PI * p.vibratoRate * t);
                freq *= std::pow(2.0f, semitones / 12.0f);
            }

            phase += PA_TWO_PI * freq / PA_SAMPLE_RATE;
            if (phase > PA_TWO_PI * 100.0f) phase -= PA_TWO_PI * 100.0f;

            float sample = p.waveform(phase);

            if (p.noiseAmount > 0.0f)
                sample = sample * (1.0f - p.noiseAmount) + Wave::noise() * p.noiseAmount;

            if (p.distortion > 0.0f) {
                float drive = 1.0f + p.distortion * 10.0f;
                sample = std::tanh(sample * drive) / std::tanh(drive);
            }

            float env    = p.envelope.evaluate(t, p.duration);
            sample      *= env * p.amplitude;

            float sL = sample;
            float sR = sample;
            if (p.stereoWidth) {
                int   delay = static_cast<int>(0.0005f * PA_SAMPLE_RATE);
                if (i >= delay) {
                    int prevIdx = (i - delay) * 2;
                    sR = sample * 0.85f + buf.samples[prevIdx] / 32767.0f * 0.15f;
                }
            }

            sL = lpfL.process(sL);
            sR = lpfR.process(sR);

            buf.samples[i * 2]     = static_cast<int16_t>(std::max(-1.0f, std::min(1.0f, sL)) * 32767.0f);
            buf.samples[i * 2 + 1] = static_cast<int16_t>(std::max(-1.0f, std::min(1.0f, sR)) * 32767.0f);
        }

        return buf;
    }

    AudioBuffer generateJump() {
        SynthParams p;
        p.duration   = 0.22f;
        p.freqStart  = 240.0f;
        p.freqEnd    = 520.0f;
        p.amplitude  = 0.65f;
        p.noiseAmount= 0.08f;
        p.waveform   = Wave::sine;
        p.envelope   = {0.005f, 0.05f, 0.4f, 0.12f};
        return synthesize(p);
    }

    AudioBuffer generateLand() {
        SynthParams p;
        p.duration   = 0.18f;
        p.freqStart  = 120.0f;
        p.freqEnd    = 60.0f;
        p.amplitude  = 0.75f;
        p.noiseAmount= 0.4f;
        p.distortion = 0.15f;
        p.waveform   = Wave::sine;
        p.envelope   = {0.002f, 0.04f, 0.2f, 0.12f};
        return synthesize(p);
    }

    AudioBuffer generateHit(float intensity) {
        SynthParams p;
        p.duration   = 0.12f + intensity * 0.1f;
        p.freqStart  = 180.0f + intensity * 200.0f;
        p.freqEnd    = 80.0f;
        p.amplitude  = 0.5f + intensity * 0.4f;
        p.noiseAmount= 0.35f;
        p.distortion = intensity * 0.3f;
        p.waveform   = Wave::square;
        p.envelope   = {0.001f, 0.03f + intensity * 0.05f, 0.1f, 0.08f};
        return synthesize(p);
    }

    AudioBuffer generateShoot() {
        SynthParams p;
        p.duration   = 0.25f;
        p.freqStart  = 600.0f;
        p.freqEnd    = 120.0f;
        p.amplitude  = 0.55f;
        p.noiseAmount= 0.05f;
        p.distortion = 0.2f;
        p.waveform   = Wave::saw;
        p.envelope   = {0.003f, 0.08f, 0.1f, 0.1f};
        return synthesize(p);
    }

    AudioBuffer generatePickup() {
        SynthParams p1;
        p1.duration   = 0.1f;
        p1.freqStart  = 523.25f;
        p1.freqEnd    = 523.25f;
        p1.amplitude  = 0.5f;
        p1.waveform   = Wave::sine;
        p1.envelope   = {0.01f, 0.02f, 0.7f, 0.04f};
        auto buf1 = synthesize(p1);

        SynthParams p2;
        p2.duration   = 0.15f;
        p2.freqStart  = 783.99f;
        p2.freqEnd    = 783.99f;
        p2.amplitude  = 0.5f;
        p2.waveform   = Wave::sine;
        p2.envelope   = {0.01f, 0.03f, 0.6f, 0.06f};
        auto buf2 = synthesize(p2);

        AudioBuffer out;
        out.sampleRate  = AUDIO_FREQUENCY;
        out.numChannels = 2;
        out.samples.reserve(buf1.samples.size() + buf2.samples.size());
        out.samples.insert(out.samples.end(), buf1.samples.begin(), buf1.samples.end());
        out.samples.insert(out.samples.end(), buf2.samples.begin(), buf2.samples.end());
        return out;
    }

    AudioBuffer generateExplosion(float radius) {
        SynthParams p;
        p.duration   = 0.6f + radius * 0.3f;
        p.freqStart  = 200.0f;
        p.freqEnd    = 30.0f;
        p.amplitude  = std::min(1.0f, 0.7f + radius * 0.15f);
        p.noiseAmount= 0.75f;
        p.distortion = 0.5f + radius * 0.2f;
        p.waveform   = Wave::sine;
        p.envelope   = {0.002f, 0.1f, 0.3f, 0.4f + radius * 0.1f};
        return synthesize(p);
    }

    AudioBuffer generatePortal() {
        SynthParams p;
        p.duration    = 1.0f;
        p.freqStart   = 320.0f;
        p.freqEnd     = 320.0f;
        p.amplitude   = 0.4f;
        p.vibratoDepth = 0.3f;
        p.waveform    = Wave::sine;
        p.envelope    = {0.2f, 0.1f, 0.6f, 0.4f};
        return synthesize(p);
    }

    AudioBuffer generateClick() {
        SynthParams p;
        p.duration   = 0.06f;
        p.freqStart  = 800.0f;
        p.freqEnd    = 600.0f;
        p.amplitude  = 0.35f;
        p.noiseAmount= 0.1f;
        p.waveform   = Wave::sine;
        p.envelope   = {0.002f, 0.01f, 0.3f, 0.03f};
        p.stereoWidth= false;
        return synthesize(p);
    }

    AudioBuffer generateCoin() {
        SynthParams p1; p1.duration=0.08f; p1.freqStart=987.77f; p1.freqEnd=987.77f;
        p1.amplitude=0.45f; p1.waveform=Wave::sine;
        p1.envelope={0.005f, 0.02f, 0.6f, 0.03f};

        SynthParams p2; p2.duration=0.1f;  p2.freqStart=1318.51f; p2.freqEnd=1318.51f;
        p2.amplitude=0.45f; p2.waveform=Wave::sine;
        p2.envelope={0.005f, 0.02f, 0.6f, 0.04f};

        auto b1 = synthesize(p1);
        auto b2 = synthesize(p2);
        AudioBuffer out; out.sampleRate=AUDIO_FREQUENCY; out.numChannels=2;
        out.samples.reserve(b1.samples.size() + b2.samples.size());
        out.samples.insert(out.samples.end(), b1.samples.begin(), b1.samples.end());
        out.samples.insert(out.samples.end(), b2.samples.begin(), b2.samples.end());
        return out;
    }

} // namespace ProceduralAudio
} // namespace core
} // namespace engine
