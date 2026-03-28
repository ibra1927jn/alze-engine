#pragma once

#include "PhysicsMaterial.h"
#include "PhysicsMath.h"
#include "RigidBody3D.h"
#include <vector>
#include <algorithm>

namespace engine {
namespace physics {

/// ThermalBody — Thermal state attached to a RigidBody3D.
///
/// Tracks temperature, tracks phase, and emits events.
///
struct ThermalBody {
    int bodyIndex = -1;                 // Index into PhysicsWorld3D::m_bodies
    float temperature = 293.15f;        // Current temperature (K)
    float surfaceArea  = 1.0f;          // m² (for radiation/convection)
    const PhysicsMaterial* material = nullptr;

    enum Phase : uint8_t { SOLID, LIQUID, GAS };
    Phase phase = SOLID;
    float accumulatedLatentHeat = 0.0f; // J — Heat trapped during transition
    float baseTemperature = 293.15f;    // K — Temperature at which scale is 1.0

    bool isIgnited    = false;
    bool justMelted   = false;  // Set to true on frame of phase transition
    bool justIgnited  = false;

    void resetEvents() {
        justMelted = false;
        justIgnited = false;
    }

    void updatePhase() {
        if (!material) return;
        Phase oldPhase = phase;
        if (accumulatedLatentHeat > 0.0f) return; // Wait until transition completes

        if (temperature >= material->boilingPoint && material->boilingPoint > 0)
            phase = GAS;
        else if (temperature >= material->meltingPoint && material->meltingPoint > 0)
            phase = LIQUID;
        else
            phase = SOLID;

        if (phase != oldPhase && oldPhase == SOLID)
            justMelted = true;

        // Ignition check
        if (!isIgnited && material->ignitionPoint > 0 &&
            temperature >= material->ignitionPoint) {
            isIgnited = true;
            justIgnited = true;
        }
    }
};

/// ThermalSystem — Processes heat transfer between contacting bodies.
///
/// Call step() after the collision solver to simulate heat flow.
/// Supports:
///   - Conduction (between contacting bodies)
///   - Radiation (to environment)
///   - Friction heat (from collision solver impulses)
///   - Phase transitions (solid → liquid → gas)
///
class ThermalSystem {
public:
    float ambientTemperature = 293.15f;  // K (20°C)
    float timeScale = 1.0f;              // Speed up/slow down thermal sim

    /// Register a body for thermal simulation
    int addThermalBody(int bodyIndex, const PhysicsMaterial& mat,
                       float surfaceArea = 1.0f) {
        ThermalBody tb;
        tb.bodyIndex = bodyIndex;
        tb.temperature = mat.temperature;
        tb.surfaceArea = surfaceArea;
        tb.material = &mat;  // Must remain valid
        int idx = static_cast<int>(m_thermalBodies.size());
        m_thermalBodies.push_back(tb);
        return idx;
    }

