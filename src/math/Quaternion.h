#pragma once

#include "SimdConfig.h"
#include "Vector3D.h"
#include "MathUtils.h"
#include <cmath>
#include <string>

namespace engine {
namespace math {

// Forward declaration
class Matrix4x4;

/// Quaternion — Cuaternión unitario para rotaciones 3D (con SIMD SSE2).
///
/// Empaqueta {x, y, z, w} en un registro __m128 de 128 bits.
/// Representación: q = w + xi + yj + zk
/// Identidad: (0, 0, 0, 1) — No rota nada.
///
/// Ventajas sobre ángulos de Euler:
///   - Sin gimbal lock (bloqueo de cardán)
///   - Interpolación suave (slerp)
///   - Composición eficiente (multiplicación)
///   - Siempre representan rotaciones válidas (si normalizados)
///
class ENGINE_ALIGN Quaternion {
public:
    // ── Datos ──────────────────────────────────────────────────
#if ENGINE_SIMD_SSE2
    union {
        __m128 simd;     // Registro SSE2: [x, y, z, w]
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        struct { float x, y, z, w; };
#pragma GCC diagnostic pop
    };
#else
    float x, y, z, w;
#endif

    // ── Constructores ──────────────────────────────────────────

    /// Identidad: (0, 0, 0, 1) — sin rotación
    inline Quaternion()
#if ENGINE_SIMD_SSE2
        : simd(_mm_set_ps(1.0f, 0.0f, 0.0f, 0.0f)) {}
#else
        : x(0.0f), y(0.0f), z(0.0f), w(1.0f) {}
#endif

    /// Cuaternión con valores explícitos
    inline Quaternion(float x_, float y_, float z_, float w_)
#if ENGINE_SIMD_SSE2
        : simd(_mm_set_ps(w_, z_, y_, x_)) {}
#else
        : x(x_), y(y_), z(z_), w(w_) {}
#endif

#if ENGINE_SIMD_SSE2
    /// Constructor directo desde registro SSE
    inline Quaternion(__m128 v) : simd(v) {}
#endif

    // ── Constante ──────────────────────────────────────────────
    static const Quaternion Identity;

    // ── Factorías ──────────────────────────────────────────────

    /// Crear desde eje + ángulo (el eje debe estar normalizado)
    static inline Quaternion fromAxisAngle(const Vector3D& axis, float radians) {
        float halfAngle = radians * 0.5f;
        float s = std::sin(halfAngle);
        float c = std::cos(halfAngle);
        Vector3D a = axis.normalized();
        return Quaternion(a.x * s, a.y * s, a.z * s, c);
    }

    /// Crear desde ángulos de Euler (pitch=X, yaw=Y, roll=Z) en radianes.
    /// Orden de aplicación: Z(roll) → X(pitch) → Y(yaw) (intrínseco YXZ)
    /// Equivalente a Qy * Qx * Qz
    static inline Quaternion fromEuler(float pitch, float yaw, float roll) {
        // Build individual quaternions and compose
        Quaternion qx = fromAxisAngle(Vector3D::Right, pitch);
        Quaternion qy = fromAxisAngle(Vector3D::Up, yaw);
        Quaternion qz = fromAxisAngle(Vector3D(0,0,1), roll);
        return qy * qx * qz;  // YXZ order: Z first, then X, then Y
    }

    /// Crear desde ángulos de Euler (Vector3D: x=pitch, y=yaw, z=roll)
    static inline Quaternion fromEuler(const Vector3D& euler) {
        return fromEuler(euler.x, euler.y, euler.z);
    }

    /// Crear una rotación que mira en una dirección
    static Quaternion lookRotation(const Vector3D& forward, const Vector3D& up = Vector3D::Up);

    // ── Operaciones ────────────────────────────────────────────

