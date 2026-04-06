#pragma once

#include "math/Vector3D.h"
#include <cmath>
#include <vector>
#include <algorithm>

namespace engine {
namespace physics {

// ═══════════════════════════════════════════════════════════════
// Compressible Flow — Euler equations for compressible gas dynamics
//
// Models shock waves, sonic booms, explosions, and supersonic flow.
// Uses Euler equations (inviscid compressible Navier-Stokes):
//   ∂ρ/∂t + ∇·(ρv) = 0                    (mass conservation)
//   ∂(ρv)/∂t + ∇·(ρv⊗v + pI) = ρg         (momentum)
//   ∂E/∂t + ∇·((E+p)v) = ρg·v             (energy)
//
// Uses particle-based SPH approach for compressible gas.
// ═══════════════════════════════════════════════════════════════

namespace CompressibleConstants {
    constexpr float GAMMA_AIR = 1.4f;         // Adiabatic index for air
    constexpr float GAMMA_MONATOMIC = 5.0f / 3.0f; // He, Ar
    constexpr float GAMMA_DIATOMIC = 7.0f / 5.0f;  // N2, O2
    constexpr float R_SPECIFIC_AIR = 287.058f; // J/(kg·K) specific gas constant for air
    constexpr float P_ATM = 101325.0f;         // Pa
    constexpr float T_STANDARD = 293.15f;      // K
    constexpr float RHO_AIR = 1.225f;          // kg/m³ at sea level
}

namespace CompressibleMath {

    /// Speed of sound in ideal gas: c = √(γRT) or c = √(γP/ρ)
    inline float speedOfSound(float gamma, float temperature_K) {
        return std::sqrt(gamma * CompressibleConstants::R_SPECIFIC_AIR * temperature_K);
    }

    inline float speedOfSoundFromPressure(float gamma, float pressure, float density) {
        if (density <= 0.0f) return 0.0f;
        return std::sqrt(gamma * pressure / density);
    }

    /// Mach number: M = v / c
    inline float machNumber(float velocity, float speedOfSound) {
        return (speedOfSound > 0.0f) ? velocity / speedOfSound : 0.0f;
    }

    /// Classify flow regime
    enum class FlowRegime { SUBSONIC, TRANSONIC, SUPERSONIC, HYPERSONIC };
    inline FlowRegime classifyFlow(float mach) {
        if (mach < 0.8f) return FlowRegime::SUBSONIC;
        if (mach < 1.2f) return FlowRegime::TRANSONIC;
        if (mach < 5.0f) return FlowRegime::SUPERSONIC;
        return FlowRegime::HYPERSONIC;
    }

    /// Ideal gas equation of state: P = ρRT
    inline float idealGasPressure(float density, float temperature, float R_specific) {
        return density * R_specific * temperature;
    }

    /// Temperature from internal energy: T = e / (cv) = e·(γ-1)/(R)
    inline float temperatureFromEnergy(float specificInternalEnergy, float gamma,
                                        float R_specific) {
        float cv = R_specific / (gamma - 1.0f);
        return (cv > 0.0f) ? specificInternalEnergy / cv : 0.0f;
    }

    /// Isentropic relations — pressure ratio across shock
    /// P2/P1 = (1 + (γ-1)/2 · M²)^(γ/(γ-1))
    inline float isentropicPressureRatio(float mach, float gamma) {
        float base = 1.0f + 0.5f * (gamma - 1.0f) * mach * mach;
        return std::pow(base, gamma / (gamma - 1.0f));
    }

    /// Normal shock relations — pressure jump across shock wave
    /// P2/P1 = 1 + 2γ/(γ+1) · (M₁² - 1)
    inline float normalShockPressureRatio(float mach1, float gamma) {
        return 1.0f + 2.0f * gamma / (gamma + 1.0f) * (mach1 * mach1 - 1.0f);
    }

    /// Mach number after normal shock
    /// M₂² = (1 + (γ-1)/2 · M₁²) / (γ·M₁² - (γ-1)/2)
    inline float normalShockMachAfter(float mach1, float gamma) {
        float m1sq = mach1 * mach1;
        float num = 1.0f + 0.5f * (gamma - 1.0f) * m1sq;
        float den = gamma * m1sq - 0.5f * (gamma - 1.0f);
        if (den <= 0.0f) return 1.0f;
        return std::sqrt(num / den);
    }

