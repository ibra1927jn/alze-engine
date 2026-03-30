#pragma once

#include "SimdConfig.h"
#include "MathUtils.h"
#include <string>

namespace engine {
namespace math {

/// Vector2D — Corazón matemático del motor (con SIMD SSE2).
///
/// Internamente empaqueta {x, y} en un registro __m128 de 128 bits.
/// Las operaciones aritméticas (+, -, *, /) se ejecutan en paralelo
/// usando instrucciones SIMD, procesando ambos componentes a la vez.
///
/// API pública IDÉNTICA — el código de juego no cambia,
/// pero las operaciones son significativamente más rápidas.
///
class ENGINE_ALIGN Vector2D {
public:
    // ── Datos ──────────────────────────────────────────────────
#if ENGINE_SIMD_SSE2
    union {
        __m128 simd;     // Registro SSE2: [x, y, 0, 0]
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        struct { float x, y; };
#pragma GCC diagnostic pop
    };
#else
    float x;
    float y;
#endif

    // ── Constructores ──────────────────────────────────────────

    inline Vector2D()
#if ENGINE_SIMD_SSE2
        : simd(_mm_setzero_ps()) {}
#else
        : x(0.0f), y(0.0f) {}
#endif

    inline Vector2D(float x_, float y_)
#if ENGINE_SIMD_SSE2
        : simd(_mm_set_ps(0.0f, 0.0f, y_, x_)) {}
#else
        : x(x_), y(y_) {}
#endif

#if ENGINE_SIMD_SSE2
    /// Constructor directo desde registro SSE
    inline Vector2D(__m128 v) : simd(v) {}
#endif

    // ── Constantes estáticas ───────────────────────────────────
    static const Vector2D Zero;
    static const Vector2D One;
    static const Vector2D Up;
    static const Vector2D Down;
    static const Vector2D Left;
    static const Vector2D Right;

    // ── Operadores aritméticos (SIMD) ──────────────────────────

    inline Vector2D operator+(const Vector2D& other) const {
#if ENGINE_SIMD_SSE2
        return Vector2D(_mm_add_ps(simd, other.simd));
#else
        return Vector2D(x + other.x, y + other.y);
#endif
    }

    inline Vector2D operator-(const Vector2D& other) const {
#if ENGINE_SIMD_SSE2
        return Vector2D(_mm_sub_ps(simd, other.simd));
#else
        return Vector2D(x - other.x, y - other.y);
#endif
    }

    inline Vector2D operator*(float scalar) const {
#if ENGINE_SIMD_SSE2
        return Vector2D(_mm_mul_ps(simd, _mm_set1_ps(scalar)));
#else
        return Vector2D(x * scalar, y * scalar);
#endif
    }

    inline Vector2D operator/(float scalar) const {
#if ENGINE_SIMD_SSE2
        return Vector2D(_mm_div_ps(simd, _mm_set1_ps(scalar)));
#else
        return Vector2D(x / scalar, y / scalar);
#endif
    }

    inline Vector2D& operator+=(const Vector2D& other) {
#if ENGINE_SIMD_SSE2
        simd = _mm_add_ps(simd, other.simd);
#else
        x += other.x; y += other.y;
#endif
        return *this;
    }

    inline Vector2D& operator-=(const Vector2D& other) {
#if ENGINE_SIMD_SSE2
        simd = _mm_sub_ps(simd, other.simd);
#else
        x -= other.x; y -= other.y;
#endif
        return *this;
    }

    inline Vector2D& operator*=(float scalar) {
#if ENGINE_SIMD_SSE2
        simd = _mm_mul_ps(simd, _mm_set1_ps(scalar));
#else
        x *= scalar; y *= scalar;
#endif
        return *this;
    }

    inline Vector2D& operator/=(float scalar) {
#if ENGINE_SIMD_SSE2
        simd = _mm_div_ps(simd, _mm_set1_ps(scalar));
#else
        x /= scalar; y /= scalar;
#endif
        return *this;
    }

    inline Vector2D operator-() const {
#if ENGINE_SIMD_SSE2
        return Vector2D(_mm_sub_ps(_mm_setzero_ps(), simd));
#else
        return Vector2D(-x, -y);
#endif
    }

