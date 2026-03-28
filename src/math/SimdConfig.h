#pragma once

/// SimdConfig — Detección automática de soporte SIMD.
///
/// Detecta en tiempo de compilación qué extensiones SIMD están disponibles
/// y define macros para que el resto del motor pueda usar rutas optimizadas.
///
/// Jerarquía SIMD x86:
///   SSE2   → 128 bits (4 floats en paralelo)  — mínimo en x86_64
///   SSE4.1 → Instrucciones extras (blend, floor, ceil)
///   AVX    → 256 bits (8 floats)
///   AVX2   → Operaciones enteras de 256 bits
///
/// En nuestro motor Vector2D usa SSE2: empaquetamos {x, y, 0, 0} en un __m128.

// ── Detección SSE2 ─────────────────────────────────────────────
#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
    #define ENGINE_SIMD_SSE2 1
    #include <emmintrin.h>  // SSE2 intrinsics
#else
    #define ENGINE_SIMD_SSE2 0
#endif

// ── Detección SSE4.1 ───────────────────────────────────────────
#if defined(__SSE4_1__)
    #define ENGINE_SIMD_SSE41 1
    #include <smmintrin.h>  // SSE4.1 intrinsics
#else
    #define ENGINE_SIMD_SSE41 0
#endif

// ── Alineación de memoria para SIMD ────────────────────────────
#if ENGINE_SIMD_SSE2
    #define ENGINE_ALIGN alignas(16)
#else
    #define ENGINE_ALIGN
#endif

namespace engine {
namespace math {

/// Devuelve true si el motor fue compilado con soporte SIMD
constexpr bool hasSIMD() {
    #if ENGINE_SIMD_SSE2
        return true;
    #else
        return false;
    #endif
}

/// Nombre de la extensión SIMD disponible
constexpr const char* simdName() {
    #if ENGINE_SIMD_SSE41
        return "SSE4.1";
    #elif ENGINE_SIMD_SSE2
        return "SSE2";
    #else
        return "None (scalar)";
    #endif
}

} // namespace math
} // namespace engine