    /// Temperature ratio across normal shock
    inline float normalShockTemperatureRatio(float mach1, float gamma) {
        float m1sq = mach1 * mach1;
        float pRatio = normalShockPressureRatio(mach1, gamma);
        float rhoRatio = (gamma + 1.0f) * m1sq / (2.0f + (gamma - 1.0f) * m1sq);
        return (rhoRatio > 0.0f) ? pRatio / rhoRatio : 1.0f;
    }

    /// Density ratio across normal shock (Rankine-Hugoniot)
    inline float normalShockDensityRatio(float mach1, float gamma) {
        float m1sq = mach1 * mach1;
        return (gamma + 1.0f) * m1sq / (2.0f + (gamma - 1.0f) * m1sq);
    }

    /// Drag coefficient in compressible flow (Prandtl-Glauert correction)
    /// Cd_compressible = Cd_incompressible / √(1 - M²)  (subsonic only)
    inline float prandtlGlauertCorrection(float cd_incompressible, float mach) {
        if (mach >= 1.0f) return cd_incompressible * 3.0f; // Rough supersonic estimate
        float denomSq = 1.0f - mach * mach;
        if (denomSq <= 0.01f) denomSq = 0.01f; // Avoid singularity near Mach 1
        return cd_incompressible / std::sqrt(denomSq);
    }

    /// Stagnation (total) pressure: P0 = P · (1 + (γ-1)/2 · M²)^(γ/(γ-1))
    inline float stagnationPressure(float staticPressure, float mach, float gamma) {
        return staticPressure * isentropicPressureRatio(mach, gamma);
    }

    /// Stagnation temperature: T0 = T · (1 + (γ-1)/2 · M²)
    inline float stagnationTemperature(float staticTemp, float mach, float gamma) {
        return staticTemp * (1.0f + 0.5f * (gamma - 1.0f) * mach * mach);
    }

} // namespace CompressibleMath

// ═══════════════════════════════════════════════════════════════
// Blast Wave — Sedov-Taylor solution for point explosions
// Used for explosion simulation
// ═══════════════════════════════════════════════════════════════

namespace BlastWave {

    /// Sedov-Taylor blast wave radius at time t
    /// R(t) = ξ₀ · (E·t²/ρ₀)^(1/5)   (ξ₀ ≈ 1.15 for γ=1.4)
    inline float blastRadius(float energy_J, float ambientDensity, float time_s,
                              float gamma = 1.4f) {
        (void)gamma;
        float xi0 = 1.15f; // Dimensionless constant for γ ≈ 1.4
        if (ambientDensity <= 0.0f || time_s <= 0.0f) return 0.0f;
        return xi0 * std::pow(energy_J * time_s * time_s / ambientDensity, 0.2f);
    }

    /// Blast overpressure at distance r from explosion
    /// Hopkinson-Cranz scaling: ΔP ∝ (E^(1/3)/r)
    inline float blastOverpressure(float energy_J, float distance_m,
                                    float ambientPressure = CompressibleConstants::P_ATM) {
        if (distance_m <= 0.01f) distance_m = 0.01f;
        if (ambientPressure <= 0.0f) return 0.0f;
        float scaledDist = distance_m / std::pow(energy_J / ambientPressure, 1.0f / 3.0f);
        // Kingery-Bulmash approximation (simplified)
        float peakOverpressure = ambientPressure * 808.0f *
                                  std::pow(1.0f + scaledDist * scaledDist, -1.5f);
        return std::min(peakOverpressure, energy_J / (4.18879f * distance_m * distance_m * distance_m));
    }

    /// Blast wave velocity (from Rankine-Hugoniot)
    inline float blastWaveVelocity(float overpressure, float ambientPressure,
                                    float ambientDensity, float gamma = 1.4f) {
        if (ambientPressure <= 0.0f || ambientDensity <= 0.0f) return 0.0f;
        float pressureRatio = 1.0f + overpressure / ambientPressure;
        float c0 = std::sqrt(gamma * ambientPressure / ambientDensity);
        return c0 * std::sqrt(1.0f + (gamma + 1.0f) / (2.0f * gamma) * (pressureRatio - 1.0f));
    }

} // namespace BlastWave

} // namespace physics
} // namespace engine
