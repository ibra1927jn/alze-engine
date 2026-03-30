#pragma once

#include "SimdConfig.h"
#include "Vector3D.h"
#include "MathUtils.h"
#include <cmath>
#include <string>

namespace engine {
namespace math {

// Forward declaration
class Quaternion;

/// Matrix4x4 — Matriz 4x4 column-major para transformaciones 3D (con SIMD SSE2).
///
/// Column-major: compatible directamente con OpenGL (glUniformMatrix4fv).
/// Internamente almacena 4 columnas como 4 registros __m128.
///
/// Convención de multiplicación: M * v transforma el vector v.
/// Composición: (A * B) * v = A * (B * v) — B se aplica primero.
///
/// Layout en memoria (column-major):
///   col[0] = {m00, m10, m20, m30}  ← primera columna
///   col[1] = {m01, m11, m21, m31}  ← segunda columna
///   col[2] = {m02, m12, m22, m32}  ← tercera columna
///   col[3] = {m03, m13, m23, m33}  ← cuarta columna
///
class ENGINE_ALIGN Matrix4x4 {
public:
#if ENGINE_SIMD_SSE2
    __m128 col[4];  // 4 columnas como registros SSE2
#else
    float m[16];    // Column-major: m[col*4 + row]
#endif

    // ── Acceso ─────────────────────────────────────────────────

    /// Acceso por fila y columna (row, col)
    inline float get(int row, int c) const {
#if ENGINE_SIMD_SSE2
        alignas(16) float tmp[4];
        _mm_store_ps(tmp, col[c]);
        return tmp[row];
#else
        return m[c * 4 + row];
#endif
    }

    /// Establecer valor por fila y columna
    inline void set(int row, int c, float v) {
#if ENGINE_SIMD_SSE2
        alignas(16) float tmp[4];
        _mm_store_ps(tmp, col[c]);
        tmp[row] = v;
        col[c] = _mm_load_ps(tmp);
#else
        m[c * 4 + row] = v;
#endif
    }

    /// Puntero raw a los 16 floats (column-major, para OpenGL)
    inline const float* data() const {
#if ENGINE_SIMD_SSE2
        return reinterpret_cast<const float*>(&col[0]);
#else
        return m;
#endif
    }

    // ── Constructores ──────────────────────────────────────────

    /// Matriz cero
    Matrix4x4() {
#if ENGINE_SIMD_SSE2
        col[0] = _mm_setzero_ps();
        col[1] = _mm_setzero_ps();
        col[2] = _mm_setzero_ps();
        col[3] = _mm_setzero_ps();
#else
        for (int i = 0; i < 16; i++) m[i] = 0.0f;
#endif
    }

    // ── Factorías estáticas ────────────────────────────────────

    /// Matriz identidad
    static inline Matrix4x4 identity() {
        Matrix4x4 mat;
#if ENGINE_SIMD_SSE2
        mat.col[0] = _mm_set_ps(0, 0, 0, 1);
        mat.col[1] = _mm_set_ps(0, 0, 1, 0);
        mat.col[2] = _mm_set_ps(0, 1, 0, 0);
        mat.col[3] = _mm_set_ps(1, 0, 0, 0);
#else
        mat.m[0] = 1; mat.m[5] = 1; mat.m[10] = 1; mat.m[15] = 1;
#endif
        return mat;
    }

    /// Matriz de traslación
    static inline Matrix4x4 translation(float tx, float ty, float tz) {
        Matrix4x4 mat = identity();
#if ENGINE_SIMD_SSE2
        mat.col[3] = _mm_set_ps(1, tz, ty, tx);
#else
        mat.m[12] = tx; mat.m[13] = ty; mat.m[14] = tz;
#endif
        return mat;
    }

    static inline Matrix4x4 translation(const Vector3D& t) {
        return translation(t.x, t.y, t.z);
    }

