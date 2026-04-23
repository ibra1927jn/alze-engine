#pragma once

#include "PhysicsConfig.h"
#include "FluidSystem.h"
#include "Chemistry.h"
#include "Thermodynamics.h"
#include "Electromagnetism.h"
#include "GravityNBody.h"
#include "WaveSystem.h"
#include "MHDSystem.h"
#include "math/Vector3D.h"
#include <cmath>
#include <vector>

namespace engine {
namespace physics {

// Forward declarations for optional systems
class FractureSystem;

// ═══════════════════════════════════════════════════════════════
// UnifiedSimulation — Orchestrates ALL physics subsystems
//
// Solves the "island problem": instead of each system being
// independent, this class manages execution order and data flow
// between systems every frame:
//
//   1. Thermal → SPH viscosity update  (hot water flows faster)
//   2. Chemistry reactions → heat injection into SPH
//   3. EM fields → Lorentz force on charged SPH particles (MHD)
//   4. SPH step (with updated viscosity + forces)
//   5. Thermal diffusion via SPH neighbor data
//   6. Wave propagation update
//   7. N-body gravity update
//   8. EM step for rigid bodies
//
// This ensures physically correct causality between systems.
// ═══════════════════════════════════════════════════════════════

class UnifiedSimulation {
public:
    // ── Subsystem Registration ─────────────────────────────────
    void setFluidSystem(FluidSystem* fs)       { m_fluid = fs; }
    void setThermalSystem(ThermalSystem* ts)    { m_thermal = ts; }
    void setChemistry(ReactionSystem* cs)       { m_chemistry = cs; }
    void setEMSystem(EMSystem* em)              { m_em = em; }
    void setGravity(GravityNBodySystem* gn)     { m_gravity = gn; }
    void setWaveSystem(WaveSystem* ws)          { m_wave = ws; }
    void setMHD(MHDSystem* mhd)                { m_mhd = mhd; }

    PhysicsConfig& config() { return m_config; }
    const PhysicsConfig& config() const { return m_config; }

    // ── Statistics ─────────────────────────────────────────────
    int getActiveSubsystemCount() const {
        int count = 0;
        if (m_fluid) count++;
        if (m_thermal) count++;
        if (m_chemistry) count++;
        if (m_em) count++;
        if (m_gravity) count++;
        if (m_wave) count++;
        if (m_mhd) count++;
        return count;
    }

    bool hasCoupling() const {
        return m_config.thermoFluidCoupling || m_config.chemFluidCoupling || m_config.emFluidCoupling;
    }

    /// Main orchestrated step — called once per physics timestep
    void step(float dt, std::vector<RigidBody3D>& bodies) {

        // ══ Phase 1: Temperature → Viscosity coupling ══════════
        // Hot water is less viscous (Andrade equation)
        if (m_config.thermoFluidCoupling && m_fluid) {
            updateViscosityFromTemperature();
        }

        // ══ Phase 2: Chemistry → Heat injection ════════════════
        // Exothermic reactions heat nearby fluid particles
        if (m_chemistry) {
            m_chemistry->step(dt);
            if (m_config.chemFluidCoupling && m_fluid) {
                injectReactionHeat();
            }
        }

        // ══ Phase 3: EM → Fluid coupling (MHD) ════════════════
        // Lorentz force J×B on conducting fluid particles
        if (m_config.emFluidCoupling && m_mhd && m_fluid) {
            m_mhd->applyToFluid(*m_fluid, dt);
        }

        // ══ Phase 4: SPH Fluid step ════════════════════════════
        // Runs with updated viscosity, injected heat, and EM forces
        if (m_fluid) {
            m_fluid->step(dt);
        }

        // ══ Phase 5: Thermal diffusion in fluid ════════════════
        // Heat diffuses between neighboring SPH particles via Fourier's law
        if (m_config.thermoFluidCoupling && m_fluid) {
            diffuseHeatInFluid(dt);
            // Check phase transitions (freeze/boil)
            checkPhaseTransitions();
        }

        // ══ Phase 6: Thermal step for rigid bodies ═════════════
        if (m_thermal) {
            m_thermal->step(dt, bodies);
        }

        // ══ Phase 7: EM step for rigid bodies ══════════════════
        if (m_em) {
            m_em->step(dt, bodies);
        }

        // ══ Phase 8: Wave propagation ══════════════════════════
        if (m_wave) {
            m_wave->step(dt, bodies);
        }

        // ══ Phase 9: N-body gravity ════════════════════════════
        if (m_gravity) {
            m_gravity->step(dt, bodies);
        }
    }

private:
    PhysicsConfig m_config;

