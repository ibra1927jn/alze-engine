#pragma once

#include <cmath>

namespace engine {
namespace math {

/// Utilidades matemáticas globales del motor.
/// Funciones puras y constantes — todo inline para máximo rendimiento.
namespace MathUtils {

    // ── Constantes ─────────────────────────────────────────────
    constexpr float PI      = 3.14159265358979323846f;
    constexpr float TWO_PI  = PI * 2.0f;
    constexpr float HALF_PI = PI * 0.5f;
    constexpr float EPSILON = 1e-6f;

    // ── Conversiones angulares ─────────────────────────────────

    /// Grados → Radianes
    inline float degToRad(float degrees) {
        return degrees * (PI / 180.0f);
    }

    /// Radianes → Grados
    inline float radToDeg(float radians) {
        return radians * (180.0f / PI);
    }

    // ── Interpolación y límites ────────────────────────────────

    /// Interpolación lineal: a + t*(b - a)
    inline float lerp(float a, float b, float t) {
        return a + t * (b - a);
    }

    /// Restringe un valor entre min y max
    inline float clamp(float value, float min, float max) {
        if (value < min) return min;
        if (value > max) return max;
        return value;
    }

    /// Mapea un valor de un rango a otro
    inline float remap(float value, float fromMin, float fromMax, float toMin, float toMax) {
        float t = (value - fromMin) / (fromMax - fromMin);
        return lerp(toMin, toMax, t);
    }

    // ── Comparaciones ──────────────────────────────────────────

    /// Comparación con tolerancia (evita problemas de punto flotante)
    inline bool approxEqual(float a, float b, float epsilon = EPSILON) {
        return std::fabs(a - b) < epsilon;
    }

    // ── Utilidades ─────────────────────────────────────────────

    /// Devuelve el signo: -1, 0, o 1
    inline float sign(float value) {
        if (value > EPSILON) return 1.0f;
        if (value < -EPSILON) return -1.0f;
        return 0.0f;
    }

    /// Devuelve el mínimo de dos valores
    inline float min(float a, float b) {
        return (a < b) ? a : b;
    }

    /// Devuelve el máximo de dos valores
    inline float max(float a, float b) {
        return (a > b) ? a : b;
    }

    /// Valor absoluto
    inline float abs(float value) {
        return std::fabs(value);
    }

} // namespace MathUtils

} // namespace math
} // namespace engine
