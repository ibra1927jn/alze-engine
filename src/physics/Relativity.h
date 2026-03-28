#pragma once

#include "math/Vector3D.h"
#include <cmath>

namespace engine {
namespace physics {

// ═══════════════════════════════════════════════════════════════
// Special Relativity — Einstein's theory for near-c velocities
//
// All formulas use SI units unless noted.
// At speeds << c, all formulas reduce to Newtonian mechanics.
// ═══════════════════════════════════════════════════════════════

namespace RelativityConstants {
    constexpr double C            = 2.99792458e8;     // Speed of light (m/s)
    constexpr double C_SQUARED    = 8.9875517873681764e16; // c² (m²/s²)
    constexpr double ELECTRON_MASS = 9.1093837015e-31; // kg
    constexpr double PROTON_MASS   = 1.67262192369e-27; // kg
}

namespace RelativityMath {

    /// Lorentz factor: γ = 1/√(1-(v/c)²)
    /// Returns 1.0 for v=0, approaches ∞ as v→c
    inline double lorentzFactor(double speed_ms) {
        double beta = speed_ms / RelativityConstants::C;
        double betaSq = beta * beta;
        if (betaSq >= 1.0) return 1e15; // Prevent infinity
        return 1.0 / std::sqrt(1.0 - betaSq);
    }

    /// Beta: β = v/c (dimensionless, 0 to 1)
    inline double beta(double speed_ms) {
        return speed_ms / RelativityConstants::C;
    }

    /// Time dilation: t' = γ·t₀
    /// t0 = proper time (in rest frame), returns dilated time
    inline double timeDilation(double properTime_s, double speed_ms) {
        return lorentzFactor(speed_ms) * properTime_s;
    }

    /// Length contraction: L = L₀/γ
    /// L0 = proper length (at rest), returns contracted length
    inline double lengthContraction(double properLength_m, double speed_ms) {
        return properLength_m / lorentzFactor(speed_ms);
    }

    /// Relativistic mass: m = γ·m₀
    inline double relativisticMass(double restMass_kg, double speed_ms) {
        return lorentzFactor(speed_ms) * restMass_kg;
    }

    /// Relativistic momentum: p = γmv
    inline double relativisticMomentum(double restMass_kg, double speed_ms) {
        return lorentzFactor(speed_ms) * restMass_kg * speed_ms;
    }

    /// Relativistic kinetic energy: KE = (γ-1)mc²
    inline double relativisticKineticEnergy(double restMass_kg, double speed_ms) {
        double gamma = lorentzFactor(speed_ms);
        return (gamma - 1.0) * restMass_kg * RelativityConstants::C_SQUARED;
    }

    /// Total energy: E = γmc² = KE + mc²
    inline double totalEnergy(double restMass_kg, double speed_ms) {
        return lorentzFactor(speed_ms) * restMass_kg * RelativityConstants::C_SQUARED;
    }

    /// Rest energy: E₀ = mc² (the famous equation)
    inline double restEnergy(double restMass_kg) {
        return restMass_kg * RelativityConstants::C_SQUARED;
    }

    /// Energy-momentum relation: E² = (pc)² + (mc²)²
    /// Given total energy and rest mass, find momentum magnitude
    inline double momentumFromEnergy(double totalEnergy_J, double restMass_kg) {
        double E0 = restEnergy(restMass_kg);
        double pSquared = (totalEnergy_J * totalEnergy_J - E0 * E0);
        if (pSquared < 0.0) return 0.0;
        return std::sqrt(pSquared) / RelativityConstants::C;
    }

    /// Speed from kinetic energy (relativistic)
    /// KE = (γ-1)mc² → γ = 1 + KE/(mc²) → v = c√(1-1/γ²)
    inline double speedFromKineticEnergy(double KE_J, double restMass_kg) {
        double E0 = restEnergy(restMass_kg);
        double gamma = 1.0 + KE_J / E0;
        if (gamma <= 1.0) return 0.0;
        return RelativityConstants::C * std::sqrt(1.0 - 1.0 / (gamma * gamma));
    }

    /// Relativistic velocity addition: u' = (u + v) / (1 + uv/c²)
    /// u, v: velocities of two frames (1D, same direction)
    inline double velocityAddition(double u, double v) {
        return (u + v) / (1.0 + u * v / RelativityConstants::C_SQUARED);
    }

    /// Relativistic Doppler effect (longitudinal)
    /// f_observed = f_source · √((1+β)/(1-β))  (approaching)
    /// β = v/c (positive = approaching)
    inline double relativisticDoppler(double f_source, double speed_ms, bool approaching) {
        double b = beta(speed_ms);
        if (b >= 1.0) b = 0.9999;
        if (approaching)
            return f_source * std::sqrt((1.0 + b) / (1.0 - b));
        else
            return f_source * std::sqrt((1.0 - b) / (1.0 + b));
    }

    /// Gravitational redshift (from Schwarzschild metric, weak field)
    /// Δf/f = gh/c²  (h = height difference, g = gravitational accel)
    inline double gravitationalRedshift(double frequency, double g, double heightDiff) {
        return frequency * (1.0 - g * heightDiff / RelativityConstants::C_SQUARED);
    }

    /// Mass-energy equivalence: mass from energy
    inline double massFromEnergy(double energy_J) {
        return energy_J / RelativityConstants::C_SQUARED;
    }

} // namespace RelativityMath

} // namespace physics
} // namespace engine