    /// Multiplicación de cuaterniones (composición de rotaciones)
    /// q1 * q2: primero aplica q2, luego q1
    inline Quaternion operator*(const Quaternion& other) const {
#if ENGINE_SIMD_SSE2
        // SIMD Hamilton product using shuffle + multiply + add/sub
        // q1 = [x, y, z, w], q2 = [x, y, z, w]
        // Result.x = w*q2.x + x*q2.w + y*q2.z - z*q2.y
        // Result.y = w*q2.y - x*q2.z + y*q2.w + z*q2.x
        // Result.z = w*q2.z + x*q2.y - y*q2.x + z*q2.w
        // Result.w = w*q2.w - x*q2.x - y*q2.y - z*q2.z

        // Broadcast each component of 'this'
        __m128 xxxx = _mm_shuffle_ps(simd, simd, _MM_SHUFFLE(0,0,0,0));
        __m128 yyyy = _mm_shuffle_ps(simd, simd, _MM_SHUFFLE(1,1,1,1));
        __m128 zzzz = _mm_shuffle_ps(simd, simd, _MM_SHUFFLE(2,2,2,2));
        __m128 wwww = _mm_shuffle_ps(simd, simd, _MM_SHUFFLE(3,3,3,3));

        // _mm_set_ps(e3, e2, e1, e0) → positions [0]=e0, [1]=e1, [2]=e2, [3]=e3
        // Layout: simd[0]=x, simd[1]=y, simd[2]=z, simd[3]=w

        // w * [bx, by, bz, bw] — no shuffle needed
        __m128 r0 = _mm_mul_ps(wwww, other.simd);

        // x * [+bw, -bz, +by, -bx]
        // Shuffle b to [bw, bz, by, bx] → _MM_SHUFFLE(0,1,2,3)
        __m128 b_wzyx = _mm_shuffle_ps(other.simd, other.simd, _MM_SHUFFLE(0,1,2,3));
        // Signs at [0,1,2,3] = [+1, -1, +1, -1] → _mm_set_ps(-1, +1, -1, +1)
        static const __m128 sign1 = _mm_set_ps(-1.0f, 1.0f, -1.0f, 1.0f);
        __m128 r1 = _mm_mul_ps(xxxx, _mm_mul_ps(b_wzyx, sign1));

        // y * [+bz, +bw, -bx, -by]
        // Shuffle b to [bz, bw, bx, by] → _MM_SHUFFLE(1,0,3,2)
        __m128 b_zwxy = _mm_shuffle_ps(other.simd, other.simd, _MM_SHUFFLE(1,0,3,2));
        // Signs at [0,1,2,3] = [+1, +1, -1, -1] → _mm_set_ps(-1, -1, +1, +1)
        static const __m128 sign2 = _mm_set_ps(-1.0f, -1.0f, 1.0f, 1.0f);
        __m128 r2 = _mm_mul_ps(yyyy, _mm_mul_ps(b_zwxy, sign2));

        // z * [-by, +bx, +bw, -bz]
        // Shuffle b to [by, bx, bw, bz] → _MM_SHUFFLE(2,3,0,1)
        __m128 b_yxwz = _mm_shuffle_ps(other.simd, other.simd, _MM_SHUFFLE(2,3,0,1));
        // Signs at [0,1,2,3] = [-1, +1, +1, -1] → _mm_set_ps(-1, +1, +1, -1)
        static const __m128 sign3 = _mm_set_ps(-1.0f, 1.0f, 1.0f, -1.0f);
        __m128 r3 = _mm_mul_ps(zzzz, _mm_mul_ps(b_yxwz, sign3));

        return Quaternion(_mm_add_ps(_mm_add_ps(r0, r1), _mm_add_ps(r2, r3)));
#else
        // Scalar Hamilton product — 16 multiplies + 12 adds
        return Quaternion(
            w * other.x + x * other.w + y * other.z - z * other.y,
            w * other.y - x * other.z + y * other.w + z * other.x,
            w * other.z + x * other.y - y * other.x + z * other.w,
            w * other.w - x * other.x - y * other.y - z * other.z
        );
#endif
    }

    /// Conjugado: invierte la rotación (para cuaterniones unitarios = inversa)
    inline Quaternion conjugate() const {
#if ENGINE_SIMD_SSE2
        // Negate x, y, z but keep w: multiply by [-1, -1, -1, 1]
        static const __m128 conjugateMask = _mm_set_ps(1.0f, -1.0f, -1.0f, -1.0f);
        return Quaternion(_mm_mul_ps(simd, conjugateMask));
#else
        return Quaternion(-x, -y, -z, w);
#endif
    }

    /// Inversa: q⁻¹ = conjugado / |q|² (para unitarios, = conjugado)
    inline Quaternion inverse() const {
        float sqrLen = x*x + y*y + z*z + w*w;
        if (sqrLen < MathUtils::EPSILON) return Identity;
        float inv = 1.0f / sqrLen;
        return Quaternion(-x * inv, -y * inv, -z * inv, w * inv);
    }

    /// Magnitud (para cuaterniones unitarios debería ser ~1.0)
    inline float magnitude() const {
        return std::sqrt(sqrMagnitude());
    }

