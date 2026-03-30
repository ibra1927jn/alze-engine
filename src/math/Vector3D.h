#pragma once

#include "SimdConfig.h"
#include "MathUtils.h"
#include <cmath>
#include <string>

namespace engine {
namespace math {

// Forward declaration
class Vector2D;

/// Vector3D — Vector tridimensional con SIMD SSE2.
///
/// Internamente empaqueta {x, y, z, 0} en un registro __m128 de 128 bits.
/// Las operaciones aritméticas se ejecutan en paralelo usando instrucciones
/// SIMD, procesando los tres componentes a la vez.
///
/// Diseñado para ser el corazón de las matemáticas 3D del motor:
/// posiciones, direcciones, velocidades, normales, escalas.
///
class ENGINE_ALIGN Vector3D {
public:
    // ── Datos ──────────────────────────────────────────────────
#if ENGINE_SIMD_SSE2
    union {
        __m128 simd;     // Registro SSE2: [x, y, z, 0]
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        struct { float x, y, z, _pad; };
#pragma GCC diagnostic pop
    };
#else
    float x;
    float y;
    float z;
#endif

    // ── Constructores ──────────────────────────────────────────

    inline Vector3D()
#if ENGINE_SIMD_SSE2
        : simd(_mm_setzero_ps()) {}
#else
        : x(0.0f), y(0.0f), z(0.0f) {}
#endif

    inline Vector3D(float x_, float y_, float z_)
#if ENGINE_SIMD_SSE2
        : simd(_mm_set_ps(0.0f, z_, y_, x_)) {}
#else
        : x(x_), y(y_), z(z_) {}
#endif

#if ENGINE_SIMD_SSE2
    /// Constructor directo desde registro SSE
    inline Vector3D(__m128 v) : simd(v) {}
#endif

    /// Constructor desde Vector2D + componente Z
    inline Vector3D(const Vector2D& v2, float z_);

    // ── Constantes estáticas ───────────────────────────────────
    static const Vector3D Zero;
    static const Vector3D One;
    static const Vector3D Up;        // (0, 1, 0) — Y arriba (convención OpenGL)
    static const Vector3D Down;      // (0,-1, 0)
    static const Vector3D Right;     // (1, 0, 0)
    static const Vector3D Left;      // (-1,0, 0)
    static const Vector3D Forward;   // (0, 0,-1) — -Z es "adelante" en OpenGL
    static const Vector3D Back;      // (0, 0, 1)

    // ── Operadores aritméticos (SIMD) ──────────────────────────

    inline Vector3D operator+(const Vector3D& other) const {
#if ENGINE_SIMD_SSE2
        return Vector3D(_mm_add_ps(simd, other.simd));
#else
        return Vector3D(x + other.x, y + other.y, z + other.z);
#endif
    }

    inline Vector3D operator-(const Vector3D& other) const {
#if ENGINE_SIMD_SSE2
        return Vector3D(_mm_sub_ps(simd, other.simd));
#else
        return Vector3D(x - other.x, y - other.y, z - other.z);
#endif
    }

    inline Vector3D operator*(float scalar) const {
#if ENGINE_SIMD_SSE2
        return Vector3D(_mm_mul_ps(simd, _mm_set1_ps(scalar)));
#else
        return Vector3D(x * scalar, y * scalar, z * scalar);
#endif
    }

    inline Vector3D operator/(float scalar) const {
#if ENGINE_SIMD_SSE2
        return Vector3D(_mm_div_ps(simd, _mm_set1_ps(scalar)));
#else
        return Vector3D(x / scalar, y / scalar, z / scalar);
#endif
    }

    /// Multiplicación componente a componente (Hadamard product)
    inline Vector3D operator*(const Vector3D& other) const {
#if ENGINE_SIMD_SSE2
        return Vector3D(_mm_mul_ps(simd, other.simd));
#else
        return Vector3D(x * other.x, y * other.y, z * other.z);
#endif
    }

    inline Vector3D& operator+=(const Vector3D& other) {
#if ENGINE_SIMD_SSE2
        simd = _mm_add_ps(simd, other.simd);
#else
        x += other.x; y += other.y; z += other.z;
#endif
        return *this;
    }

    inline Vector3D& operator-=(const Vector3D& other) {
#if ENGINE_SIMD_SSE2
        simd = _mm_sub_ps(simd, other.simd);
#else
        x -= other.x; y -= other.y; z -= other.z;
#endif
        return *this;
    }

    inline Vector3D& operator*=(float scalar) {
#if ENGINE_SIMD_SSE2
        simd = _mm_mul_ps(simd, _mm_set1_ps(scalar));
#else
        x *= scalar; y *= scalar; z *= scalar;
#endif
        return *this;
    }

    inline Vector3D& operator/=(float scalar) {
#if ENGINE_SIMD_SSE2
        simd = _mm_div_ps(simd, _mm_set1_ps(scalar));
#else
        x /= scalar; y /= scalar; z /= scalar;
#endif
        return *this;
    }

