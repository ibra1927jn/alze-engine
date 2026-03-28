#pragma once

#include "RigidBody3D.h"
#include "math/Vector3D.h"
#include <vector>
#include <functional>
#include <cmath>

namespace engine {
namespace physics {

// ═══════════════════════════════════════════════════════════════
// Material Strength Properties
// ═══════════════════════════════════════════════════════════════

struct MaterialStrength {
    std::string name;
    
    // Elastic modulus (Pa)
    float youngsModulus  = 200e9f;   // E — resistance to deformation
    float poissonRatio   = 0.3f;     // ν — lateral contraction ratio
    
    // Failure thresholds (Pa)
    float yieldStrength  = 250e6f;   // σ_y — onset of plastic deformation (Von Mises)
    float ultimateTensile = 400e6f;  // UTS — max stress before fracture
    float fractureToughness = 50e6f; // K_Ic — J/m² (Mode-I fracture toughness)

    // ── Presets ─────────────────────────────────────────────────
    static MaterialStrength Steel() {
        MaterialStrength m; m.name = "Steel";
        m.youngsModulus = 200e9f; m.poissonRatio = 0.3f;
        m.yieldStrength = 250e6f; m.ultimateTensile = 400e6f;
        m.fractureToughness = 50e6f;
        return m;
    }
    static MaterialStrength Glass() {
        MaterialStrength m; m.name = "Glass";
        m.youngsModulus = 70e9f; m.poissonRatio = 0.23f;
        m.yieldStrength = 50e6f; m.ultimateTensile = 50e6f; // Brittle — no plastic range
        m.fractureToughness = 0.7e6f;
        return m;
    }
    static MaterialStrength Concrete() {
        MaterialStrength m; m.name = "Concrete";
        m.youngsModulus = 30e9f; m.poissonRatio = 0.2f;
        m.yieldStrength = 25e6f; m.ultimateTensile = 30e6f;
        m.fractureToughness = 0.5e6f;
        return m;
    }
    static MaterialStrength Bone() {
        MaterialStrength m; m.name = "Bone";
        m.youngsModulus = 17e9f; m.poissonRatio = 0.3f;
        m.yieldStrength = 130e6f; m.ultimateTensile = 150e6f;
        m.fractureToughness = 6e6f;
        return m;
    }
    static MaterialStrength Ice() {
        MaterialStrength m; m.name = "Ice";
        m.youngsModulus = 9e9f; m.poissonRatio = 0.33f;
        m.yieldStrength = 5e6f; m.ultimateTensile = 5e6f; // Very brittle
        m.fractureToughness = 0.1e6f;
        return m;
    }
    static MaterialStrength Rubber() {
        MaterialStrength m; m.name = "Rubber";
        m.youngsModulus = 0.05e9f; m.poissonRatio = 0.49f;
        m.yieldStrength = 10e6f; m.ultimateTensile = 20e6f;
        m.fractureToughness = 0.01e6f;
        return m;
    }
};

// ═══════════════════════════════════════════════════════════════
// Stress State — Cauchy Stress Tensor (symmetric 3×3)
//
//   σ = | σxx  τxy  τxz |
//       | τxy  σyy  τyz |
//       | τxz  τyz  σzz |
//
// Stored as 6 independent floats.
// ═══════════════════════════════════════════════════════════════

struct StressState {
    float sxx = 0, syy = 0, szz = 0;  // Normal stresses (Pa)
    float txy = 0, txz = 0, tyz = 0;  // Shear stresses (Pa)

    /// Von Mises equivalent stress σ_vm = √(½·[(σ1-σ2)²+(σ2-σ3)²+(σ3-σ1)²])
    /// Using principal stress approximation from Cauchy tensor invariants.
    float vonMises() const {
        float sx = sxx - syy;
        float sy = syy - szz;
        float sz = szz - sxx;
        return std::sqrt(0.5f * (sx*sx + sy*sy + sz*sz) + 3.0f * (txy*txy + txz*txz + tyz*tyz));
    }

    /// Hydrostatic stress (mean normal stress = pressure)
    float hydrostatic() const { return (sxx + syy + szz) / 3.0f; }
};

// ═══════════════════════════════════════════════════════════════
// FractureBody — A breakable body attached to a RigidBody3D
// ═══════════════════════════════════════════════════════════════

struct FractureBody {
    int rigidBodyIndex = -1;
    MaterialStrength material;
    StressState stress;
    bool hasYielded = false;    // Plastic deformation has occurred
    bool hasFractured = false;  // Body has broken
    float damage = 0.0f;        // 0 = pristine, 1 = fractured
};

// ═══════════════════════════════════════════════════════════════
// Fracture Math
// ═══════════════════════════════════════════════════════════════

namespace FractureMath {

