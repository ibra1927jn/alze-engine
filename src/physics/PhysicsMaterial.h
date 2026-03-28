#pragma once

#include <cmath>

namespace engine {
namespace physics {

/// PhysicsMaterial — Scientific material properties system.
///
/// Encapsulates real-world physical constants for simulation:
///   - Friction (static, kinetic, rolling) with proper Coulomb model
///   - Density (kg/m³) for mass and buoyancy calculations
///   - Thermal properties (conductivity, specific heat, phase transitions)
///   - Aerodynamic coefficients (drag, lift)
///
/// Use presets for common materials:
///   auto mat = PhysicsMaterial::Steel();
///   auto mat = PhysicsMaterial::Ice();
///   body.material = PhysicsMaterial::Rubber();
///
struct PhysicsMaterial {
    // ── Friction ────────────────────────────────────────────────
    float staticFriction    = 0.6f;    // μs — Threshold to start moving
    float kineticFriction   = 0.4f;    // μk — Once moving
    float rollingFriction   = 0.01f;   // μr — Rolling resistance
    float stribeckVelocity  = 0.1f;    // m/s — Transition speed static→kinetic

    // ── Material ────────────────────────────────────────────────
    float density       = 1000.0f;     // kg/m³ (water=1000, steel=7800)
    float hardness      = 0.5f;        // [0..1] Affects contact deformation
    float restitution   = 0.3f;        // Coefficient of restitution (bounciness)

    // ── Thermal ─────────────────────────────────────────────────
    float thermalConductivity = 50.0f; // W/(m·K) — Heat flow rate
    float specificHeat        = 500.0f;// J/(kg·K) — Energy to raise 1K
    float latentHeatFusion    = 3.33e5f; // J/kg — Energy to melt 1kg (water=333 kJ/kg)
    float latentHeatVap       = 2.26e6f; // J/kg — Energy to boil 1kg (water=2260 kJ/kg)
    float thermalExpansionCoeff = 1.2e-5f; // 1/K — Linear expansion (steel=1.2e-5)
    float temperature         = 293.15f;// K (20°C default)
    float meltingPoint        = 1500.0f;// K — Phase transition temp
    float boilingPoint        = 3000.0f;// K
    float ignitionPoint       = 0.0f;  // K (0 = non-flammable)
    float emissivity          = 0.5f;  // [0..1] Thermal radiation factor

    // ── Aerodynamic ─────────────────────────────────────────────
    float dragCoefficient  = 0.47f;    // Cd (sphere=0.47, cube=1.05)
    float liftCoefficient  = 0.0f;     // CL
    float crossSectionArea = 0.0f;     // m² (0 = auto-compute from shape)

    // ── Solid Mechanics (Yield, Fracture, Fatigue) ──────────────
    float yieldStrength    = 250e6f;   // Pa (Stress to cause permanent plastic deformation)
    float ultimateStrength = 400e6f;   // Pa (Stress to cause fracture)
    float fatigueLimit     = 100e6f;   // Pa (Endurance limit below which no fatigue occurs)
    float fatigueSNExponent = 3.0f;    // 'm' in S-N curve approximation (Basquin's exponent)
    float elasticModulus   = 200e9f;   // Pa (Young's Modulus)
    float poissonRatio     = 0.3f;     // Transverse strain ratio

    // ── Combination Helpers ─────────────────────────────────────

    /// Combine two materials for a contact pair (geometric mean)
    static float combineFriction(float muA, float muB) {
        return std::sqrt(muA * muB);
    }

    /// Combine restitution (max of pair, physics convention)
    static float combineRestitution(float eA, float eB) {
        return (eA > eB) ? eA : eB;
    }

    /// Evaluate Stribeck friction curve for smooth static→kinetic transition
    /// Returns effective μ based on sliding velocity magnitude
    float stribeckFriction(float slidingSpeed) const {
        if (slidingSpeed < 1e-6f) return staticFriction;
        float t = slidingSpeed / stribeckVelocity;
        if (t >= 1.0f) return kineticFriction;
        // Smooth interpolation: static → kinetic
        float s = t * t * (3.0f - 2.0f * t); // smoothstep
        return staticFriction + (kineticFriction - staticFriction) * s;
    }

    // ── Presets ──────────────────────────────────────────────────