    /// Matriz de escala
    static inline Matrix4x4 scale(float sx, float sy, float sz) {
        Matrix4x4 mat;
#if ENGINE_SIMD_SSE2
        mat.col[0] = _mm_set_ps(0, 0, 0, sx);
        mat.col[1] = _mm_set_ps(0, 0, sy, 0);
        mat.col[2] = _mm_set_ps(0, sz, 0, 0);
        mat.col[3] = _mm_set_ps(1, 0, 0, 0);
#else
        mat.m[0] = sx; mat.m[5] = sy; mat.m[10] = sz; mat.m[15] = 1;
#endif
        return mat;
    }

    static inline Matrix4x4 scale(const Vector3D& s) {
        return scale(s.x, s.y, s.z);
    }

    static inline Matrix4x4 scale(float uniform) {
        return scale(uniform, uniform, uniform);
    }

    /// Matriz de rotación alrededor de un eje arbitrario (Rodrigues)
    static inline Matrix4x4 rotation(const Vector3D& axis, float radians) {
        Vector3D a = axis.normalized();
        float c = std::cos(radians);
        float s = std::sin(radians);
        float t = 1.0f - c;

        // Standard rotation matrix (row, col):
        //   R00 = t*ax*ax+c      R01 = t*ax*ay-s*az   R02 = t*ax*az+s*ay
        //   R10 = t*ax*ay+s*az   R11 = t*ay*ay+c      R12 = t*ay*az-s*ax
        //   R20 = t*ax*az-s*ay   R21 = t*ay*az+s*ax   R22 = t*az*az+c

        Matrix4x4 mat;
        // Column-major: col[c] = {R0c, R1c, R2c, R3c}
        // _mm_set_ps order: (element3, element2, element1, element0)
        //                 = (R3c,      R2c,      R1c,      R0c)
#if ENGINE_SIMD_SSE2
        mat.col[0] = _mm_set_ps(0,
            t * a.x * a.z - s * a.y,    // R20
            t * a.x * a.y + s * a.z,    // R10
            t * a.x * a.x + c);         // R00
        mat.col[1] = _mm_set_ps(0,
            t * a.y * a.z + s * a.x,    // R21
            t * a.y * a.y + c,          // R11
            t * a.x * a.y - s * a.z);   // R01
        mat.col[2] = _mm_set_ps(0,
            t * a.z * a.z + c,          // R22
            t * a.y * a.z - s * a.x,    // R12
            t * a.x * a.z + s * a.y);   // R02
        mat.col[3] = _mm_set_ps(1, 0, 0, 0);
#else
        // Column-major: m[col*4 + row]
        mat.m[0]  = t * a.x * a.x + c;       // col0, row0 = R00
        mat.m[1]  = t * a.x * a.y + s * a.z; // col0, row1 = R10
        mat.m[2]  = t * a.x * a.z - s * a.y; // col0, row2 = R20
        mat.m[4]  = t * a.x * a.y - s * a.z; // col1, row0 = R01
        mat.m[5]  = t * a.y * a.y + c;       // col1, row1 = R11
        mat.m[6]  = t * a.y * a.z + s * a.x; // col1, row2 = R21
        mat.m[8]  = t * a.x * a.z + s * a.y; // col2, row0 = R02
        mat.m[9]  = t * a.y * a.z - s * a.x; // col2, row1 = R12
        mat.m[10] = t * a.z * a.z + c;       // col2, row2 = R22
        mat.m[15] = 1;
#endif
        return mat;
    }

    /// Rotación alrededor del eje X
    static inline Matrix4x4 rotationX(float radians) {
        return rotation(Vector3D::Right, radians);
    }

    /// Rotación alrededor del eje Y
    static inline Matrix4x4 rotationY(float radians) {
        return rotation(Vector3D::Up, radians);
    }

    /// Rotación alrededor del eje Z
    static inline Matrix4x4 rotationZ(float radians) {
        return rotation(Vector3D(0, 0, 1), radians);
    }

    /// Construir desde un Quaternion (declarada, implementada en Quaternion.cpp)
    static Matrix4x4 fromQuaternion(const Quaternion& q);

    // ── Proyección ─────────────────────────────────────────────