    /// Magnitud al cuadrado
    inline float sqrMagnitude() const {
#if ENGINE_SIMD_SSE2
        __m128 mul = _mm_mul_ps(simd, simd);
        // Horizontal sum of all 4 components
        __m128 shuf1 = _mm_shuffle_ps(mul, mul, _MM_SHUFFLE(2, 3, 0, 1));
        __m128 sums = _mm_add_ps(mul, shuf1);
        __m128 shuf2 = _mm_shuffle_ps(sums, sums, _MM_SHUFFLE(0, 1, 2, 3));
        __m128 result = _mm_add_ss(sums, shuf2);
        return _mm_cvtss_f32(result);
#else
        return x*x + y*y + z*z + w*w;
#endif
    }

    /// Normalizar (asegurar que es unitario)
    inline Quaternion normalized() const {
#if ENGINE_SIMD_SSE2
        float sqrMag = sqrMagnitude();
        if (sqrMag < MathUtils::EPSILON * MathUtils::EPSILON) return Identity;
        __m128 invMag = _mm_set1_ps(1.0f / std::sqrt(sqrMag));
        return Quaternion(_mm_mul_ps(simd, invMag));
#else
        float mag = magnitude();
        if (mag < MathUtils::EPSILON) return Identity;
        float inv = 1.0f / mag;
        return Quaternion(x * inv, y * inv, z * inv, w * inv);
#endif
    }

    /// Rotate a vector by this quaternion — alias de rotate() (mantener retrocompatibilidad)
    inline Vector3D rotateVector(const Vector3D& v) const {
        return rotate(v);
    }

    /// Scalar multiplication (named method — avoids operator ambiguity with Quaternion*Quaternion)
    inline Quaternion scale(float s) const {
        return Quaternion(x * s, y * s, z * s, w * s);
    }

    /// Scalar multiplication: float * quaternion
    friend inline Quaternion operator*(float s, const Quaternion& q) {
        return Quaternion(q.x * s, q.y * s, q.z * s, q.w * s);
    }

    /// Normalizar in-place
    inline void normalize() {
        float mag = magnitude();
        if (mag < MathUtils::EPSILON) { *this = Identity; return; }
        float inv = 1.0f / mag;
        x *= inv; y *= inv; z *= inv; w *= inv;
    }

    /// Producto escalar entre cuaterniones (útil para slerp)
    inline float dot(const Quaternion& other) const {
#if ENGINE_SIMD_SSE2
        __m128 mul = _mm_mul_ps(simd, other.simd);
        __m128 shuf1 = _mm_shuffle_ps(mul, mul, _MM_SHUFFLE(2, 3, 0, 1));
        __m128 sums = _mm_add_ps(mul, shuf1);
        __m128 shuf2 = _mm_shuffle_ps(sums, sums, _MM_SHUFFLE(0, 1, 2, 3));
        __m128 result = _mm_add_ss(sums, shuf2);
        return _mm_cvtss_f32(result);
#else
        return x * other.x + y * other.y + z * other.z + w * other.w;
#endif
    }

    // ── Rotación ───────────────────────────────────────────────

    /// Rotar un vector por este cuaternión: v' = q * v * q⁻¹
    /// Optimizado: evita crear cuaternión temporal del vector
    inline Vector3D rotate(const Vector3D& v) const {
        // t = 2 * (q.xyz × v)
        Vector3D qv(x, y, z);
        Vector3D t = qv.cross(v) * 2.0f;
        // v' = v + w*t + qv × t
        return v + t * w + qv.cross(t);
    }

    // ── Interpolación ──────────────────────────────────────────

    /// Interpolación esférica (Spherical Linear intERPolation)
    /// Produce la interpolación más suave y correcta entre rotaciones
    static inline Quaternion slerp(const Quaternion& a, const Quaternion& b, float t) {
        t = MathUtils::clamp(t, 0.0f, 1.0f);

        float cosTheta = a.dot(b);

        // Si el dot es negativo, invertimos uno para tomar el camino más corto
        Quaternion bAdj = b;
        if (cosTheta < 0.0f) {
            bAdj = Quaternion(-b.x, -b.y, -b.z, -b.w);
            cosTheta = -cosTheta;
        }

        // Si los cuaterniones son casi iguales, usar nlerp para evitar div/0
        if (cosTheta > 0.9995f) {
            return nlerp(a, bAdj, t);
        }

        float theta = std::acos(cosTheta);
        float sinTheta = std::sin(theta);
        float wa = std::sin((1.0f - t) * theta) / sinTheta;
        float wb = std::sin(t * theta) / sinTheta;

        return Quaternion(
            wa * a.x + wb * bAdj.x,
            wa * a.y + wb * bAdj.y,
            wa * a.z + wb * bAdj.z,
            wa * a.w + wb * bAdj.w
        );
    }