    inline Vector3D operator-() const {
#if ENGINE_SIMD_SSE2
        return Vector3D(_mm_sub_ps(_mm_setzero_ps(), simd));
#else
        return Vector3D(-x, -y, -z);
#endif
    }

    // ── Comparación ────────────────────────────────────────────
    inline bool operator==(const Vector3D& other) const {
        return MathUtils::approxEqual(x, other.x) &&
               MathUtils::approxEqual(y, other.y) &&
               MathUtils::approxEqual(z, other.z);
    }

    inline bool operator!=(const Vector3D& other) const {
        return !(*this == other);
    }

    // ── Operaciones vectoriales ────────────────────────────────

    /// Producto escalar (SIMD: multiply + horizontal add)
    inline float dot(const Vector3D& other) const {
#if ENGINE_SIMD_SSE2
        __m128 mul = _mm_mul_ps(simd, other.simd);
        // x*ox + y*oy + z*oz
        // Shuffle: [y*oy, y*oy, y*oy, y*oy]
        __m128 shuf1 = _mm_shuffle_ps(mul, mul, _MM_SHUFFLE(1, 1, 1, 1));
        // Shuffle: [z*oz, z*oz, z*oz, z*oz]
        __m128 shuf2 = _mm_shuffle_ps(mul, mul, _MM_SHUFFLE(2, 2, 2, 2));
        __m128 sum = _mm_add_ss(_mm_add_ss(mul, shuf1), shuf2);
        return _mm_cvtss_f32(sum);
#else
        return x * other.x + y * other.y + z * other.z;
#endif
    }

    /// Producto cruzado 3D: a × b
    /// Devuelve un vector perpendicular a ambos (regla de la mano derecha)
    inline Vector3D cross(const Vector3D& other) const {
#if ENGINE_SIMD_SSE2
        // a × b = (a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x)
        __m128 a_yzx = _mm_shuffle_ps(simd, simd, _MM_SHUFFLE(3, 0, 2, 1));
        __m128 b_yzx = _mm_shuffle_ps(other.simd, other.simd, _MM_SHUFFLE(3, 0, 2, 1));
        __m128 c = _mm_sub_ps(
            _mm_mul_ps(simd, b_yzx),
            _mm_mul_ps(a_yzx, other.simd)
        );
        return Vector3D(_mm_shuffle_ps(c, c, _MM_SHUFFLE(3, 0, 2, 1)));
#else
        return Vector3D(
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        );
#endif
    }

    /// Magnitud (longitud) del vector
    inline float magnitude() const {
        return std::sqrt(sqrMagnitude());
    }

    /// Magnitud al cuadrado (evita sqrt)
    inline float sqrMagnitude() const {
        return dot(*this);
    }

    /// Aliases (standard naming used by glm, Bullet, etc.)
    inline float length() const { return magnitude(); }
    inline float lengthSquared() const { return sqrMagnitude(); }

    /// Vector normalizado (longitud = 1)
    inline Vector3D normalized() const {
#if ENGINE_SIMD_SSE2
        float sqrMag = sqrMagnitude();
        if (sqrMag < MathUtils::EPSILON * MathUtils::EPSILON) return Zero;
        // Use rsqrt with one Newton-Raphson refinement for ~23-bit precision
        __m128 sqrMagV = _mm_set1_ps(sqrMag);
        __m128 rsqrt = _mm_rsqrt_ss(sqrMagV);
        // NR step: rsqrt = rsqrt * (1.5 - 0.5 * sqrMag * rsqrt * rsqrt)
        __m128 half = _mm_set_ss(0.5f);
        __m128 three_half = _mm_set_ss(1.5f);
        __m128 muls = _mm_mul_ss(_mm_mul_ss(half, sqrMagV), _mm_mul_ss(rsqrt, rsqrt));
        rsqrt = _mm_mul_ss(rsqrt, _mm_sub_ss(three_half, muls));
        __m128 invMag = _mm_shuffle_ps(rsqrt, rsqrt, 0);
        return Vector3D(_mm_mul_ps(simd, invMag));
#else
        float mag = magnitude();
        if (mag < MathUtils::EPSILON) return Zero;
        return *this / mag;
#endif
    }

    /// Normalizar in-place
    inline void normalize() {
        float mag = magnitude();
        if (mag < MathUtils::EPSILON) return;
        *this /= mag;
    }

    /// Distancia entre dos puntos
    static inline float distance(const Vector3D& a, const Vector3D& b) {
        return (b - a).magnitude();
    }

    /// Distancia al cuadrado entre dos puntos (evita sqrt)
    static inline float sqrDistance(const Vector3D& a, const Vector3D& b) {
        return (b - a).sqrMagnitude();
    }

