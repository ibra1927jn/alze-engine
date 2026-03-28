#pragma once

#include <cmath>
#include "math/Vector3D.h"

namespace engine {
namespace physics {

/// PhysicsMath — Advanced mathematical utilities for physics simulation.
///
/// Contains numerical methods, oscillators, and physical equations
/// that go beyond basic linear algebra.
///
namespace PhysicsMath {

    // ── Constants ───────────────────────────────────────────────
    constexpr float STEFAN_BOLTZMANN   = 5.670374419e-8f; // W/(m²·K⁴)
    constexpr float BOLTZMANN_K        = 1.380649e-23f;    // J/K
    constexpr float AIR_DENSITY        = 1.225f;           // kg/m³ (sea level, 15°C)
    constexpr float WATER_DENSITY      = 1000.0f;          // kg/m³
    constexpr float GRAVITY_EARTH      = 9.80665f;         // m/s²
    constexpr float ABSOLUTE_ZERO      = 0.0f;             // K
    constexpr float ROOM_TEMP          = 293.15f;          // K (20°C)

    // ── Spring-Damper System ────────────────────────────────────

    /// Compute spring-damper force: F = -kx - cv
    /// x = displacement from rest, v = velocity, k = stiffness, c = damping
    inline math::Vector3D springDamperForce(const math::Vector3D& displacement,
                                             const math::Vector3D& velocity,
                                             float stiffness, float damping) {
        return displacement * (-stiffness) + velocity * (-damping);
    }

    /// Spring-damper scalar version
    inline float springDamperScalar(float x, float v, float k, float c) {
        return -k * x - c * v;
    }

    /// Critical damping coefficient for a mass-spring system
    inline float criticalDamping(float mass, float stiffness) {
        return 2.0f * std::sqrt(mass * stiffness);
    }

    // ── Wave Equation ───────────────────────────────────────────

    /// Simple harmonic wave: y = A * sin(2πft + φ)
    inline float waveEquation(float amplitude, float frequency,
                               float phase, float time) {
        return amplitude * std::sin(6.28318530718f * frequency * time + phase);
    }

    /// Damped wave: y = A * e^(-γt) * sin(2πft + φ)
    inline float dampedWave(float amplitude, float frequency,
                             float dampingRate, float phase, float time) {
        return amplitude * std::exp(-dampingRate * time) *
               std::sin(6.28318530718f * frequency * time + phase);
    }

    // ── Aerodynamics ────────────────────────────────────────────

    /// Drag force: F_drag = -0.5 * ρ * Cd * A * |v|² * v̂
    /// rho = fluid density, cd = drag coefficient, area = cross-section
    inline math::Vector3D dragForce(const math::Vector3D& velocity,
                                     float rho, float cd, float area) {
        float speedSq = velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z;
        if (speedSq < 1e-8f) return math::Vector3D(0, 0, 0);
        // F = -0.5 * rho * cd * A * |v| * v  (factor out one |v| from |v|^2)
        float factor = -0.5f * rho * cd * area;
        float speed = std::sqrt(speedSq);
        return velocity * (factor * speed);
    }

    /// Terminal velocity for a falling object
    inline float terminalVelocity(float mass, float g, float rho,
                                   float cd, float area) {
        return std::sqrt((2.0f * mass * g) / (rho * cd * area));
    }

    /// Magnus force (spinning object in fluid): F = S * (ω × v)
    /// S = Magnus coefficient (depends on geometry)
    inline math::Vector3D magnusForce(const math::Vector3D& velocity,
                                       const math::Vector3D& angularVelocity,
                                       float magnusCoefficient) {
        return angularVelocity.cross(velocity) * magnusCoefficient;
    }

    /// Buoyancy force (Archimedes): F = ρ_fluid * V_submerged * g
    /// submergedFraction [0..1] = fraction of object volume below fluid surface
    inline math::Vector3D buoyancyForce(float fluidDensity, float objectVolume,
                                         float submergedFraction, float gravity) {
        float magnitude = fluidDensity * objectVolume * submergedFraction * gravity;
        return math::Vector3D(0, magnitude, 0);
    }

