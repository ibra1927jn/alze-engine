#pragma once

#include "math/Vector3D.h"
#include "FluidSystem.h"
#include "Electromagnetism.h"
#include <cmath>

namespace engine {
namespace physics {

// ═══════════════════════════════════════════════════════════════
// Magnetohydrodynamics (MHD) — Coupling of EM fields with fluids
//
// Governs the behavior of electrically conducting fluids:
//   - Plasma (stars, fusion reactors)
//   - Liquid metals (mercury, molten iron — Earth's core)
//   - Salt water (ocean MHD)
//   - Ferrofluids
//
// Ideal MHD equations:
//   ∂ρ/∂t + ∇·(ρv) = 0
//   ρ(∂v/∂t + v·∇v) = -∇p + J×B + ρg
//   ∂B/∂t = ∇×(v×B) + η∇²B
//   ∇·B = 0
//
// where J = ∇×B/μ₀ (current density from Ampere's law)
// ═══════════════════════════════════════════════════════════════

namespace MHDConstants {
    constexpr float PI = 3.14159265359f;
    constexpr float MU_0 = 1.2566370614e-6f;  // Vacuum permeability (T·m/A)
    constexpr float EPSILON_0 = 8.8541878128e-12f; // Vacuum permittivity
}

// ═══════════════════════════════════════════════════════════════
// MHD Math — Core formulas
// ═══════════════════════════════════════════════════════════════

namespace MHDMath {

    /// Alfvén velocity: v_A = B / √(μ₀ρ)
    /// Speed of magnetic perturbations in a conducting fluid
    inline float alfvenVelocity(float magneticFieldStrength, float density) {
        if (density <= 0.0f) return 0.0f;
        return magneticFieldStrength / std::sqrt(MHDConstants::MU_0 * density);
    }

    /// Magnetic pressure: P_mag = B²/(2μ₀)
    /// Pressure exerted by the magnetic field
    inline float magneticPressure(float B) {
        return (B * B) / (2.0f * MHDConstants::MU_0);
    }

    /// Plasma beta: β = P_thermal / P_magnetic = 2μ₀p / B²
    /// β << 1: magnetically dominated, β >> 1: pressure dominated
    inline float plasmaBeta(float thermalPressure, float B) {
        float pMag = magneticPressure(B);
        return (pMag > 0.0f) ? thermalPressure / pMag : 0.0f;
    }

    /// Lorentz force on conducting fluid: F = J × B
    /// J: current density (A/m²), B: magnetic field (T)
    inline math::Vector3D lorentzForceOnFluid(const math::Vector3D& currentDensity,
                                               const math::Vector3D& magneticField) {
        return currentDensity.cross(magneticField);
    }

    /// Magnetic Reynolds number: Rm = μ₀σvL
    /// σ: electrical conductivity (S/m), v: velocity (m/s), L: length scale (m)
    /// Rm >> 1: field frozen in fluid, Rm << 1: field diffuses freely
    inline float magneticReynoldsNumber(float conductivity, float velocity, float lengthScale) {
        return MHDConstants::MU_0 * conductivity * velocity * lengthScale;
    }

    /// Magnetic diffusivity: η = 1/(μ₀σ)
    inline float magneticDiffusivity(float conductivity) {
        if (conductivity <= 0.0f) return 0.0f;
        return 1.0f / (MHDConstants::MU_0 * conductivity);
    }

    /// Ideal MHD: frozen-in flux condition
    /// In perfectly conducting fluid, B changes with flow:
    /// ∂B/∂t = ∇×(v×B)
    /// Simplified: new B ≈ B + dt · ∇×(v×B)
    /// For SPH, compute v×B then approximate curl
    inline math::Vector3D idealInduction(const math::Vector3D& velocity,
                                          const math::Vector3D& B) {
        return velocity.cross(B);
    }

    /// Magnetosonic speed: fast wave speed in MHD
    /// c_ms = √(c_s² + v_A²)
    /// c_s: sound speed, v_A: Alfvén speed
    inline float magnetosonicSpeed(float soundSpeed, float alfvenSpeed) {
        return std::sqrt(soundSpeed * soundSpeed + alfvenSpeed * alfvenSpeed);
    }

} // namespace MHDMath

// ═══════════════════════════════════════════════════════════════
// Conducting Fluid Properties
// ═══════════════════════════════════════════════════════════════

struct ConductingFluidProperties {
    float electricalConductivity; // S/m
    float density;                // kg/m³
    float thermalPressure;        // Pa

    static ConductingFluidProperties SolarPlasma() {
        return {1e6f, 1.4e-4f, 3.45e4f};  // Solar corona
    }
    static ConductingFluidProperties MoltenIron() {
        return {1.06e6f, 7000.0f, 1e9f};  // Earth's outer core
    }
    static ConductingFluidProperties Mercury() {
        return {1.04e6f, 13534.0f, 1e5f};
    }
    static ConductingFluidProperties Seawater() {
        return {4.8f, 1025.0f, 1e5f};
    }
    static ConductingFluidProperties FusionPlasma() {
        return {1e8f, 1e-4f, 1e6f};       // Tokamak
    }
};

// ═══════════════════════════════════════════════════════════════
// MHDSystem — Applies Lorentz force to SPH particles in B field
// ═══════════════════════════════════════════════════════════════

class MHDSystem {
public:
    math::Vector3D externalB = math::Vector3D(0, 0, 1.0f); // Uniform external B field (Tesla)
    float conductivity = 1e6f; // S/m (electrical conductivity of the fluid)

    /// Apply J×B Lorentz force to SPH particles
    /// Assumes each particle carries a small current from its velocity in B
    void applyToFluid(FluidSystem& fluid, float dt) {
        int n = fluid.getParticleCount();
        (void)dt;

        for (int i = 0; i < n; i++) {
            auto& p = fluid.getParticle(i);
            if (!p.isActive) continue;

            // Induced current density: J = σ(v × B)  (Ohm's law for moving conductor)
            math::Vector3D J = (p.velocity.cross(externalB)) * conductivity;

            // Lorentz force: F = J × B (per unit volume)
            math::Vector3D F = J.cross(externalB);

            // Apply as acceleration (F/ρ)
            if (p.density > 1e-6f) {
                p.velocity += F * (dt / p.density);
            }
        }
    }

    /// Compute Alfvén speed for the current configuration
    float getAlfvenSpeed(float density) const {
        return MHDMath::alfvenVelocity(externalB.magnitude(), density);
    }

    /// Compute magnetic Reynolds number
    float getMagneticReynolds(float velocity, float lengthScale) const {
        return MHDMath::magneticReynoldsNumber(conductivity, velocity, lengthScale);
    }
};

} // namespace physics
} // namespace engine