    /// Compute stress from applied force on a body surface.
    /// F: applied force (N), area: cross-section area (m²), direction: unit normal
    inline StressState fromForce(const math::Vector3D& F, float area,
                                 const math::Vector3D& direction) {
        if (area <= 0.0f) return {};

        // Simple uniaxial stress state in the direction of the force
        StressState s;
        // Decompose force into normal + shear components
        float fNorm = F.dot(direction);
        math::Vector3D fShear = F - direction * fNorm;

        // Normal stress in direction
        float sigma = fNorm / area;
        s.sxx = sigma * direction.x * direction.x;
        s.syy = sigma * direction.y * direction.y;
        s.szz = sigma * direction.z * direction.z;
        s.txy = (fShear.x * direction.y + fShear.y * direction.x) / (2.0f * area);
        s.txz = (fShear.x * direction.z + fShear.z * direction.x) / (2.0f * area);
        s.tyz = (fShear.y * direction.z + fShear.z * direction.y) / (2.0f * area);
        return s;
    }

    /// Elastic strain from stress via Hooke's law: ε = σ/E (simplified scalar)
    inline float elasticStrain(float sigma, float E) {
        return (E > 0.0f) ? sigma / E : 0.0f;
    }

    /// Damage accumulation rate (normalized)
    /// Based on ratio exceedance of yield stress
    inline float damageRate(float vonMisesStress, float yieldStress, float uts) {
        if (vonMisesStress <= yieldStress) return 0.0f;
        return (vonMisesStress - yieldStress) / (uts - yieldStress + 1.0f);
    }

} // namespace FractureMath

// ═══════════════════════════════════════════════════════════════
// FractureSystem — Manages breakable bodies
// ═══════════════════════════════════════════════════════════════

class FractureSystem {
public:
    /// Register a body as fracture-capable
    int attachToBody(int rigidBodyIndex, const MaterialStrength& mat) {
        FractureBody fb;
        fb.rigidBodyIndex = rigidBodyIndex;
        fb.material = mat;
        int idx = static_cast<int>(m_bodies.size());
        m_bodies.push_back(fb);
        return idx;
    }

    /// Apply force to fracture body — updates internal stress and damage
    void applyForce(int fractureBodyIdx, const math::Vector3D& force,
                    float crossSectionArea, const math::Vector3D& direction) {
        if (fractureBodyIdx < 0 || fractureBodyIdx >= (int)m_bodies.size()) return;
        auto& fb = m_bodies[fractureBodyIdx];

        fb.stress = FractureMath::fromForce(force, crossSectionArea, direction);

        float vm = fb.stress.vonMises();
        float dr = FractureMath::damageRate(vm, fb.material.yieldStrength,
                                            fb.material.ultimateTensile);
        fb.damage = std::min(1.0f, fb.damage + dr * 0.016f); // Accumulate per-frame

        if (vm >= fb.material.yieldStrength && !fb.hasYielded) {
            fb.hasYielded = true;
        }
        if (fb.damage >= 1.0f && !fb.hasFractured) {
            fb.hasFractured = true;
            if (onFracture) onFracture(fb.rigidBodyIndex, fb.material);
        }
    }

    /// Step all fracture bodies (stress relaxation over time)
    void step(float dt) {
        for (auto& fb : m_bodies) {
            if (fb.hasFractured) continue;
            // Stress relaxation: stresses decay towards zero (creep/viscoelastic)
            float relax = std::exp(-dt * 2.0f); // ~0.5s relaxation time constant
            fb.stress.sxx *= relax; fb.stress.syy *= relax; fb.stress.szz *= relax;
            fb.stress.txy *= relax; fb.stress.txz *= relax; fb.stress.tyz *= relax;
        }
    }

    const FractureBody& getBody(int i) const { return m_bodies[i]; }
    FractureBody& getBody(int i) { return m_bodies[i]; }
    int getBodyCount() const { return static_cast<int>(m_bodies.size()); }

    /// Fired when a body fractures: (rigidBodyIndex, material)
    std::function<void(int, const MaterialStrength&)> onFracture;

private:
    std::vector<FractureBody> m_bodies;
};

} // namespace physics
} // namespace engine