    /// Interpolación lineal normalizada (más rápida que slerp, menos precisa)
    static inline Quaternion nlerp(const Quaternion& a, const Quaternion& b, float t) {
        t = MathUtils::clamp(t, 0.0f, 1.0f);
        return Quaternion(
            a.x + t * (b.x - a.x),
            a.y + t * (b.y - a.y),
            a.z + t * (b.z - a.z),
            a.w + t * (b.w - a.w)
        ).normalized();
    }

    // ── Conversión ─────────────────────────────────────────────

    /// Convertir a Matrix4x4 de rotación
    Matrix4x4 toMatrix() const;

    /// Convertir a ángulos de Euler (retorna Vector3D con x=pitch, y=yaw, z=roll)
    /// Extrae ángulos usando la convención YXZ intrínseca (matching fromEuler)
    inline Vector3D toEuler() const {
        // Compute rotation matrix elements from quaternion
        float xx = x * x, yy = y * y, zz = z * z;
        float xy = x * y, xz = x * z, yz = y * z;
        float wx = w * x, wy = w * y, wz = w * z;

        // Rotation matrix R (from quaternion):
        // R00 = 1-2(yy+zz)   R01 = 2(xy-wz)     R02 = 2(xz+wy)
        // R10 = 2(xy+wz)     R11 = 1-2(xx+zz)    R12 = 2(yz-wx)
        // R20 = 2(xz-wy)     R21 = 2(yz+wx)      R22 = 1-2(xx+yy)

        // YXZ intrinsic: R = Ry * Rx * Rz
        // Extract pitch (X) from R12 = -sin(pitch)
        float r12 = 2.0f * (yz - wx);
        float r22 = 1.0f - 2.0f * (xx + yy);
        float r10 = 2.0f * (xy + wz);
        float r00 = 1.0f - 2.0f * (yy + zz);
        float r11 = 1.0f - 2.0f * (xx + zz);

        Vector3D euler;

        // Pitch (X rotation): sin(pitch) = -R12
        float sinPitch = -(r12);
        if (MathUtils::abs(sinPitch) >= 0.9999f) {
            // Gimbal lock
            euler.x = std::copysign(MathUtils::HALF_PI, sinPitch);
            euler.y = std::atan2(-2.0f * (xz - wy), r00);
            euler.z = 0.0f;
        } else {
            euler.x = std::asin(sinPitch);
            // Yaw (Y rotation)
            float r02 = 2.0f * (xz + wy);
            euler.y = std::atan2(r02, r22);
            // Roll (Z rotation)
            euler.z = std::atan2(r10, r11);
        }

        return euler;
    }

    // ── Comparación ────────────────────────────────────────────
    inline bool operator==(const Quaternion& other) const {
        return MathUtils::approxEqual(x, other.x) &&
               MathUtils::approxEqual(y, other.y) &&
               MathUtils::approxEqual(z, other.z) &&
               MathUtils::approxEqual(w, other.w);
    }

    inline bool operator!=(const Quaternion& other) const {
        return !(*this == other);
    }

    // ── Advanced Construction ──────────────────────────────────

    /// Shortest rotation from one direction to another
    static inline Quaternion fromToRotation(const Vector3D& from, const Vector3D& to) {
        Vector3D f = from.normalized();
        Vector3D t = to.normalized();
        float d = f.dot(t);

        if (d >= 0.9999f) return Quaternion();  // Identity

        if (d <= -0.9999f) {
            // 180° rotation — find orthogonal axis
            Vector3D axis = Vector3D(1, 0, 0).cross(f);
            if (axis.sqrMagnitude() < 0.0001f)
                axis = Vector3D(0, 1, 0).cross(f);
            axis = axis.normalized();
            return Quaternion(axis.x, axis.y, axis.z, 0.0f);  // 180° around axis
        }

        Vector3D half = (f + t).normalized();
        Vector3D c = f.cross(half);
        return Quaternion(c.x, c.y, c.z, f.dot(half)).normalized();
    }

    /// Angle between two quaternions (in radians)
    static inline float angle(const Quaternion& a, const Quaternion& b) {
        float d = a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
        return std::acos(MathUtils::clamp(MathUtils::abs(d), 0.0f, 1.0f)) * 2.0f;
    }

    // ── Debug ──────────────────────────────────────────────────
    std::string toString() const;
};

} // namespace math
} // namespace engine
