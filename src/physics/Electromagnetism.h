#pragma once

#include "math/Vector3D.h"
#include "RigidBody3D.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include <functional>

namespace engine {
namespace physics {

// ═══════════════════════════════════════════════════════════════
// Electromagnetic Constants (SI units)
// ═══════════════════════════════════════════════════════════════

namespace EMConstants {
    constexpr float COULOMB_K         = 8.9875517873681764e9f;  // N·m²/C² (Coulomb constant)
    constexpr float VACUUM_PERMITTIVITY = 8.8541878128e-12f;    // ε₀ F/m
    constexpr float VACUUM_PERMEABILITY = 1.25663706212e-6f;    // μ₀ H/m
    constexpr float ELEMENTARY_CHARGE   = 1.602176634e-19f;     // e (Coulombs)
    constexpr float ELECTRON_MASS       = 9.1093837015e-31f;    // kg
    constexpr float PROTON_MASS         = 1.67262192369e-27f;   // kg
    constexpr float SPEED_OF_LIGHT      = 299792458.0f;         // m/s
    constexpr float PI = 3.14159265358979f;
} // namespace EMConstants

// ═══════════════════════════════════════════════════════════════
// EM Math — Core electromagnetic equations
// ═══════════════════════════════════════════════════════════════

namespace EMMath {

    /// Coulomb force on charge q1 due to charge q2
    /// F = kₑ · q₁·q₂ / r² · r̂  (repulsive if same sign)
    inline math::Vector3D coulombForce(float q1, float q2,
                                        const math::Vector3D& r12) {
        float rSq = r12.sqrMagnitude();
        if (rSq < 1e-10f) return math::Vector3D::Zero; // Prevent singularity
        float rLen = std::sqrt(rSq);
        float F = EMConstants::COULOMB_K * q1 * q2 / rSq;
        return r12 * (F / rLen); // Direction: 12 → pushes 1 away from 2 if same sign
    }

    /// Electric field at point due to point charge q at origin
    /// E = kₑ · q / r² · r̂
    inline math::Vector3D electricField(float q, const math::Vector3D& r) {
        float rSq = r.sqrMagnitude();
        if (rSq < 1e-10f) return math::Vector3D::Zero;
        float rLen = std::sqrt(rSq);
        return r * (EMConstants::COULOMB_K * q / (rSq * rLen));
    }

    /// Lorentz force: F = q(E + v × B)
    inline math::Vector3D lorentzForce(float q,
                                        const math::Vector3D& E,
                                        const math::Vector3D& v,
                                        const math::Vector3D& B) {
        return (E + v.cross(B)) * q;
    }

    /// Magnetic field of a dipole at position r (from dipole at origin with moment m)
    /// B(r) = (μ₀/4π) · [3(m·r̂)r̂ - m] / r³
    inline math::Vector3D dipoleMagneticField(const math::Vector3D& moment,
                                               const math::Vector3D& r) {
        float rSq = r.sqrMagnitude();
        if (rSq < 1e-10f) return math::Vector3D::Zero;
        float rLen = std::sqrt(rSq);
        float r3 = rLen * rLen * rLen;
        math::Vector3D rhat = r * (1.0f / rLen);
        float mdotr = moment.dot(rhat);
        float coeff = EMConstants::VACUUM_PERMEABILITY / (4.0f * EMConstants::PI * r3);
        return (rhat * (3.0f * mdotr) - moment) * coeff;
    }

    /// Electric potential energy: U = kₑ · q₁·q₂ / r
    inline float electricPotentialEnergy(float q1, float q2, float distance) {
        if (distance < 1e-6f) return 0.0f;
        return EMConstants::COULOMB_K * q1 * q2 / distance;
    }

    /// Magnetic force on a current-carrying wire in a field
    /// F = I · L × B  (I = current in amps, L = wire direction vector)
    inline math::Vector3D wireForceInField(float current,
                                            const math::Vector3D& wireDir,
                                            const math::Vector3D& B) {
        return wireDir.cross(B) * current;
    }