    FluidSystem*       m_fluid     = nullptr;
    ThermalSystem*     m_thermal   = nullptr;
    ReactionSystem*    m_chemistry = nullptr;
    EMSystem*          m_em        = nullptr;
    GravityNBodySystem* m_gravity  = nullptr;
    WaveSystem*        m_wave      = nullptr;
    MHDSystem*         m_mhd       = nullptr;

    // ── Coupling: Temperature → Viscosity ──────────────────────
    // Uses Andrade equation: μ(T) = μ_ref · exp(B·(1/T - 1/T_ref))
    // Water: B ≈ 1870K; viscosity roughly halves per 25°C increase
    void updateViscosityFromTemperature() {
        if (!m_fluid) return;
        int n = m_fluid->getParticleCount();
        constexpr float T_ref = 293.15f;
        constexpr float B_andrade = 1870.0f;
        float baseViscosity = m_config.sphViscosity;

        for (int i = 0; i < n; i++) {
            auto& p = m_fluid->getParticle(i);
            if (!p.isActive) continue;
            float T = std::max(p.temperature, 200.0f); // Clamp to prevent singularity
            float ratio = std::exp(B_andrade * (1.0f / T - 1.0f / T_ref));
            p.viscosity = baseViscosity * ratio;
        }
    }

    // ── Coupling: Chemistry → Fluid Heat ───────────────────────
    void injectReactionHeat() {
        if (!m_chemistry || !m_fluid) return;
        int nVols = m_chemistry->getVolumeCount();
        int nParts = m_fluid->getParticleCount();

        for (int v = 0; v < nVols; v++) {
            auto& vol = m_chemistry->getVolume(v);
            float heat_kJ = vol.lastHeatReleased;
            if (std::abs(heat_kJ) < 1e-6f) continue;

            // Distribute heat to nearby fluid particles
            float rSq = vol.radius * vol.radius;
            int affected = 0;

            for (int i = 0; i < nParts; i++) {
                const auto& p = m_fluid->getParticle(i);
                if (!p.isActive) continue;
                if ((p.position - vol.position).sqrMagnitude() < rSq) affected++;
            }
            if (affected == 0) continue;

            // ΔT = Q / (m·c) per particle
            float heatPerParticle_J = (heat_kJ * 1000.0f) / static_cast<float>(affected);

            for (int i = 0; i < nParts; i++) {
                auto& p = m_fluid->getParticle(i);
                if (!p.isActive) continue;
                if ((p.position - vol.position).sqrMagnitude() >= rSq) continue;
                // Exothermic (heat<0 in convention) → temperature increases
                p.temperature -= heatPerParticle_J / (p.mass * m_config.specificHeatCapacity);
            }
        }
    }

    // ── Coupling: Thermal diffusion in SPH ────────────────────
    void diffuseHeatInFluid(float dt) {
        if (!m_fluid) return;
        int n = m_fluid->getParticleCount();
        float h = m_fluid->smoothingRadius;
        float alpha = m_config.thermalDiffusivity;

        std::vector<float> dT(n, 0.0f);

        for (int i = 0; i < n; i++) {
            auto& pi = m_fluid->getParticle(i);
            if (!pi.isActive) continue;

            float sumLap = 0.0f;
            for (int j = 0; j < n; j++) {
                if (j == i) continue;
                const auto& pj = m_fluid->getParticle(j);
                if (!pj.isActive) continue;

                math::Vector3D diff = pi.position - pj.position;
                float dist = diff.magnitude();
                if (dist >= h || dist < 1e-6f) continue;

                float lapW = SPHKernels::viscosityLaplacian(dist, h);
                sumLap += (pj.mass / std::max(pj.density, 1.0f)) * (pj.temperature - pi.temperature) * lapW;
            }
            dT[i] = alpha * sumLap * dt;
        }

        for (int i = 0; i < n; i++) {
            m_fluid->getParticle(i).temperature += dT[i];
        }
    }

    // ── Phase transitions ─────────────────────────────────────
    void checkPhaseTransitions() {
        if (!m_fluid) return;
        for (int i = 0; i < m_fluid->getParticleCount(); i++) {
            auto& p = m_fluid->getParticle(i);
            if (!p.isActive) continue;

            if (p.temperature < m_config.freezingPoint) {
                p.velocity = p.velocity * 0.01f; // Near-freeze
            } else if (p.temperature > m_config.boilingPoint) {
                p.velocity.y += 2.0f * 0.016f;   // Buoyant evaporation
            }
        }
    }
};

} // namespace physics
} // namespace engine