    /// Matriz de proyección perspectiva (OpenGL convention: NDC [-1,1])
    /// @param fovY  Campo de visión vertical en radianes
    /// @param aspect Ratio ancho/alto
    /// @param near   Plano cercano (> 0)
    /// @param far    Plano lejano (> near)
    static inline Matrix4x4 perspective(float fovY, float aspect, float near, float far) {
        float tanHalf = std::tan(fovY * 0.5f);
        Matrix4x4 mat;

        mat.set(0, 0, 1.0f / (aspect * tanHalf));
        mat.set(1, 1, 1.0f / tanHalf);
        mat.set(2, 2, -(far + near) / (far - near));
        mat.set(3, 2, -1.0f);
        mat.set(2, 3, -(2.0f * far * near) / (far - near));
        // mat[3][3] queda 0 (proyección perspectiva)

        return mat;
    }

    /// Matriz de proyección ortográfica
    static inline Matrix4x4 orthographic(float left, float right,
                                          float bottom, float top,
                                          float near, float far) {
        Matrix4x4 mat;

        mat.set(0, 0, 2.0f / (right - left));
        mat.set(1, 1, 2.0f / (top - bottom));
        mat.set(2, 2, -2.0f / (far - near));
        mat.set(0, 3, -(right + left) / (right - left));
        mat.set(1, 3, -(top + bottom) / (top - bottom));
        mat.set(2, 3, -(far + near) / (far - near));
        mat.set(3, 3, 1.0f);

        return mat;
    }

    // ── Cámara ─────────────────────────────────────────────────

    /// Matriz de vista (View Matrix) — posiciona la cámara en el mundo
    static inline Matrix4x4 lookAt(const Vector3D& eye, const Vector3D& target, const Vector3D& worldUp) {
        Vector3D f = (target - eye).normalized();  // Forward
        Vector3D r = f.cross(worldUp).normalized(); // Right
        Vector3D u = r.cross(f);                    // Up

        Matrix4x4 mat = identity();
        // Fila 0 (spread across columns)
        mat.set(0, 0,  r.x);
        mat.set(0, 1,  r.y);
        mat.set(0, 2,  r.z);
        // Fila 1
        mat.set(1, 0,  u.x);
        mat.set(1, 1,  u.y);
        mat.set(1, 2,  u.z);
        // Fila 2 (negated forward — OpenGL looks down -Z)
        mat.set(2, 0, -f.x);
        mat.set(2, 1, -f.y);
        mat.set(2, 2, -f.z);
        // Translation
        mat.set(0, 3, -r.dot(eye));
        mat.set(1, 3, -u.dot(eye));
        mat.set(2, 3,  f.dot(eye));

        return mat;
    }

    // ── Operaciones ────────────────────────────────────────────

    /// Multiplicación de matrices (SIMD: broadcast + multiply-add por columna)
    inline Matrix4x4 operator*(const Matrix4x4& other) const {
        Matrix4x4 result;
#if ENGINE_SIMD_SSE2
        for (int i = 0; i < 4; i++) {
            __m128 x = _mm_shuffle_ps(other.col[i], other.col[i], _MM_SHUFFLE(0,0,0,0));
            __m128 y = _mm_shuffle_ps(other.col[i], other.col[i], _MM_SHUFFLE(1,1,1,1));
            __m128 z = _mm_shuffle_ps(other.col[i], other.col[i], _MM_SHUFFLE(2,2,2,2));
            __m128 w = _mm_shuffle_ps(other.col[i], other.col[i], _MM_SHUFFLE(3,3,3,3));

            result.col[i] = _mm_add_ps(
                _mm_add_ps(_mm_mul_ps(x, col[0]), _mm_mul_ps(y, col[1])),
                _mm_add_ps(_mm_mul_ps(z, col[2]), _mm_mul_ps(w, col[3]))
            );
        }
#else
        for (int c = 0; c < 4; c++) {
            for (int r = 0; r < 4; r++) {
                float sum = 0;
                for (int k = 0; k < 4; k++) {
                    sum += get(r, k) * other.get(k, c);
                }
                result.set(r, c, sum);
            }
        }
#endif
        return result;
    }

