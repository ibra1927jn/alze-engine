#pragma once

#include "math/Vector3D.h"
#include <cmath>
#include <algorithm>

namespace engine {
namespace physics {

// ═══════════════════════════════════════════════════════════════
// Advanced Friction Models
//
// Extends basic Coulomb friction with:
//   - 2D contact patch friction (tangent + bitangent)
//   - Anisotropic friction (direction-dependent μ)
//   - Rolling resistance with speed-dependent coefficient
//   - Tyre/wheel friction (Pacejka Magic Formula)
// ═══════════════════════════════════════════════════════════════

namespace FrictionMath {

    /// 2D Contact Friction — projects relative velocity onto
    /// tangent plane and applies friction in both axes independently
    struct FrictionResult2D {
        math::Vector3D impulse;         // Total friction impulse to apply
        math::Vector3D tangent1;        // Primary friction direction
        math::Vector3D tangent2;        // Secondary friction direction
        float slip1 = 0.0f;            // Slip speed along tangent1
        float slip2 = 0.0f;            // Slip speed along tangent2
    };

    /// Build orthonormal tangent frame from contact normal
    inline void buildTangentFrame(const math::Vector3D& normal,
                                   math::Vector3D& tangent1,
                                   math::Vector3D& tangent2) {
        // Choose axis least parallel to normal
        math::Vector3D ref = (std::abs(normal.y) < 0.9f)
            ? math::Vector3D(0, 1, 0)
            : math::Vector3D(1, 0, 0);
        tangent1 = normal.cross(ref).normalized();
        tangent2 = normal.cross(tangent1).normalized();
    }

    /// Compute 2D friction impulse on tangent plane
    /// relVel: relative velocity at contact point
    /// normal: contact normal
    /// normalImpulse: magnitude of normal impulse (result of collision)
    /// mu_s: static friction coefficient
    /// mu_k: kinetic friction coefficient
    inline FrictionResult2D computeFriction2D(
        const math::Vector3D& relVel,
        const math::Vector3D& normal,
        float normalImpulse,
        float mu_s, float mu_k)
    {
        FrictionResult2D result;
        buildTangentFrame(normal, result.tangent1, result.tangent2);

        result.slip1 = relVel.dot(result.tangent1);
        result.slip2 = relVel.dot(result.tangent2);

        float slipMag = std::sqrt(result.slip1 * result.slip1 + result.slip2 * result.slip2);

        if (slipMag < 1e-6f) {
            result.impulse = math::Vector3D::Zero;
            return result;
        }

        // Determine static vs kinetic
        float maxFriction = (slipMag < 0.01f) ? mu_s * normalImpulse : mu_k * normalImpulse;

        // Apply friction proportionally to slip in each tangent direction
        float frictionPerSlip = maxFriction / slipMag;
        float f1 = -result.slip1 * frictionPerSlip;
        float f2 = -result.slip2 * frictionPerSlip;

        result.impulse = result.tangent1 * f1 + result.tangent2 * f2;
        return result;
    }

    /// Anisotropic friction — different μ along tangent axes
    /// mu_primary: friction along primary axis (e.g., wood grain direction)
    /// mu_secondary: friction across primary axis
    inline FrictionResult2D computeAnisotropicFriction(
        const math::Vector3D& relVel,
        const math::Vector3D& normal,
        const math::Vector3D& primaryAxis,  // Direction of anisotropy (e.g., grain)
        float normalImpulse,
        float mu_primary, float mu_secondary)
    {
        FrictionResult2D result;
        // Project primaryAxis onto tangent plane
        result.tangent1 = (primaryAxis - normal * primaryAxis.dot(normal)).normalized();
        result.tangent2 = normal.cross(result.tangent1).normalized();

        result.slip1 = relVel.dot(result.tangent1);
        result.slip2 = relVel.dot(result.tangent2);

        // Independent friction limits per axis
        float f1 = std::max(-mu_primary * normalImpulse,
                   std::min(mu_primary * normalImpulse, -result.slip1));
        float f2 = std::max(-mu_secondary * normalImpulse,
                   std::min(mu_secondary * normalImpulse, -result.slip2));

        result.impulse = result.tangent1 * f1 + result.tangent2 * f2;
        return result;
    }

    /// Rolling resistance: F_roll = C_rr * N * sign(v)
    /// C_rr depends on speed: low at low speed, increases with deformation
    inline float rollingResistanceForce(float normalForce, float speed,
                                         float crr_base = 0.01f,
                                         float crr_speed_factor = 0.0001f) {
        float crr = crr_base + crr_speed_factor * std::abs(speed);
        return crr * normalForce;
    }

    /// Pacejka "Magic Formula" for tyre friction
    /// Widely used in racing simulators for lateral/longitudinal tyre forces
    /// F = D · sin(C · arctan(B·slip - E·(B·slip - arctan(B·slip))))
    /// B: stiffness factor, C: shape factor, D: peak value, E: curvature
    struct PacejkaParams {
        float B = 10.0f;   // Stiffness
        float C = 1.9f;    // Shape (1.9 = lateral, 1.65 = longitudinal)
        float D = 1.0f;    // Peak (normalized to normal force)
        float E = 0.97f;   // Curvature

        static PacejkaParams DryRoad() { return {10.0f, 1.9f, 1.0f, 0.97f}; }
        static PacejkaParams WetRoad() { return {8.0f, 1.7f, 0.7f, 0.90f}; }
        static PacejkaParams Ice()     { return {4.0f, 2.0f, 0.1f, 1.0f}; }
        static PacejkaParams Gravel()  { return {5.0f, 1.6f, 0.6f, 0.80f}; }
    };

    inline float pacejkaForce(float slipAngle_rad, float normalForce,
                               const PacejkaParams& p = PacejkaParams::DryRoad()) {
        float x = p.B * slipAngle_rad;
        float y = std::sin(p.C * std::atan(x - p.E * (x - std::atan(x))));
        return p.D * normalForce * y;
    }

} // namespace FrictionMath

// ═══════════════════════════════════════════════════════════════
// Surface Material — describes friction properties per-surface
// ═══════════════════════════════════════════════════════════════

struct SurfaceFriction {
    float mu_static  = 0.5f;
    float mu_kinetic = 0.4f;
    bool  isAnisotropic = false;
    math::Vector3D anisotropyDirection = math::Vector3D(1, 0, 0);
    float mu_primary   = 0.5f;  // Along anisotropy direction
    float mu_secondary = 0.5f;  // Perpendicular

    static SurfaceFriction WoodGrain() {
        SurfaceFriction s; s.isAnisotropic = true;
        s.mu_primary = 0.3f; s.mu_secondary = 0.6f;
        s.anisotropyDirection = math::Vector3D(1, 0, 0);
        return s;
    }
    static SurfaceFriction BrushedMetal() {
        SurfaceFriction s; s.isAnisotropic = true;
        s.mu_primary = 0.2f; s.mu_secondary = 0.5f;
        return s;
    }
    static SurfaceFriction ScratchedIce() {
        SurfaceFriction s; s.isAnisotropic = true;
        s.mu_primary = 0.02f; s.mu_secondary = 0.08f;
        return s;
    }
    static SurfaceFriction Rubber()  { return {0.9f, 0.8f}; }
    static SurfaceFriction Teflon()  { return {0.04f, 0.04f}; }
    static SurfaceFriction Asphalt() { return {0.7f, 0.6f}; }
};

} // namespace physics
} // namespace engine