    static PhysicsMaterial Steel() {
        PhysicsMaterial m;
        m.staticFriction = 0.74f; m.kineticFriction = 0.57f;
        m.rollingFriction = 0.001f; m.density = 7800.0f;
        m.hardness = 0.9f; m.restitution = 0.6f;
        m.thermalConductivity = 50.2f; m.specificHeat = 502.0f;
        m.meltingPoint = 1643.0f; m.boilingPoint = 3273.0f;
        m.emissivity = 0.3f; m.dragCoefficient = 0.47f;
        m.yieldStrength = 250e6f; m.ultimateStrength = 400e6f;
        m.fatigueLimit = 100e6f; m.elasticModulus = 200e9f;
        return m;
    }

    static PhysicsMaterial Wood() {
        PhysicsMaterial m;
        m.staticFriction = 0.5f; m.kineticFriction = 0.3f;
        m.rollingFriction = 0.02f; m.density = 600.0f;
        m.hardness = 0.3f; m.restitution = 0.4f;
        m.thermalConductivity = 0.17f; m.specificHeat = 1700.0f;
        m.meltingPoint = 0.0f; m.boilingPoint = 0.0f;
        m.ignitionPoint = 573.0f; // ~300°C
        m.emissivity = 0.9f; m.dragCoefficient = 1.05f;
        m.yieldStrength = 40e6f; m.ultimateStrength = 50e6f;
        m.fatigueLimit = 15e6f; m.elasticModulus = 11e9f;
        return m;
    }

    static PhysicsMaterial Rubber() {
        PhysicsMaterial m;
        m.staticFriction = 1.0f; m.kineticFriction = 0.8f;
        m.rollingFriction = 0.015f; m.density = 1100.0f;
        m.hardness = 0.1f; m.restitution = 0.85f;
        m.thermalConductivity = 0.16f; m.specificHeat = 1880.0f;
        m.meltingPoint = 453.0f; m.ignitionPoint = 533.0f;
        m.emissivity = 0.92f; m.dragCoefficient = 0.47f;
        m.yieldStrength = 15e6f; m.ultimateStrength = 25e6f;
        m.fatigueLimit = 2e6f; m.elasticModulus = 0.05e9f;
        return m;
    }

    static PhysicsMaterial Ice() {
        PhysicsMaterial m;
        m.staticFriction = 0.1f; m.kineticFriction = 0.03f;
        m.rollingFriction = 0.005f; m.density = 917.0f;
        m.hardness = 0.15f; m.restitution = 0.2f;
        m.thermalConductivity = 2.22f; m.specificHeat = 2090.0f;
        m.meltingPoint = 273.15f; m.boilingPoint = 373.15f;
        m.emissivity = 0.97f; m.dragCoefficient = 0.47f;
        m.yieldStrength = 1e6f; m.ultimateStrength = 2e6f; 
        m.fatigueLimit = 0.5e6f; m.elasticModulus = 9e9f; // Brittle
        return m;
    }

    static PhysicsMaterial Glass() {
        PhysicsMaterial m;
        m.staticFriction = 0.94f; m.kineticFriction = 0.4f;
        m.rollingFriction = 0.002f; m.density = 2500.0f;
        m.hardness = 0.85f; m.restitution = 0.65f;
        m.thermalConductivity = 0.96f; m.specificHeat = 840.0f;
        m.meltingPoint = 1773.0f; m.boilingPoint = 2503.0f;
        m.emissivity = 0.92f; m.dragCoefficient = 0.47f;
        return m;
    }

    static PhysicsMaterial Concrete() {
        PhysicsMaterial m;
        m.staticFriction = 0.65f; m.kineticFriction = 0.5f;
        m.rollingFriction = 0.01f; m.density = 2400.0f;
        m.hardness = 0.75f; m.restitution = 0.2f;
        m.thermalConductivity = 1.7f; m.specificHeat = 880.0f;
        m.meltingPoint = 1500.0f; m.emissivity = 0.94f;
        m.dragCoefficient = 1.05f;
        m.yieldStrength = 3e6f; m.ultimateStrength = 30e6f; // Strong in comp, weak tang
        m.fatigueLimit = 15e6f; m.elasticModulus = 30e9f;
        return m;
    }

    static PhysicsMaterial Gold() {
        PhysicsMaterial m;
        m.staticFriction = 0.49f; m.kineticFriction = 0.39f;
        m.rollingFriction = 0.001f; m.density = 19300.0f;
        m.hardness = 0.25f; m.restitution = 0.3f;
        m.thermalConductivity = 317.0f; m.specificHeat = 129.0f;
        m.meltingPoint = 1337.0f; m.boilingPoint = 3129.0f;
        m.emissivity = 0.02f; m.dragCoefficient = 0.47f;
        return m;
    }
};

} // namespace physics
} // namespace engine
