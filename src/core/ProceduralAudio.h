#pragma once

#include "AudioEngine.h"
#include <cmath>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <numeric>

namespace engine {
namespace core {

static constexpr float PA_PI      = 3.14159265358979323846f;
static constexpr float PA_TWO_PI  = PA_PI * 2.0f;
static constexpr float PA_SAMPLE_RATE = static_cast<float>(AUDIO_FREQUENCY);

namespace Wave {
    inline float sine(float phase)     { return std::sin(phase); }
    inline float square(float phase)   { return std::sin(phase) >= 0.0f ? 1.0f : -1.0f; }
    inline float saw(float phase)      { return 2.0f * (phase / PA_TWO_PI - std::floor(phase / PA_TWO_PI + 0.5f)); }
    inline float tri(float phase) {
        float t = phase / PA_TWO_PI;
        t = t - std::floor(t);
        return (t < 0.5f) ? (4.0f * t - 1.0f) : (3.0f - 4.0f * t);
    }
    inline float noise() {
        return (static_cast<float>(rand()) / RAND_MAX) * 2.0f - 1.0f;
    }
}

struct ADSR {
    float attack  = 0.01f;
    float decay   = 0.1f;
    float sustain = 0.7f;
    float release = 0.2f;
    float evaluate(float t, float totalDuration) const;
};

struct SynthParams {
    float  duration    = 0.3f;
    float  freqStart   = 440.0f;
    float  freqEnd     = 440.0f;
    float  amplitude   = 0.8f;
    ADSR   envelope;
    std::function<float(float)> waveform = Wave::sine;

    float  noiseAmount = 0.0f;
    float  vibratoRate = 5.0f;
    float  vibratoDept = 0.0f;
    float  distortion  = 0.0f;
    bool   stereoWidth = true;
};

namespace ProceduralAudio {
    struct LowPassFilter {
        float cutoff = 20000.0f;
        float prev   = 0.0f;
        float process(float in);
    };

    AudioBuffer synthesize(const SynthParams& p);

    AudioBuffer generateJump();
    AudioBuffer generateLand();
    AudioBuffer generateHit(float intensity = 0.5f);
    AudioBuffer generateShoot();
    AudioBuffer generatePickup();
    AudioBuffer generateExplosion(float radius = 1.0f);
    AudioBuffer generatePortal();
    AudioBuffer generateClick();
    AudioBuffer generateCoin();
}

inline bool registerProceduralSound(AudioEngine& engine, const std::string& id, AudioBuffer&& buffer) {
    if (!engine.isInitialized()) return false;
    auto clip   = std::make_shared<AudioClip>();
    clip->name  = id;
    clip->buffer= std::make_shared<AudioBuffer>(std::move(buffer));
    return engine.loadSoundFromBuffer(id, clip->buffer);
}

} // namespace core
} // namespace engine
