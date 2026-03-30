#pragma once

#include <SDL.h>
#include <SDL_mixer.h>
#include <cmath>
#include <cstring>
#include <vector>
#include "Logger.h"

namespace engine {
namespace core {

/// SoundGenerator — Genera sonidos procedurales en memoria.
///
/// No necesita archivos WAV. Crea ondas matemáticas (seno, ruido)
/// directamente en buffers SDL_mixer. Thread-safe: los chunks
/// se generan en el hilo principal y se reproducen via SDL_mixer
/// que opera en su propio hilo de audio.
///
class SoundGenerator {
public:
    static constexpr int SAMPLE_RATE = 44100;
    static constexpr int CHANNELS = 1;
    static constexpr Uint16 FORMAT = AUDIO_S16SYS;

    /// Generar sonido de salto (seno ascendente 200→800Hz, 80ms)
    static Mix_Chunk* generateJump() {
        const float duration = 0.08f;
        const int samples = static_cast<int>(SAMPLE_RATE * duration);
        std::vector<int16_t> buffer(samples);

        for (int i = 0; i < samples; i++) {
            float t = static_cast<float>(i) / SAMPLE_RATE;
            float progress = static_cast<float>(i) / samples;
            float freq = 200.0f + 600.0f * progress;  // Sweep up
            float envelope = 1.0f - progress;          // Fade out
            float sample = std::sin(2.0f * 3.14159f * freq * t) * envelope;
            buffer[i] = static_cast<int16_t>(sample * 8000);
        }

        return createChunk(buffer);
    }

    /// Generar sonido de aterrizaje (tono bajo 80Hz, 60ms, decay rápido)
    static Mix_Chunk* generateLand() {
        const float duration = 0.06f;
        const int samples = static_cast<int>(SAMPLE_RATE * duration);
        std::vector<int16_t> buffer(samples);

        for (int i = 0; i < samples; i++) {
            float t = static_cast<float>(i) / SAMPLE_RATE;
            float progress = static_cast<float>(i) / samples;
            float envelope = (1.0f - progress) * (1.0f - progress);  // Cuadrático
            float sample = std::sin(2.0f * 3.14159f * 80.0f * t) * envelope;
            // Añadir un poco de ruido para textura
            float noise = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 2.0f - 1.0f) * 0.3f;
            buffer[i] = static_cast<int16_t>((sample + noise * envelope) * 10000);
        }

        return createChunk(buffer);
    }

    /// Generar sonido de colisión (ruido filtrado, duración proporcional al impulso)
    static Mix_Chunk* generateHit(float intensity = 0.5f) {
        float duration = 0.03f + intensity * 0.05f;
        const int samples = static_cast<int>(SAMPLE_RATE * duration);
        std::vector<int16_t> buffer(samples);

        float freq = 150.0f + intensity * 200.0f;
        for (int i = 0; i < samples; i++) {
            float t = static_cast<float>(i) / SAMPLE_RATE;
            float progress = static_cast<float>(i) / samples;
            float envelope = (1.0f - progress);
            float tone = std::sin(2.0f * 3.14159f * freq * t) * 0.6f;
            float noise = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 2.0f - 1.0f) * 0.4f;
            buffer[i] = static_cast<int16_t>((tone + noise) * envelope * 6000 * intensity);
        }

        return createChunk(buffer);
    }

    /// Generar sonido de paso/movimiento (click suave)
    static Mix_Chunk* generateStep() {
        const float duration = 0.025f;
        const int samples = static_cast<int>(SAMPLE_RATE * duration);
        std::vector<int16_t> buffer(samples);

        for (int i = 0; i < samples; i++) {
            float progress = static_cast<float>(i) / samples;
            float envelope = (1.0f - progress) * (1.0f - progress);
            float sample = std::sin(2.0f * 3.14159f * 400.0f * progress * 0.02f) * envelope;
            buffer[i] = static_cast<int16_t>(sample * 3000);
        }

        return createChunk(buffer);
    }

private:
    static Mix_Chunk* createChunk(const std::vector<int16_t>& buffer) {
        size_t bytes = buffer.size() * sizeof(int16_t);
        Uint8* data = static_cast<Uint8*>(SDL_malloc(bytes));
        if (!data) return nullptr;
        std::memcpy(data, buffer.data(), bytes);

        Mix_Chunk* chunk = Mix_QuickLoad_RAW(data, static_cast<Uint32>(bytes));
        if (chunk) {
            chunk->allocated = 1;  // SDL_mixer will free the data
        }
        return chunk;
    }
};

} // namespace core
} // namespace engine