    /// Faraday EMF: ε = -dΦ/dt  (Φ = B·A = magnetic flux)
    /// Returns induced voltage given change in flux over time
    inline float faradayEMF(float dFlux_dt) {
        return -dFlux_dt;
    }

    /// Cyclotron radius: r = mv / (|q|B)
    inline float cyclotronRadius(float mass, float speed, float charge, float B) {
        float qB = std::abs(charge) * B;
        if (qB < 1e-20f) return 1e20f; // No field → infinite radius
        return mass * speed / qB;
    }

    /// Cyclotron frequency: ω = |q|B / m
    inline float cyclotronFrequency(float charge, float B, float mass) {
        if (mass < 1e-30f) return 0.0f;
        return std::abs(charge) * B / mass;
    }

} // namespace EMMath

// ═══════════════════════════════════════════════════════════════
// ChargedBody — Electric charge associated with a RigidBody3D
// ═══════════════════════════════════════════════════════════════

struct ChargedBody {
    int bodyIndex = -1;          // Index into PhysicsWorld3D::m_bodies
    float charge = 0.0f;         // Coulombs (positive or negative)
    float magneticMoment = 0.0f; // |m| for magnetic dipole (A·m²)
    math::Vector3D magneticAxis = math::Vector3D(0, 1, 0); // Direction of magnetic dipole

    /// Get the magnetic moment vector
    math::Vector3D getMomentVector() const {
        return magneticAxis.normalized() * magneticMoment;
    }
};

// ═══════════════════════════════════════════════════════════════
// MagneticField — Region with a magnetic field
// ═══════════════════════════════════════════════════════════════

struct MagneticFieldRegion {
    enum Type : uint8_t { UNIFORM, DIPOLE };
    Type type = UNIFORM;

    // Uniform field
    math::Vector3D fieldStrength = math::Vector3D(0, 0, 1); // Tesla (direction + magnitude)

    // AABB bounds for uniform field (infinite if not set)
    math::Vector3D boundsMin = math::Vector3D(-1e6f, -1e6f, -1e6f);
    math::Vector3D boundsMax = math::Vector3D(1e6f, 1e6f, 1e6f);

    // Dipole source
    math::Vector3D dipolePosition;
    math::Vector3D dipoleMoment; // A·m²

    /// Get field at a point
    math::Vector3D getFieldAt(const math::Vector3D& pos) const {
        if (type == UNIFORM) {
            // Check bounds
            if (pos.x < boundsMin.x || pos.x > boundsMax.x ||
                pos.y < boundsMin.y || pos.y > boundsMax.y ||
                pos.z < boundsMin.z || pos.z > boundsMax.z)
                return math::Vector3D::Zero;
            return fieldStrength;
        }
        // Dipole
        return EMMath::dipoleMagneticField(dipoleMoment, pos - dipolePosition);
    }
};

// ═══════════════════════════════════════════════════════════════
// EMSystem — Electromagnetic force solver
//
// Computes electrostatic (Coulomb) and magnetostatic (Lorentz)
// forces on charged rigid bodies.
//
// Pipeline per step:
//   1. Compute electric field at each body from all other charges
//   2. Compute total magnetic field (regions + dipoles)
//   3. Apply Lorentz force: F = q(E + v × B)
//   4. Optionally compute induced EMF (Faraday)
// ═══════════════════════════════════════════════════════════════

class EMSystem {
public:
    float softening = 0.01f;  // Prevent singularity at r→0 (meters)

    // ── Charge Management ───────────────────────────────────────

    int addChargedBody(int bodyIndex, float charge, float magneticMoment = 0.0f) {
        ChargedBody cb;
        cb.bodyIndex = bodyIndex;
        cb.charge = charge;
        cb.magneticMoment = magneticMoment;
        int idx = static_cast<int>(m_charges.size());
        m_charges.push_back(cb);
        return idx;
    }

    int addMagneticField(const MagneticFieldRegion& region) {
        int idx = static_cast<int>(m_fields.size());
        m_fields.push_back(region);
        return idx;
    }

    /// Add a uniform magnetic field covering all space
    int addUniformField(const math::Vector3D& B) {
        MagneticFieldRegion r;
        r.type = MagneticFieldRegion::UNIFORM;
        r.fieldStrength = B;
        return addMagneticField(r);
    }