    /// Transformar un punto 3D (w=1, aplica traslación)
    inline Vector3D transformPoint(const Vector3D& point) const {
#if ENGINE_SIMD_SSE2
        __m128 x = _mm_set1_ps(point.x);
        __m128 y = _mm_set1_ps(point.y);
        __m128 z = _mm_set1_ps(point.z);
        __m128 result = _mm_add_ps(
            _mm_add_ps(_mm_mul_ps(x, col[0]), _mm_mul_ps(y, col[1])),
            _mm_add_ps(_mm_mul_ps(z, col[2]), col[3])  // w=1 → + col[3]
        );
        return Vector3D(result);
#else
        return Vector3D(
            get(0,0)*point.x + get(0,1)*point.y + get(0,2)*point.z + get(0,3),
            get(1,0)*point.x + get(1,1)*point.y + get(1,2)*point.z + get(1,3),
            get(2,0)*point.x + get(2,1)*point.y + get(2,2)*point.z + get(2,3)
        );
#endif
    }

    /// Transformar un vector 3D (w=0, ignora traslación)
    inline Vector3D transformVector(const Vector3D& vec) const {
#if ENGINE_SIMD_SSE2
        __m128 x = _mm_set1_ps(vec.x);
        __m128 y = _mm_set1_ps(vec.y);
        __m128 z = _mm_set1_ps(vec.z);
        __m128 result = _mm_add_ps(
            _mm_add_ps(_mm_mul_ps(x, col[0]), _mm_mul_ps(y, col[1])),
            _mm_mul_ps(z, col[2])  // w=0 → NO suma col[3]
        );
        return Vector3D(result);
#else
        return Vector3D(
            get(0,0)*vec.x + get(0,1)*vec.y + get(0,2)*vec.z,
            get(1,0)*vec.x + get(1,1)*vec.y + get(1,2)*vec.z,
            get(2,0)*vec.x + get(2,1)*vec.y + get(2,2)*vec.z
        );
#endif
    }

    /// Transpuesta (SIMD: uses _MM_TRANSPOSE4_PS macros)
    inline Matrix4x4 transposed() const {
#if ENGINE_SIMD_SSE2
        Matrix4x4 t;
        t.col[0] = col[0];
        t.col[1] = col[1];
        t.col[2] = col[2];
        t.col[3] = col[3];
        _MM_TRANSPOSE4_PS(t.col[0], t.col[1], t.col[2], t.col[3]);
        return t;
#else
        Matrix4x4 t;
        for (int r = 0; r < 4; r++)
            for (int c = 0; c < 4; c++)
                t.set(r, c, get(c, r));
        return t;
#endif
    }

    /// Determinante (expansión por cofactores de la primera fila)
    float determinant() const;

    /// Inversa (adjunta / determinante). Devuelve identidad si singular.
    Matrix4x4 inverse() const;

    /// Comparación con tolerancia
    inline bool operator==(const Matrix4x4& other) const {
        for (int c = 0; c < 4; c++)
            for (int r = 0; r < 4; r++)
                if (!MathUtils::approxEqual(get(r, c), other.get(r, c)))
                    return false;
        return true;
    }

    inline bool operator!=(const Matrix4x4& other) const {
        return !(*this == other);
    }

    // ── Decomposition Helpers ──────────────────────────────────

    /// Extract translation from column 3
    inline Vector3D extractTranslation() const {
        return Vector3D(get(0,3), get(1,3), get(2,3));
    }

    /// Extract scale (lengths of the 3 basis columns)
    inline Vector3D extractScale() const {
        float sx = Vector3D(get(0,0), get(1,0), get(2,0)).magnitude();
        float sy = Vector3D(get(0,1), get(1,1), get(2,1)).magnitude();
        float sz = Vector3D(get(0,2), get(1,2), get(2,2)).magnitude();
        return Vector3D(sx, sy, sz);
    }

    // ── Debug ──────────────────────────────────────────────────
    std::string toString() const;
};

} // namespace math
} // namespace engine
