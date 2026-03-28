#pragma once

#include <cmath>

namespace engine {
namespace math {

/// Central math constants and utilities for the engine
namespace Constants {

    constexpr float PI       = 3.14159265358979323846f;
    constexpr float TAU      = PI * 2.0f;              // Full circle
    constexpr float HALF_PI  = PI * 0.5f;
    constexpr float DEG2RAD  = PI / 180.0f;
    constexpr float RAD2DEG  = 180.0f / PI;
    constexpr float EPSILON  = 1e-6f;
    constexpr float INF      = 1e30f;
    constexpr float SQRT2    = 1.41421356237309504880f;
    constexpr float INV_SQRT2 = 0.70710678118654752440f;

} // namespace Constants

/// SmoothStep — Hermite interpolation (smooth 0→1 transition)
inline float smoothstep(float edge0, float edge1, float x) {
    float t = (x - edge0) / (edge1 - edge0);
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return t * t * (3.0f - 2.0f * t);
}

/// SmootherStep — 5th-order smoothstep (even smoother, zero 2nd derivative at edges)
inline float smootherstep(float edge0, float edge1, float x) {
    float t = (x - edge0) / (edge1 - edge0);
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

/// InverseLerp — Find the t parameter for a value between a and b
inline float inverseLerp(float a, float b, float value) {
    float denom = b - a;
    if (std::abs(denom) < 1e-8f) return 0.0f;
    return (value - a) / denom;
}

/// Remap — Map a value from [inMin, inMax] to [outMin, outMax]
inline float remap(float value, float inMin, float inMax, float outMin, float outMax) {
    float t = inverseLerp(inMin, inMax, value);
    return outMin + t * (outMax - outMin);
}

/// Repeat — Like modulo but for floats (always positive result)
inline float repeat(float t, float length) {
    return t - std::floor(t / length) * length;
}

/// PingPong — Bounces value back and forth within [0, length]
inline float pingPong(float t, float length) {
    t = repeat(t, length * 2.0f);
    return length - std::abs(t - length);
}

/// DeltaAngle — Shortest difference between two angles (in radians)
inline float deltaAngle(float current, float target) {
    float diff = repeat(target - current + Constants::PI, Constants::TAU) - Constants::PI;
    return diff;
}

// ── Spline Interpolation ──────────────────────────────────────

/// Catmull-Rom spline — smooth curve through 4 control points at parameter t ∈ [0,1]
inline float catmullRom(float p0, float p1, float p2, float p3, float t) {
    float t2 = t * t, t3 = t2 * t;
    return 0.5f * ((2.0f * p1) +
                   (-p0 + p2) * t +
                   (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                   (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

/// Cubic Bezier — 4 control points at parameter t ∈ [0,1]
inline float cubicBezier(float p0, float p1, float p2, float p3, float t) {
    float u = 1.0f - t;
    float u2 = u * u, t2 = t * t;
    return u2 * u * p0 + 3.0f * u2 * t * p1 + 3.0f * u * t2 * p2 + t2 * t * p3;
}

// ── Easing Functions ──────────────────────────────────────────

inline float easeInQuad(float t) { return t * t; }
inline float easeOutQuad(float t) { return t * (2.0f - t); }
inline float easeInOutQuad(float t) {
    return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
}

inline float easeInCubic(float t) { return t * t * t; }
inline float easeOutCubic(float t) { float u = t - 1.0f; return u * u * u + 1.0f; }
inline float easeInOutCubic(float t) {
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f;
}

inline float easeInElastic(float t) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    return -std::pow(2.0f, 10.0f * t - 10.0f) * std::sin((t * 10.0f - 10.75f) * Constants::TAU / 3.0f);
}

inline float easeOutElastic(float t) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    return std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * Constants::TAU / 3.0f) + 1.0f;
}

inline float easeOutBounce(float t) {
    if (t < 1.0f / 2.75f)       return 7.5625f * t * t;
    if (t < 2.0f / 2.75f)       { t -= 1.5f / 2.75f; return 7.5625f * t * t + 0.75f; }
    if (t < 2.5f / 2.75f)       { t -= 2.25f / 2.75f; return 7.5625f * t * t + 0.9375f; }
    t -= 2.625f / 2.75f; return 7.5625f * t * t + 0.984375f;
}

// ── Spring / Damper ───────────────────────────────────────────

/// Exponential decay — instant interpolation feel (great for cameras)
/// Usage: value = expDecay(value, target, decay, dt);
inline float expDecay(float current, float target, float decay, float dt) {
    return target + (current - target) * std::exp(-decay * dt);
}

/// Critical damped spring — physically based smooth tracking
/// Updates both value and velocity in-place
inline void springDamper(float& value, float& velocity, float target,
                          float stiffness, float damping, float dt) {
    float force = stiffness * (target - value) - damping * velocity;
    velocity += force * dt;
    value += velocity * dt;
}

} // namespace math
} // namespace engine

