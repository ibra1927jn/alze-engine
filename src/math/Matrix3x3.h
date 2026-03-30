#pragma once

#include "SimdConfig.h"
#include "Vector2D.h"
#include <cmath>
#include <string>

namespace engine {
namespace math {

/// Matriz 3x3 para transformaciones 2D (con SIMD SSE2).
///
/// Internamente almacena 3 filas como __m128 (4 floats), donde
/// el cuarto float de cada fila es padding. Las multiplicaciones
/// se hacen con instrucciones SIMD en paralelo.
///
class ENGINE_ALIGN Matrix3x3 {
public:
#if ENGINE_SIMD_SSE2
    // 3 filas como registros SSE (el 4to float es 0)
    __m128 row[3];

    // Acceso escalar
    float get(int r, int c) const {
        alignas(16) float tmp[4];
        _mm_store_ps(tmp, row[r]);
        return tmp[c];
    }
    void set(int r, int c, float v) {
        alignas(16) float tmp[4];
        _mm_store_ps(tmp, row[r]);
        tmp[c] = v;
        row[r] = _mm_load_ps(tmp);
    }
#else
    float m[3][3];
    float get(int r, int c) const { return m[r][c]; }
    void set(int r, int c, float v) { m[r][c] = v; }
#endif

    // ── Constructores ──────────────────────────────────────────
    Matrix3x3() {
#if ENGINE_SIMD_SSE2
        row[0] = _mm_setzero_ps();
        row[1] = _mm_setzero_ps();
        row[2] = _mm_setzero_ps();
#else
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
                m[i][j] = 0.0f;
#endif
    }

    // ── Factorías estáticas ────────────────────────────────────

    static inline Matrix3x3 identity() {
        Matrix3x3 mat;
#if ENGINE_SIMD_SSE2
        mat.row[0] = _mm_set_ps(0, 0, 0, 1);
        mat.row[1] = _mm_set_ps(0, 0, 1, 0);
        mat.row[2] = _mm_set_ps(0, 1, 0, 0);
#else
        mat.m[0][0] = 1; mat.m[1][1] = 1; mat.m[2][2] = 1;
#endif
        return mat;
    }

    static inline Matrix3x3 translation(float tx, float ty) {
        Matrix3x3 mat = identity();
#if ENGINE_SIMD_SSE2
        mat.row[0] = _mm_set_ps(0, tx, 0, 1);
        mat.row[1] = _mm_set_ps(0, ty, 1, 0);
#else
        mat.m[0][2] = tx;
        mat.m[1][2] = ty;
#endif
        return mat;
    }

    static inline Matrix3x3 translation(const Vector2D& t) {
        return translation(t.x, t.y);
    }

    static inline Matrix3x3 rotation(float radians) {
        float c = std::cos(radians);
        float s = std::sin(radians);
        Matrix3x3 mat;
#if ENGINE_SIMD_SSE2
        mat.row[0] = _mm_set_ps(0, 0, -s, c);
        mat.row[1] = _mm_set_ps(0, 0,  c, s);
        mat.row[2] = _mm_set_ps(0, 1,  0, 0);
#else
        mat.m[0][0] = c;  mat.m[0][1] = -s;
        mat.m[1][0] = s;  mat.m[1][1] = c;
        mat.m[2][2] = 1;
#endif
        return mat;
    }

    static inline Matrix3x3 scale(float sx, float sy) {
        Matrix3x3 mat;
#if ENGINE_SIMD_SSE2
        mat.row[0] = _mm_set_ps(0, 0, 0, sx);
        mat.row[1] = _mm_set_ps(0, 0, sy, 0);
        mat.row[2] = _mm_set_ps(0, 1, 0, 0);
#else
        mat.m[0][0] = sx;
        mat.m[1][1] = sy;
        mat.m[2][2] = 1;
#endif
        return mat;
    }

    static inline Matrix3x3 scale(const Vector2D& s) { return scale(s.x, s.y); }
    static inline Matrix3x3 scale(float uniform) { return scale(uniform, uniform); }

    // ── Operaciones (SIMD) ─────────────────────────────────────

    /// Multiplicación de matrices con SIMD
    inline Matrix3x3 operator*(const Matrix3x3& other) const {
        Matrix3x3 result;
#if ENGINE_SIMD_SSE2
        for (int i = 0; i < 3; i++) {
            // Broadcast cada elemento de la fila i y multiplicar con columnas
            __m128 x = _mm_shuffle_ps(row[i], row[i], _MM_SHUFFLE(0,0,0,0));
            __m128 y = _mm_shuffle_ps(row[i], row[i], _MM_SHUFFLE(1,1,1,1));
            __m128 z = _mm_shuffle_ps(row[i], row[i], _MM_SHUFFLE(2,2,2,2));

            result.row[i] = _mm_add_ps(
                _mm_add_ps(_mm_mul_ps(x, other.row[0]), _mm_mul_ps(y, other.row[1])),
                _mm_mul_ps(z, other.row[2])
            );
        }
#else
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++) {
                result.m[i][j] = 0;
                for (int k = 0; k < 3; k++)
                    result.m[i][j] += m[i][k] * other.m[k][j];
            }
#endif
        return result;
    }

    /// Transformar un punto 2D (coordenadas homogéneas: w=1)
    inline Vector2D transformPoint(const Vector2D& point) const {
#if ENGINE_SIMD_SSE2
        // 2D transform uses scalar for correctness (only 2 useful results)
        float x = point.x * get(0,0) + point.y * get(0,1) + get(0,2);
        float y = point.x * get(1,0) + point.y * get(1,1) + get(1,2);
        return Vector2D(x, y);
#else
        return Vector2D(
            point.x * m[0][0] + point.y * m[0][1] + m[0][2],
            point.x * m[1][0] + point.y * m[1][1] + m[1][2]
        );
#endif
    }

    /// Transformar un vector 2D (ignora traslación)
    inline Vector2D transformVector(const Vector2D& vec) const {
        float x = vec.x * get(0,0) + vec.y * get(0,1);
        float y = vec.x * get(1,0) + vec.y * get(1,1);
        return Vector2D(x, y);
    }

    /// Transpuesta
    inline Matrix3x3 transposed() const {
        Matrix3x3 t;
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
                t.set(i, j, get(j, i));
        return t;
    }

    /// Determinante
    inline float determinant() const {
        return get(0,0) * (get(1,1)*get(2,2) - get(1,2)*get(2,1))
             - get(0,1) * (get(1,0)*get(2,2) - get(1,2)*get(2,0))
             + get(0,2) * (get(1,0)*get(2,1) - get(1,1)*get(2,0));
    }

    // Compatibility: m[r][c] syntax for tests
    struct RowProxy {
        Matrix3x3& mat; int r;
        float operator[](int c) const { return mat.get(r, c); }
    };
    struct ConstRowProxy {
        const Matrix3x3& mat; int r;
        float operator[](int c) const { return mat.get(r, c); }
    };

    // ── Debug ──────────────────────────────────────────────────
    std::string toString() const;
};

} // namespace math
} // namespace engine
