#pragma once

#include <SDL.h>

namespace engine {
namespace core {

/// Timer — Abstracción del tiempo de alta precisión.
///
/// Envuelve SDL_GetPerformanceCounter para no depender de SDL
/// directamente en el resto del motor.
///
/// Uso:
///   Timer timer;
///   timer.reset();
///   // ... trabajo ...
///   float elapsed = timer.elapsed();        // Segundos
///   float elapsedMs = timer.elapsedMs();    // Milisegundos
///
class Timer {
public:
    Timer() : m_frequency(SDL_GetPerformanceFrequency()) {
        reset();
    }

    /// Resetear el timer al momento actual
    void reset() {
        m_start = SDL_GetPerformanceCounter();
    }

    /// Tiempo transcurrido en segundos (alta precisión)
    float elapsed() const {
        Uint64 now = SDL_GetPerformanceCounter();
        return static_cast<float>(now - m_start) / static_cast<float>(m_frequency);
    }

    /// Tiempo transcurrido en milisegundos
    float elapsedMs() const {
        return elapsed() * 1000.0f;
    }

    /// Devuelve el tiempo transcurrido y resetea (lap/split)
    float lap() {
        float dt = elapsed();
        reset();
        return dt;
    }

    /// Frecuencia del timer del sistema (ticks por segundo)
    Uint64 getFrequency() const { return m_frequency; }

    /// Tick actual del sistema
    static Uint64 now() { return SDL_GetPerformanceCounter(); }

private:
    Uint64 m_start;
    Uint64 m_frequency;
};

} // namespace core
} // namespace engine
