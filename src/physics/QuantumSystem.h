#pragma once

#include <cmath>
#include <string>
#include <complex>

namespace engine {
namespace physics {

// ═══════════════════════════════════════════════════════════════
// Quantum Constants (NIST 2018 CODATA values)
// ═══════════════════════════════════════════════════════════════

namespace QuantumConstants {
    constexpr double PLANCK         = 6.62607015e-34;   // h  J·s
    constexpr double HBAR           = 1.054571817e-34;  // ħ = h/(2π) J·s
    constexpr double ELECTRON_MASS  = 9.1093837015e-31; // kg
    constexpr double PROTON_MASS    = 1.67262192369e-27;// kg
    constexpr double ELECTRON_VOLT  = 1.602176634e-19;  // eV in Joules
    constexpr double BOHR_RADIUS    = 5.29177210903e-11;// a₀ m
    constexpr double RYDBERG_ENERGY = 13.605693122994;  // eV (Hydrogen ground state = -13.6eV)
    constexpr double FINE_STRUCTURE = 7.2973525693e-3;  // α (dimensionless)
    constexpr double SPEED_OF_LIGHT = 2.99792458e8;     // m/s
    constexpr double PI             = 3.14159265358979323846;
}

// ═══════════════════════════════════════════════════════════════
// Quantum Math — Analytically tractable QM formulas
// ═══════════════════════════════════════════════════════════════

namespace QuantumMath {

    // ── de Broglie Wavelength ────────────────────────────────────
    // λ = h / (m·v) = h / p
    // Returns wavelength in meters
    inline double deBroglieWavelength(double mass_kg, double speed_ms) {
        if (mass_kg <= 0.0 || speed_ms <= 0.0) return 0.0;
        return QuantumConstants::PLANCK / (mass_kg * speed_ms);
    }

    // λ from kinetic energy: E = p²/2m → p = √(2mE) → λ = h/p
    inline double deBroglieWavelengthFromEnergy(double mass_kg, double energy_J) {
        if (mass_kg <= 0.0 || energy_J <= 0.0) return 0.0;
        double p = std::sqrt(2.0 * mass_kg * energy_J);
        return QuantumConstants::PLANCK / p;
    }

    // ── Heisenberg Uncertainty Principle ────────────────────────
    // Δx·Δp ≥ ħ/2
    // Given Δx, returns minimum Δp (kg·m/s)
    inline double minimumMomentumUncertainty(double deltaX_m) {
        if (deltaX_m <= 0.0) return 0.0;
        return QuantumConstants::HBAR / (2.0 * deltaX_m);
    }

    // Given Δp, returns minimum Δx (m)
    inline double minimumPositionUncertainty(double deltaP_kgms) {
        if (deltaP_kgms <= 0.0) return 0.0;
        return QuantumConstants::HBAR / (2.0 * deltaP_kgms);
    }

    // Check if the uncertainty principle is satisfied (ΔxΔp ≥ ħ/2)
    inline bool heisenbergSatisfied(double deltaX_m, double deltaP_kgms) {
        return deltaX_m * deltaP_kgms >= QuantumConstants::HBAR / 2.0;
    }

    // ── Particle in a Box ────────────────────────────────────────
    // Infinite square well energy levels: E_n = n²·π²·ħ²/(2mL²)
    // n = 1, 2, 3, ... (quantum number), L = box length (m)
    // Returns energy in Joules
    inline double particleInBoxEnergy(int n, double mass_kg, double length_m) {
        if (n <= 0 || mass_kg <= 0.0 || length_m <= 0.0) return 0.0;
        double hbar2 = QuantumConstants::HBAR * QuantumConstants::HBAR;
        double pi2 = QuantumConstants::PI * QuantumConstants::PI;
        return (static_cast<double>(n * n) * pi2 * hbar2) / (2.0 * mass_kg * length_m * length_m);
    }

    // ── Tunneling Probability (WKB Approximation) ───────────────
    // For a rectangular potential barrier V > E (classical forbidden region):
    // T = exp(-2κd)  where κ = √(2m(V-E)) / ħ
    // d = barrier width (m), V = barrier height (J), E = particle energy (J)
    // Returns transmission coefficient T ∈ [0, 1]
    inline double tunnelingProbability(double mass_kg, double energy_J,
                                        double barrierHeight_J, double barrierWidth_m) {
        if (energy_J >= barrierHeight_J) return 1.0; // Classical regime
        
        double kappa = std::sqrt(2.0 * mass_kg * (barrierHeight_J - energy_J)) 
                       / QuantumConstants::HBAR;
        double exponent = -2.0 * kappa * barrierWidth_m;
        
        if (exponent < -700.0) return 0.0; // Numerically zero
        return std::exp(exponent);
    }

    // ── Hydrogen Atom Energy Levels ──────────────────────────────
    // Bohr model: E_n = -13.6 eV / n²
    // Returns energy in eV (always negative for bound states)
    inline double hydrogenEnergyLevel(int n) {
        if (n <= 0) return 0.0;
        return -QuantumConstants::RYDBERG_ENERGY / static_cast<double>(n * n);
    }

    // Photon energy for transition n_high → n_low : ΔE = |E_high - E_low|
    // Since energies are negative: E_n2 > E_n1 (less negative)
    // Emitted photon energy = E(upper) - E(lower) = |ΔE| (positive)
    // Returns photon energy in Joules (positive = emission)
    inline double hydrogenTransitionEnergy_J(int n_low, int n_high) {
        if (n_low <= 0 || n_high <= n_low) return 0.0;
        // E(n_high) is less negative (higher energy state), E(n_low) more negative
        double dE_eV = hydrogenEnergyLevel(n_high) - hydrogenEnergyLevel(n_low); // negative - more_negative = positive
        return std::abs(dE_eV) * QuantumConstants::ELECTRON_VOLT;
    }

    // Wavelength of emitted photon for hydrogen transition (Rydberg formula)
    // λ = hc / ΔE  (meters)
    inline double hydrogenTransitionWavelength(int n_low, int n_high) {
        double dE = hydrogenTransitionEnergy_J(n_low, n_high);
        if (dE <= 0.0) return 0.0;
        return (QuantumConstants::PLANCK * QuantumConstants::SPEED_OF_LIGHT) / dE;
    }

    // ── Gaussian Wave Packet ─────────────────────────────────────
    // ψ(x, t=0) = (1/(σ√(2π)))^0.5 · exp(-x²/(4σ²)) · exp(ik₀x)
    // Returns probability density |ψ|² at position x
    // sigma: spatial width of wave packet (m)
    // k0: central wave vector (1/m)
    inline double gaussianWavePacketProbabilityDensity(double x, double sigma, double /*k0*/) {
        if (sigma <= 0.0) return 0.0;
        double exponent = -(x * x) / (2.0 * sigma * sigma);
        double norm = 1.0 / (sigma * std::sqrt(2.0 * QuantumConstants::PI));
        return norm * std::exp(exponent); // k0 only affects phase, not |ψ|²
    }

    // ── Compton Scattering ────────────────────────────────────────
    // Δλ = (h / m_e·c) · (1 - cos θ)
    // Returns wavelength shift (m) for photon scattered by an electron
    inline double comptonWavelengthShift(double theta_rad) {
        const double comptonWavelength = QuantumConstants::PLANCK /
                                         (QuantumConstants::ELECTRON_MASS * 
                                          QuantumConstants::SPEED_OF_LIGHT);
        return comptonWavelength * (1.0 - std::cos(theta_rad));
    }

} // namespace QuantumMath

} // namespace physics
} // namespace engine
