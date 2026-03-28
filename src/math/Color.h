#pragma once

#include <cstdint>
#include <iostream>

namespace engine {
namespace math {

/// Color — Representación RGBA de 0 a 255.
/// Compatible directamente con SDL2 (SDL_SetRenderDrawColor).
struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;

    // ── Constructores ──────────────────────────────────────────
    constexpr Color()
        : r(255), g(255), b(255), a(255) {}

    constexpr Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
        : r(r), g(g), b(b), a(a) {}

    // ── Colores predefinidos ───────────────────────────────────
    static constexpr Color white()       { return {255, 255, 255}; }
    static constexpr Color black()       { return {0, 0, 0}; }
    static constexpr Color red()         { return {255, 60, 60}; }
    static constexpr Color green()       { return {80, 220, 120}; }
    static constexpr Color blue()        { return {100, 180, 255}; }
    static constexpr Color yellow()      { return {255, 200, 80}; }
    static constexpr Color purple()      { return {200, 130, 255}; }
    static constexpr Color orange()      { return {255, 150, 80}; }
    static constexpr Color cyan()        { return {80, 255, 220}; }
    static constexpr Color magenta()     { return {255, 80, 200}; }
    static constexpr Color darkGray()    { return {40, 40, 55}; }
    static constexpr Color transparent() { return {0, 0, 0, 0}; }

    // ── Utilidades ─────────────────────────────────────────────

    /// Devuelve el color con un nuevo alpha
    constexpr Color withAlpha(uint8_t newAlpha) const {
        return {r, g, b, newAlpha};
    }

    /// Aclara el color sumando un valor a cada componente
    Color brighter(uint8_t amount) const {
        auto clamp = [](int v) -> uint8_t { return v > 255 ? 255 : static_cast<uint8_t>(v); };
        return {clamp(r + amount), clamp(g + amount), clamp(b + amount), a};
    }

    /// Oscurece el color restando un valor a cada componente
    Color darker(uint8_t amount) const {
        auto clamp = [](int v) -> uint8_t { return v < 0 ? 0 : static_cast<uint8_t>(v); };
        return {clamp(r - amount), clamp(g - amount), clamp(b - amount), a};
    }

    // ── Comparación ────────────────────────────────────────────
    constexpr bool operator==(const Color& other) const {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }

    constexpr bool operator!=(const Color& other) const {
        return !(*this == other);
    }

    // ── Debug ──────────────────────────────────────────────────
    friend std::ostream& operator<<(std::ostream& os, const Color& c) {
        os << "Color(" << (int)c.r << ", " << (int)c.g << ", "
           << (int)c.b << ", " << (int)c.a << ")";
        return os;
    }
};

} // namespace math
} // namespace engine
