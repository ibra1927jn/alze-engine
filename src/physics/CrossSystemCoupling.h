#pragma once

#include "math/Vector3D.h"
#include "FluidSystem.h"
#include "Chemistry.h"
#include "Thermodynamics.h"
#include <cmath>

namespace engine {
namespace physics {

// ═══════════════════════════════════════════════════════════════
// Cross-System Coupling — Bridges between Thermo, SPH, Chemistry
//
// Makes the physics systems interact realistically:
//   - SPH particles carry temperature → thermal convection
//   - Chemistry reactions heat/cool fluid volumes
//   - Temperature triggers phase transitions in SPH
//   - Exothermic reactions create pressure waves in fluid
// ═══════════════════════════════════════════════════════════════

namespace PhaseTransition {

    enum class Phase { SOLID, LIQUID, GAS };

    /// Determine phase from temperature and material properties
    inline Phase getPhase(float temperatureK, float meltingPoint, float boilingPoint) {
        if (temperatureK < meltingPoint) return Phase::SOLID;
        if (temperatureK > boilingPoint) return Phase::GAS;
        return Phase::LIQUID;
    }

    /// Latent heat required for phase transition (kJ/mol)
    struct LatentHeat {
        float fusion = 6.01f;      // kJ/mol (water: melting)
        float vaporization = 40.7f; // kJ/mol (water: boiling)

        static LatentHeat Water()  { return {6.01f, 40.7f}; }
        static LatentHeat Iron()   { return {13.81f, 340.0f}; }
        static LatentHeat Copper() { return {13.26f, 300.0f}; }
    };

} // namespace PhaseTransition

// ═══════════════════════════════════════════════════════════════
// ThermalFluidCoupling — Temperature advection in SPH
// ═══════════════════════════════════════════════════════════════

class ThermalFluidCoupling {
public:
    float thermalDiffusivity = 1.43e-7f; // m²/s (water at 25°C)

    /// Step: diffuse heat between neighboring SPH particles
    /// Uses SPH Laplacian of temperature field
    void step(FluidSystem& fluid, float dt) {
        int n = fluid.getParticleCount();
        if (n <= 0) return;

        float h = fluid.smoothingRadius;

        // Store temperature changes (don't modify during Loop)
        std::vector<float> dT(n, 0.0f);

        for (int i = 0; i < n; i++) {
            auto& pi = fluid.getParticle(i);
            if (!pi.isActive) continue;

            float sumLap = 0.0f;

            // Iterate over all particles (simplified — production code uses neighbor grid)
            for (int j = 0; j < n; j++) {
                if (j == i) continue;
                const auto& pj = fluid.getParticle(j);
                if (!pj.isActive) continue;

                math::Vector3D diff = pi.position - pj.position;
                float dist = diff.magnitude();
                if (dist >= h || dist < 1e-6f) continue;

                // SPH Laplacian: ∇²T ≈ Σ mⱼ/ρⱼ · (Tⱼ - Tᵢ) · ∇²W
                float lapW = SPHKernels::viscosityLaplacian(dist, h);
                sumLap += (pj.mass / pj.density) * (pj.temperature - pi.temperature) * lapW;
            }

            // Fourier's law: ∂T/∂t = α·∇²T
            dT[i] = thermalDiffusivity * sumLap * dt;
        }

        // Apply temperature changes
        for (int i = 0; i < n; i++) {
            fluid.getParticle(i).temperature += dT[i];
        }
    }

    /// Check for phase transitions and modify particle behavior
    void checkPhaseTransitions(FluidSystem& fluid,
                                float meltingPoint = 273.15f,
                                float boilingPoint = 373.15f) {
        for (int i = 0; i < fluid.getParticleCount(); i++) {
            auto& p = fluid.getParticle(i);
            if (!p.isActive) continue;

            auto phase = PhaseTransition::getPhase(p.temperature, meltingPoint, boilingPoint);

            if (phase == PhaseTransition::Phase::SOLID) {
                // Freeze: deactivate particle (or set very high viscosity)
                p.velocity = p.velocity * 0.01f; // Near-zero velocity
            }
            else if (phase == PhaseTransition::Phase::GAS) {
                // Boil: apply upward buoyancy force
                p.velocity.y += 2.0f * 0.016f; // Buoyant acceleration per frame
            }
        }
    }
};

// ═══════════════════════════════════════════════════════════════
// ChemFluidCoupling — Chemical reactions affect SPH particles
// ═══════════════════════════════════════════════════════════════

class ChemFluidCoupling {
public:
    /// Apply heat from chemistry reaction to nearby fluid particles
    /// heatKJ: heat released in kJ (negative = endothermic)
    /// center: reaction location
    /// radius: effect radius
    void applyReactionHeat(FluidSystem& fluid, float heatKJ,
                            const math::Vector3D& center, float radius) {
        if (std::abs(heatKJ) < 1e-6f || radius <= 0.0f) return;

        int n = fluid.getParticleCount();
        float rSq = radius * radius;
        int affected = 0;

        // Count affected particles
        for (int i = 0; i < n; i++) {
            const auto& p = fluid.getParticle(i);
            if (!p.isActive) continue;
            if ((p.position - center).sqrMagnitude() < rSq) affected++;
        }

        if (affected == 0) return;

        // Distribute heat evenly (Q = mcΔT → ΔT = Q/(mc))
        // Approximate: each particle gets heatKJ/affected (in kJ), convert via specific heat
        float heatPerParticle_J = (heatKJ * 1000.0f) / static_cast<float>(affected);

        for (int i = 0; i < n; i++) {
            auto& p = fluid.getParticle(i);
            if (!p.isActive) continue;
            if ((p.position - center).sqrMagnitude() >= rSq) continue;

            // ΔT = Q / (m·c)
            // Using water specific heat ≈ 4186 J/(kg·K)
            float specificHeat = 4186.0f;
            p.temperature -= heatPerParticle_J / (p.mass * specificHeat);
        }
    }

    /// Create pressure wave from exothermic reaction (explosion-like effect)
    void createPressureWave(FluidSystem& fluid, float energyJ,
                             const math::Vector3D& center, float radius) {
        if (energyJ <= 0.0f) return;

        int n = fluid.getParticleCount();
        float rSq = radius * radius;

        for (int i = 0; i < n; i++) {
            auto& p = fluid.getParticle(i);
            if (!p.isActive) continue;

            math::Vector3D diff = p.position - center;
            float distSq = diff.sqrMagnitude();
            if (distSq >= rSq || distSq < 1e-6f) continue;

            float dist = std::sqrt(distSq);
            // Impulse decays with 1/r² (inverse square)
            float impulse = energyJ / (4.0f * 3.14159f * distSq);
            math::Vector3D dir = diff * (1.0f / dist);
            p.velocity += dir * (impulse / p.mass * 0.001f); // Scale factor for stability
        }
    }
};

} // namespace physics
} // namespace engine
