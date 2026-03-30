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

    /// Damping ratio: zeta = c / c_critical. zeta<1 underdamped, =1 critical, >1 overdamped
    inline float dampingRatio(float damping, float mass, float stiffness) {
        float cc = criticalDamping(mass, stiffness);
        return (cc > 1e-8f) ? damping / cc : 0.0f;
    }

    /// Create spring parameters from natural frequency (Hz) and damping ratio
    /// Returns {stiffness, damping}
    inline void springFromFrequency(float mass, float frequencyHz, float dampingRatio,
                                     float& outStiffness, float& outDamping) {
        float omega = 6.28318530718f * frequencyHz; // omega = 2*pi*f
        outStiffness = mass * omega * omega;         // k = m * omega^2
        outDamping = dampingRatio * 2.0f * mass * omega; // c = zeta * 2 * m * omega
    }

    /// Implicit (semi-implicit) Euler spring-damper step for stiff springs.
    /// Computes new velocity and position in a single step, unconditionally stable.
    /// Returns {newPosition, newVelocity}
    inline void implicitSpringDamperStep(
        const math::Vector3D& position, const math::Vector3D& velocity,
        const math::Vector3D& anchor,   // Rest position / target
        float stiffness, float damping, float mass, float dt,
        math::Vector3D& outPosition, math::Vector3D& outVelocity)
    {
        // Implicit Euler: v_{n+1} = v_n + dt * (-k * x_{n+1} - c * v_{n+1}) / m
        // Solving: v_{n+1} = (v_n - dt*k*x_n/m) / (1 + dt*c/m + dt^2*k/m)
        if (mass < 1e-8f) { outPosition = position; outVelocity = velocity; return; }
        float invMass = 1.0f / mass;
        float dtk = dt * stiffness * invMass;
        float dtc = dt * damping * invMass;
        float denom = 1.0f / (1.0f + dtc + dt * dtk);

        math::Vector3D displacement = position - anchor;
        outVelocity = (velocity - displacement * dtk) * denom;
        outPosition = position + outVelocity * dt;
    }

    /// Damped harmonic oscillator exact solution (for 1D analytical reference)
    /// x(t) = A * e^(-zeta*omega*t) * cos(omega_d*t + phi)
    /// omega_d = omega * sqrt(1 - zeta^2) for underdamped
    inline float dampedOscillator(float amplitude, float omega, float zeta,
                                   float phase, float time) {
        if (zeta >= 1.0f) {
            // Critically/overdamped: simple exponential decay
            return amplitude * std::exp(-omega * zeta * time);
        }
        float omegaD = omega * std::sqrt(1.0f - zeta * zeta);
        return amplitude * std::exp(-zeta * omega * time) *
               std::cos(omegaD * time + phase);
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
        if (radius < 1e-8f) return 0.0f;
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

    // ── Symplectic Euler ─────────────────────────────────────────

    struct IntegrationState {
        math::Vector3D position;
        math::Vector3D velocity;
    };

    inline IntegrationState symplecticEulerStep(const math::Vector3D& pos,
                                                 const math::Vector3D& vel,
                                                 const math::Vector3D& accel, float dt) {
        math::Vector3D velNew = vel + accel * dt;
        return {pos + velNew * dt, velNew};
    }

    // ── RK4 with force callback ────────────────────────────────

    template<typename ForceFunc>
    inline IntegrationState rk4Step(const math::Vector3D& pos, const math::Vector3D& vel,
                                     float invMass, float dt, ForceFunc forceFunc) {
        math::Vector3D a1 = forceFunc(pos, vel) * invMass;
        math::Vector3D v1 = vel;
        math::Vector3D p2 = pos + v1 * (dt * 0.5f);
        math::Vector3D v2 = vel + a1 * (dt * 0.5f);
        math::Vector3D a2 = forceFunc(p2, v2) * invMass;
        math::Vector3D p3 = pos + v2 * (dt * 0.5f);
        math::Vector3D v3 = vel + a2 * (dt * 0.5f);
        math::Vector3D a3 = forceFunc(p3, v3) * invMass;
        math::Vector3D v4 = vel + a3 * dt;
        math::Vector3D a4 = forceFunc(pos + v3 * dt, v4) * invMass;
        math::Vector3D posNew = pos + (v1 + v2 * 2.0f + v3 * 2.0f + v4) * (dt / 6.0f);
        math::Vector3D velNew = vel + (a1 + a2 * 2.0f + a3 * 2.0f + a4) * (dt / 6.0f);
        return {posNew, velNew};
    }

    // ── Energy Computation ─────────────────────────────────────

    inline float kineticEnergy(float mass, const math::Vector3D& velocity) {
        return 0.5f * mass * velocity.dot(velocity);
    }

    inline float rotationalKineticEnergy(const math::Vector3D& angularVelocity,
                                          const math::Vector3D& invInertia) {
        float ex = (invInertia.x > 0) ? angularVelocity.x * angularVelocity.x / invInertia.x : 0;
        float ey = (invInertia.y > 0) ? angularVelocity.y * angularVelocity.y / invInertia.y : 0;
        float ez = (invInertia.z > 0) ? angularVelocity.z * angularVelocity.z / invInertia.z : 0;
        return 0.5f * (ex + ey + ez);
    }

    inline float gravitationalPotentialEnergy(float mass, float height, float g = GRAVITY_EARTH) {
        return mass * g * height;
    }

    inline float totalMechanicalEnergy(float mass, const math::Vector3D& velocity,
                                        const math::Vector3D& angularVelocity,
                                        const math::Vector3D& invInertia,
                                        float height, float g = GRAVITY_EARTH) {
        return kineticEnergy(mass, velocity) +
               rotationalKineticEnergy(angularVelocity, invInertia) +
               gravitationalPotentialEnergy(mass, height, g);
    }

    // ── Angular Momentum ───────────────────────────────────────

    inline math::Vector3D angularMomentum(const math::Vector3D& angularVelocity,
                                           const math::Vector3D& invInertia) {
        return math::Vector3D(
            invInertia.x > 0 ? angularVelocity.x / invInertia.x : 0,
            invInertia.y > 0 ? angularVelocity.y / invInertia.y : 0,
            invInertia.z > 0 ? angularVelocity.z / invInertia.z : 0
        );
    }

    // ── Interpolation ───────────────────────────────────────────

    inline float smoothstep(float edge0, float edge1, float x) {
        float range = edge1 - edge0;
        if (std::abs(range) < 1e-8f) return 0.0f;
        float t = (x - edge0) / range;
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        return t * t * (3.0f - 2.0f * t);
    }

    inline float smootherstep(float edge0, float edge1, float x) {
        float range = edge1 - edge0;
        if (std::abs(range) < 1e-8f) return 0.0f;
        float t = (x - edge0) / range;
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