    /// Approximate submerged fraction for a sphere at given depth
    /// depth > 0 means center is above surface, < 0 means below
    inline float sphereSubmergedFraction(float radius, float depthBelowSurface) {
        if (depthBelowSurface <= -radius) return 0.0f;
        if (depthBelowSurface >= radius)  return 1.0f;
        // h = submerged height of sphere cap
        float h = radius + depthBelowSurface;
        // V_cap / V_sphere = h²(3r - h) / (4r³)
        return (h * h * (3.0f * radius - h)) / (4.0f * radius * radius * radius);
    }

    // ── Thermodynamics ──────────────────────────────────────────

    /// Newton's law of cooling/conduction: Q = k * A * ΔT * dt
    /// k = thermal conductivity, area = contact area, deltaT = temp difference
    inline float conductionHeat(float conductivityA, float conductivityB,
                                 float contactArea, float deltaT, float dt) {
        // Combined conductivity (harmonic mean for series conduction)
        float kCombined = (conductivityA > 0 && conductivityB > 0)
            ? 2.0f * conductivityA * conductivityB / (conductivityA + conductivityB)
            : 0.0f;
        return kCombined * contactArea * deltaT * dt;
    }

    /// Stefan-Boltzmann radiation: P = ε * σ * A * T⁴
    inline float radiationPower(float emissivity, float area, float temperatureK) {
        float T2 = temperatureK * temperatureK;
        return emissivity * STEFAN_BOLTZMANN * area * T2 * T2;
    }

    /// Friction heat generation: Q = |F_friction · v_sliding| * dt
    inline float frictionHeat(float frictionForce, float slidingSpeed, float dt) {
        return std::abs(frictionForce * slidingSpeed) * dt;
    }

    /// Temperature change from absorbed heat: ΔT = Q / (m * c)
    inline float temperatureChange(float heat, float mass, float specificHeat) {
        if (mass * specificHeat < 1e-8f) return 0.0f;
        return heat / (mass * specificHeat);
    }

    // ── Rolling Friction ────────────────────────────────────────

    /// Rolling resistance torque: τ = μr * N * r
    /// normalForce = N, radius = r, rollingFriction = μr
    inline float rollingResistanceTorque(float rollingFriction,
                                          float normalForce, float radius) {
        return rollingFriction * normalForce * radius;
    }

    // ── Numeric Integration ─────────────────────────────────────

    /// Velocity-Verlet position step: x(t+dt) = x(t) + v(t)*dt + 0.5*a(t)*dt²
    inline math::Vector3D verletPositionStep(const math::Vector3D& pos,
                                              const math::Vector3D& vel,
                                              const math::Vector3D& accel,
                                              float dt) {
        return pos + vel * dt + accel * (0.5f * dt * dt);
    }

    /// Velocity-Verlet velocity step: v(t+dt) = v(t) + 0.5*(a(t) + a(t+dt))*dt
    inline math::Vector3D verletVelocityStep(const math::Vector3D& vel,
                                              const math::Vector3D& accelOld,
                                              const math::Vector3D& accelNew,
                                              float dt) {
        return vel + (accelOld + accelNew) * (0.5f * dt);
    }

    // ── Interpolation ───────────────────────────────────────────

    inline float smoothstep(float edge0, float edge1, float x) {
        float t = (x - edge0) / (edge1 - edge0);
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        return t * t * (3.0f - 2.0f * t);
    }

    inline float smootherstep(float edge0, float edge1, float x) {
        float t = (x - edge0) / (edge1 - edge0);
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }

    // ── Volume calculations for buoyancy ────────────────────────

    inline float sphereVolume(float radius) {
        return (4.0f / 3.0f) * 3.14159265358979f * radius * radius * radius;
    }

    inline float boxVolume(float w, float h, float d) {
        return w * h * d;
    }

    inline float capsuleVolume(float height, float radius) {
        float cyl = 3.14159265358979f * radius * radius * height;
        float sph = sphereVolume(radius);
        return cyl + sph;
    }

    /// Cross-section area for drag (auto-compute from shape)
    inline float sphereCrossSection(float radius) {
        return 3.14159265358979f * radius * radius;
    }

    inline float boxCrossSection(float w, float h) {
        return w * h;
    }

} // namespace PhysicsMath

} // namespace physics
} // namespace engine