    // Process thermal step. bodies is non-const to allow thermal expansion (scaling).
    void step(float dt, std::vector<RigidBody3D>& bodies) {
        float scaledDt = dt * timeScale;

        for (auto& tb : m_thermalBodies) {
            if (!tb.material) continue;
            tb.resetEvents();

            // ── Radiation to environment ────────────────────────
            float radiationOut = PhysicsMath::radiationPower(
                tb.material->emissivity, tb.surfaceArea, tb.temperature);
            float radiationIn = PhysicsMath::radiationPower(
                0.5f, tb.surfaceArea, ambientTemperature); // env radiates back

            float netRadiation = (radiationOut - radiationIn) * scaledDt;

            // Get body mass for ΔT calculation
            float mass = 1.0f;
            if (tb.bodyIndex >= 0 && tb.bodyIndex < static_cast<int>(bodies.size()))
                mass = bodies[tb.bodyIndex].getMass();
            if (mass < 0.01f) mass = 0.01f;

            // ── Convection to air (simplified Newton cooling) ───
            float convectionCoeff = 10.0f; // W/(m²·K) forced air
            float convectionHeat = convectionCoeff * tb.surfaceArea *
                                   (tb.temperature - ambientTemperature) * scaledDt;

            // ── Apply temperature change ────────────────────────
            float totalHeatLoss = netRadiation + convectionHeat;
            
            // Reusing the applyHeat logic for environment loss
            float heatToApply = std::abs(totalHeatLoss);
            int sign = (totalHeatLoss > 0) ? -1 : 1; // loss means negative heat to body
            
            if (sign < 0) {
                // Cooling (Loss)
                if (tb.phase == ThermalBody::LIQUID && tb.temperature <= tb.material->meltingPoint && tb.material->meltingPoint > 0) {
                    tb.temperature = tb.material->meltingPoint;
                    tb.accumulatedLatentHeat += heatToApply;
                    float requiredLatent = mass * tb.material->latentHeatFusion;
                    if (tb.accumulatedLatentHeat >= requiredLatent) {
                        tb.accumulatedLatentHeat = 0.0f;
                        tb.phase = ThermalBody::SOLID;
                    }
                } else if (tb.phase == ThermalBody::GAS && tb.temperature <= tb.material->boilingPoint && tb.material->boilingPoint > 0) {
                    tb.temperature = tb.material->boilingPoint;
                    tb.accumulatedLatentHeat += heatToApply;
                    float requiredLatent = mass * tb.material->latentHeatVap;
                    if (tb.accumulatedLatentHeat >= requiredLatent) {
                        tb.accumulatedLatentHeat = 0.0f;
                        tb.phase = ThermalBody::LIQUID;
                    }
                } else {
                    float deltaT = PhysicsMath::temperatureChange(totalHeatLoss, mass, tb.material->specificHeat);
                    tb.temperature -= deltaT;
                }
            } else {
                // Heating (Gain)
                if (tb.phase == ThermalBody::SOLID && tb.temperature >= tb.material->meltingPoint && tb.material->meltingPoint > 0) {
                    tb.temperature = tb.material->meltingPoint;
                    tb.accumulatedLatentHeat += heatToApply;
                    float requiredLatent = mass * tb.material->latentHeatFusion;
                    if (tb.accumulatedLatentHeat >= requiredLatent) {
                        tb.accumulatedLatentHeat = 0.0f;
                        tb.phase = ThermalBody::LIQUID;
                    }
                } else if (tb.phase == ThermalBody::LIQUID && tb.temperature >= tb.material->boilingPoint && tb.material->boilingPoint > 0) {
                    tb.temperature = tb.material->boilingPoint;
                    tb.accumulatedLatentHeat += heatToApply;
                    float requiredLatent = mass * tb.material->latentHeatVap;
                    if (tb.accumulatedLatentHeat >= requiredLatent) {
                        tb.accumulatedLatentHeat = 0.0f;
                        tb.phase = ThermalBody::GAS;
                    }
                } else {
                    float deltaT = PhysicsMath::temperatureChange(totalHeatLoss, mass, tb.material->specificHeat);
                    tb.temperature -= deltaT;
                }
            }

            // Clamp to absolute zero
            if (tb.temperature < 0.0f) tb.temperature = 0.0f;

            // ── Phase transition ────────────────────────────────
            tb.updatePhase();

            // ── Thermal Expansion ───────────────────────────────
            // Delta L = alpha * L0 * Delta T
            // Since Delta T is additive, we calculate Scale = 1.0 + alpha * (T - Tbase)
            if (tb.bodyIndex >= 0 && tb.bodyIndex < static_cast<int>(bodies.size())) {
                RigidBody3D& rb = bodies[tb.bodyIndex];
                float dT = tb.temperature - tb.baseTemperature;
                float scale = 1.0f + (tb.material->thermalExpansionCoeff * dT);
                
                // Ensure scale doesn't crush or explode the object unreasonably in edge cases
                if (scale < 0.1f) scale = 0.1f;
                if (scale > 10.0f) scale = 10.0f;
                
                // Assume base sizes were originally unmodified, scale the bounds
                if (rb.shape == RigidBody3D::Shape::SPHERE) {
                    // Update sphere volume/surface approximation
                    tb.surfaceArea = 4.0f * 3.14159265f * (rb.sphereRadius * scale) * (rb.sphereRadius * scale);
                    // Actual scaling is ideally handled by modifying colliders, but since this
                    // requires tracking original sizes, we just apply the uniform scale effect:
                    // Here we assume scale is roughly 1.0 most of the time unless heated hundreds of degrees.
                }
            }
        }
    }