    // ── Simulation Step ─────────────────────────────────────────

    void step(float dt, std::vector<RigidBody3D>& bodies) {
        int n = static_cast<int>(m_charges.size());
        float softSq = softening * softening;

        for (int i = 0; i < n; i++) {
            auto& ci = m_charges[i];
            if (ci.bodyIndex < 0 || ci.bodyIndex >= static_cast<int>(bodies.size())) continue;
            if (bodies[ci.bodyIndex].m_removed) continue;

            auto& bodyI = bodies[ci.bodyIndex];
            math::Vector3D totalForce = math::Vector3D::Zero;

            // ── Coulomb force from all other charges ──────────
            math::Vector3D E_total = math::Vector3D::Zero;
            for (int j = 0; j < n; j++) {
                if (j == i) continue;
                auto& cj = m_charges[j];
                if (cj.bodyIndex < 0 || cj.bodyIndex >= static_cast<int>(bodies.size())) continue;
                if (bodies[cj.bodyIndex].m_removed) continue;

                math::Vector3D r = bodyI.position - bodies[cj.bodyIndex].position;
                float rSq = r.sqrMagnitude() + softSq;
                float rLen = std::sqrt(rSq);

                // E field from charge j at position of i
                E_total += r * (EMConstants::COULOMB_K * cj.charge / (rSq * rLen));
            }

            // ── Total magnetic field at body position ─────────
            math::Vector3D B_total = math::Vector3D::Zero;
            for (const auto& field : m_fields) {
                B_total += field.getFieldAt(bodyI.position);
            }
            // Add dipole fields from other magnetic charges
            for (int j = 0; j < n; j++) {
                if (j == i || m_charges[j].magneticMoment < 1e-10f) continue;
                if (m_charges[j].bodyIndex < 0 || m_charges[j].bodyIndex >= static_cast<int>(bodies.size())) continue;
                if (bodies[m_charges[j].bodyIndex].m_removed) continue;

                math::Vector3D r = bodyI.position - bodies[m_charges[j].bodyIndex].position;
                B_total += EMMath::dipoleMagneticField(m_charges[j].getMomentVector(), r);
            }

            // ── Lorentz force: F = q(E + v × B) ──────────────
            totalForce = EMMath::lorentzForce(ci.charge, E_total, bodyI.velocity, B_total);

            // Apply force
            bodyI.applyForce(totalForce);
        }
    }

    // ── Accessors ───────────────────────────────────────────────
    int getChargeCount() const { return static_cast<int>(m_charges.size()); }
    ChargedBody& getCharge(int i) { return m_charges[i]; }
    const ChargedBody& getCharge(int i) const { return m_charges[i]; }

    int getFieldCount() const { return static_cast<int>(m_fields.size()); }

    ChargedBody* findByBody(int bodyIndex) {
        for (auto& cb : m_charges)
            if (cb.bodyIndex == bodyIndex) return &cb;
        return nullptr;
    }

    /// Total electrostatic potential energy of the system
    float getTotalPotentialEnergy(const std::vector<RigidBody3D>& bodies) const {
        float U = 0;
        int n = static_cast<int>(m_charges.size());
        for (int i = 0; i < n; i++) {
            for (int j = i + 1; j < n; j++) {
                if (m_charges[i].bodyIndex < 0 || m_charges[j].bodyIndex < 0) continue;
                if (m_charges[i].bodyIndex >= static_cast<int>(bodies.size())) continue;
                if (m_charges[j].bodyIndex >= static_cast<int>(bodies.size())) continue;
                float r = (bodies[m_charges[i].bodyIndex].position -
                           bodies[m_charges[j].bodyIndex].position).magnitude();
                U += EMMath::electricPotentialEnergy(m_charges[i].charge, m_charges[j].charge, r);
            }
        }
        return U;
    }

private:
    std::vector<ChargedBody> m_charges;
    std::vector<MagneticFieldRegion> m_fields;
};

} // namespace physics
} // namespace engine