    // ── Comparación ────────────────────────────────────────────
    inline bool operator==(const Vector2D& other) const {
        return MathUtils::approxEqual(x, other.x) && MathUtils::approxEqual(y, other.y);
    }

    inline bool operator!=(const Vector2D& other) const {
        return !(*this == other);
    }

    // ── Operaciones vectoriales ────────────────────────────────

    /// Producto escalar (SIMD: multiply + horizontal add)
    inline float dot(const Vector2D& other) const {
#if ENGINE_SIMD_SSE2
        __m128 mul = _mm_mul_ps(simd, other.simd);
        // Sumar x*ox + y*oy: shuffle y sumar
        __m128 shuf = _mm_shuffle_ps(mul, mul, _MM_SHUFFLE(1, 1, 1, 1));
        __m128 sum = _mm_add_ss(mul, shuf);
        return _mm_cvtss_f32(sum);
#else
        return x * other.x + y * other.y;
#endif
    }

    /// Producto cruzado 2D (escalar)
    inline float cross(const Vector2D& other) const {
        return x * other.y - y * other.x;
    }

    /// Magnitud (longitud) del vector
    inline float magnitude() const {
        return std::sqrt(sqrMagnitude());
    }

    /// Magnitud al cuadrado (evita sqrt)
    inline float sqrMagnitude() const {
        return dot(*this);
    }

    /// Vector normalizado (longitud = 1)
    inline Vector2D normalized() const {
        float mag = magnitude();
        if (mag < MathUtils::EPSILON) return Zero;
        return *this / mag;
    }

    /// Normalizar in-place
    inline void normalize() {
        float mag = magnitude();
        if (mag < MathUtils::EPSILON) return;
        *this /= mag;
    }

    /// Distancia entre dos puntos
    static inline float distance(const Vector2D& a, const Vector2D& b) {
        return (b - a).magnitude();
    }

    /// Ángulo del vector en radianes
    inline float angle() const {
        return std::atan2(y, x);
    }

    /// Ángulo entre dos vectores
    static inline float angleBetween(const Vector2D& a, const Vector2D& b) {
        float dot = a.normalized().dot(b.normalized());
        dot = MathUtils::clamp(dot, -1.0f, 1.0f);
        return std::acos(dot);
    }

    // ── Nivel 2: Geometría avanzada (inline) ───────────────────

    /// Perpendicular (90° antihorario): (x, y) → (-y, x)
    inline Vector2D perpendicular() const {
        return Vector2D(-y, x);
    }

    /// Reflejo contra una normal: r = v - 2·(v·n)·n
    inline Vector2D reflect(const Vector2D& normal) const {
        return *this - normal * (2.0f * dot(normal));
    }

    /// Proyección sobre otro vector: proj = (a·b / b·b) * b
    inline Vector2D project(const Vector2D& onto) const {
        float ontoSqr = onto.sqrMagnitude();
        if (ontoSqr < MathUtils::EPSILON) return Zero;
        return onto * (dot(onto) / ontoSqr);
    }

    /// Rotación por radianes (sin crear matriz)
    inline Vector2D rotated(float radians) const {
        float c = std::cos(radians);
        float s = std::sin(radians);
        return Vector2D(x * c - y * s, x * s + y * c);
    }

    /// Interpolación lineal
    static inline Vector2D lerp(const Vector2D& a, const Vector2D& b, float t) {
        t = MathUtils::clamp(t, 0.0f, 1.0f);
#if ENGINE_SIMD_SSE2
        __m128 vt = _mm_set1_ps(t);
        __m128 diff = _mm_sub_ps(b.simd, a.simd);
        return Vector2D(_mm_add_ps(a.simd, _mm_mul_ps(diff, vt)));
#else
        return Vector2D(
            a.x + t * (b.x - a.x),
            a.y + t * (b.y - a.y)
        );
#endif
    }

    // ── Multiplicación escalar por la izquierda ────────────────
    friend inline Vector2D operator*(float scalar, const Vector2D& vec) {
        return vec * scalar;
    }

    // ── Debug ──────────────────────────────────────────────────
    std::string toString() const;
};

} // namespace math
} // namespace engine