    /// Apply conduction heat between two thermal bodies in contact
    void conductBetween(int thermalA, int thermalB, float contactArea,
                         float dt, std::vector<RigidBody3D>& bodies) {
        if (thermalA < 0 || thermalA >= static_cast<int>(m_thermalBodies.size())) return;
        if (thermalB < 0 || thermalB >= static_cast<int>(m_thermalBodies.size())) return;

        auto& a = m_thermalBodies[thermalA];
        auto& b = m_thermalBodies[thermalB];
        if (!a.material || !b.material) return;

        float deltaT = a.temperature - b.temperature;
        float heat = PhysicsMath::conductionHeat(
            a.material->thermalConductivity,
            b.material->thermalConductivity,
            contactArea, deltaT, dt * timeScale);

        float massA = 1.0f, massB = 1.0f;
        if (a.bodyIndex >= 0 && a.bodyIndex < static_cast<int>(bodies.size()))
            massA = std::max(bodies[a.bodyIndex].getMass(), 0.01f);
        if (b.bodyIndex >= 0 && b.bodyIndex < static_cast<int>(bodies.size()))
            massB = std::max(bodies[b.bodyIndex].getMass(), 0.01f);

        // Function to apply heat considering latent heat halts
        auto applyHeat = [](ThermalBody& tb, float Q, float mass) {
            float heatToApply = std::abs(Q);
            int sign = (Q > 0) ? 1 : -1;

            if (tb.phase == ThermalBody::SOLID && sign > 0 && tb.temperature >= tb.material->meltingPoint && tb.material->meltingPoint > 0) {
                // Melting
                tb.accumulatedLatentHeat += heatToApply;
                float requiredLatent = mass * tb.material->latentHeatFusion;
                if (tb.accumulatedLatentHeat >= requiredLatent) {
                    tb.accumulatedLatentHeat = 0.0f;
                    tb.phase = ThermalBody::LIQUID;
                    tb.justMelted = true;
                }
            } else if (tb.phase == ThermalBody::LIQUID && sign > 0 && tb.temperature >= tb.material->boilingPoint && tb.material->boilingPoint > 0) {
                // Boiling
                tb.accumulatedLatentHeat += heatToApply;
                float requiredLatent = mass * tb.material->latentHeatVap;
                if (tb.accumulatedLatentHeat >= requiredLatent) {
                    tb.accumulatedLatentHeat = 0.0f;
                    tb.phase = ThermalBody::GAS;
                }
            } else if (tb.phase == ThermalBody::LIQUID && sign < 0 && tb.temperature <= tb.material->meltingPoint && tb.material->meltingPoint > 0) {
                // Freezing
                tb.accumulatedLatentHeat += heatToApply;
                float requiredLatent = mass * tb.material->latentHeatFusion;
                if (tb.accumulatedLatentHeat >= requiredLatent) {
                    tb.accumulatedLatentHeat = 0.0f;
                    tb.phase = ThermalBody::SOLID;
                }
            } else if (tb.phase == ThermalBody::GAS && sign < 0 && tb.temperature <= tb.material->boilingPoint && tb.material->boilingPoint > 0) {
                // Condensing
                tb.accumulatedLatentHeat += heatToApply;
                float requiredLatent = mass * tb.material->latentHeatVap;
                if (tb.accumulatedLatentHeat >= requiredLatent) {
                    tb.accumulatedLatentHeat = 0.0f;
                    tb.phase = ThermalBody::LIQUID;
                }
            } else {
                // Normal temperature change
                tb.temperature += sign * PhysicsMath::temperatureChange(heatToApply, mass, tb.material->specificHeat);
            }
        };

        // Heat flows from hot (A) to cold (B)
        applyHeat(a, -heat, massA);
        applyHeat(b, heat, massB);

        a.updatePhase();
        b.updatePhase();
    }

    /// Apply friction heat to a thermal body
    void applyFrictionHeat(int thermalIndex, float frictionForce,
                            float slidingSpeed, float dt,
                            const std::vector<RigidBody3D>& bodies) {
        if (thermalIndex < 0 || thermalIndex >= static_cast<int>(m_thermalBodies.size())) return;
        auto& tb = m_thermalBodies[thermalIndex];
        if (!tb.material) return;

        float heat = PhysicsMath::frictionHeat(frictionForce, slidingSpeed, dt * timeScale);
        float mass = 1.0f;
        if (tb.bodyIndex >= 0 && tb.bodyIndex < static_cast<int>(bodies.size()))
            mass = std::max(bodies[tb.bodyIndex].getMass(), 0.01f);

        tb.temperature += PhysicsMath::temperatureChange(
            heat * 0.5f, mass, tb.material->specificHeat); // 50% to each body
        tb.updatePhase();
    }

    /// Inject heat energy (Joules) directly into a thermal body.
    /// Used by ElectricalSystem (Joule heating) and other external couplings.
    void injectHeat(int thermalIndex, float joules, const std::vector<RigidBody3D>& bodies) {
        if (thermalIndex < 0 || thermalIndex >= static_cast<int>(m_thermalBodies.size())) return;
        auto& tb = m_thermalBodies[thermalIndex];
        if (!tb.material) return;

        float mass = 1.0f;
        if (tb.bodyIndex >= 0 && tb.bodyIndex < static_cast<int>(bodies.size()))
            mass = std::max(bodies[tb.bodyIndex].getMass(), 0.01f);

        tb.temperature += PhysicsMath::temperatureChange(joules, mass, tb.material->specificHeat);
        tb.updatePhase();
    }

    // ── Accessors ───────────────────────────────────────────────
    int getCount() const { return static_cast<int>(m_thermalBodies.size()); }
    ThermalBody& get(int index) { return m_thermalBodies[index]; }
    const ThermalBody& get(int index) const { return m_thermalBodies[index]; }

    ThermalBody* findByBody(int bodyIndex) {
        for (auto& tb : m_thermalBodies)
            if (tb.bodyIndex == bodyIndex) return &tb;
        return nullptr;
    }

private:
    std::vector<ThermalBody> m_thermalBodies;
};

} // namespace physics
} // namespace engine