    /// Ángulo entre dos vectores (en radianes)
    static inline float angleBetween(const Vector3D& a, const Vector3D& b) {
        float d = a.normalized().dot(b.normalized());
        d = MathUtils::clamp(d, -1.0f, 1.0f);
        return std::acos(d);
    }

    // ── Geometría avanzada ─────────────────────────────────────

    /// Reflejo contra una normal: r = v - 2·(v·n)·n
    inline Vector3D reflect(const Vector3D& normal) const {
        return *this - normal * (2.0f * dot(normal));
    }

    /// Proyección sobre otro vector: proj = (a·b / b·b) * b
    inline Vector3D project(const Vector3D& onto) const {
        float ontoSqr = onto.sqrMagnitude();
        if (ontoSqr < MathUtils::EPSILON) return Zero;
        return onto * (dot(onto) / ontoSqr);
    }

    /// Interpolación lineal
    static inline Vector3D lerp(const Vector3D& a, const Vector3D& b, float t) {
        t = MathUtils::clamp(t, 0.0f, 1.0f);
#if ENGINE_SIMD_SSE2
        __m128 vt = _mm_set1_ps(t);
        __m128 diff = _mm_sub_ps(b.simd, a.simd);
        return Vector3D(_mm_add_ps(a.simd, _mm_mul_ps(diff, vt)));
#else
        return Vector3D(
            a.x + t * (b.x - a.x),
            a.y + t * (b.y - a.y),
            a.z + t * (b.z - a.z)
        );
#endif
    }

    /// Componente mínimo entre dos vectores (per-component min)
    static inline Vector3D min(const Vector3D& a, const Vector3D& b) {
#if ENGINE_SIMD_SSE2
        return Vector3D(_mm_min_ps(a.simd, b.simd));
#else
        return Vector3D(
            MathUtils::min(a.x, b.x),
            MathUtils::min(a.y, b.y),
            MathUtils::min(a.z, b.z)
        );
#endif
    }

    /// Componente máximo entre dos vectores (per-component max)
    static inline Vector3D max(const Vector3D& a, const Vector3D& b) {
#if ENGINE_SIMD_SSE2
        return Vector3D(_mm_max_ps(a.simd, b.simd));
#else
        return Vector3D(
            MathUtils::max(a.x, b.x),
            MathUtils::max(a.y, b.y),
            MathUtils::max(a.z, b.z)
        );
#endif
    }

    /// Absolute value per-component
    inline Vector3D abs() const {
#if ENGINE_SIMD_SSE2
        // Clear sign bit on all components
        __m128 mask = _mm_castsi128_ps(_mm_set1_epi32(0x7FFFFFFF));
        return Vector3D(_mm_and_ps(simd, mask));
#else
        return Vector3D(std::abs(x), std::abs(y), std::abs(z));
#endif
    }

    /// Refraction (Snell's law): eta = n1/n2 (index of refraction ratio)
    inline Vector3D refract(const Vector3D& normal, float eta) const {
        float d = dot(normal);
        float k = 1.0f - eta * eta * (1.0f - d * d);
        if (k < 0.0f) return Zero;  // Total internal reflection
        return *this * eta - normal * (eta * d + std::sqrt(k));
    }

    /// Unclamped lerp (allows extrapolation)
    static inline Vector3D lerpUnclamped(const Vector3D& a, const Vector3D& b, float t) {
#if ENGINE_SIMD_SSE2
        __m128 vt = _mm_set1_ps(t);
        __m128 diff = _mm_sub_ps(b.simd, a.simd);
        return Vector3D(_mm_add_ps(a.simd, _mm_mul_ps(diff, vt)));
#else
        return Vector3D(
            a.x + t * (b.x - a.x),
            a.y + t * (b.y - a.y),
            a.z + t * (b.z - a.z)
        );
#endif
    }

    /// Per-component clamp between min and max vectors
    static inline Vector3D clamp(const Vector3D& v, const Vector3D& mn, const Vector3D& mx) {
#if ENGINE_SIMD_SSE2
        return Vector3D(_mm_min_ps(_mm_max_ps(v.simd, mn.simd), mx.simd));
#else
        return Vector3D(
            MathUtils::clamp(v.x, mn.x, mx.x),
            MathUtils::clamp(v.y, mn.y, mx.y),
            MathUtils::clamp(v.z, mn.z, mx.z)
        );
#endif
    }

    /// Move towards target by maxDelta
    static inline Vector3D moveTowards(const Vector3D& current, const Vector3D& target, float maxDelta) {
        Vector3D diff = target - current;
        float dist = diff.magnitude();
        if (dist <= maxDelta || dist < MathUtils::EPSILON)
            return target;
        return current + diff * (maxDelta / dist);
    }

    // ── Multiplicación escalar por la izquierda ────────────────
    friend inline Vector3D operator*(float scalar, const Vector3D& vec) {
        return vec * scalar;
    }

    // ── Debug ──────────────────────────────────────────────────
    std::string toString() const;
};

} // namespace math
} // namespace engine
