#pragma once

#include <cmath>
#include <vector>
#include <string>

namespace engine {
namespace physics {

// ═══════════════════════════════════════════════════════════════
// Nuclear Physics — Radioactive decay, nuclear energy
//
// Models α, β, γ decay, half-lives, decay chains, and
// fission/fusion energy release.
// ═══════════════════════════════════════════════════════════════

namespace NuclearConstants {
    constexpr double SPEED_OF_LIGHT = 2.99792458e8;      // m/s
    constexpr double AMU            = 1.66053906660e-27;  // kg
    constexpr double MEV_TO_JOULES  = 1.602176634e-13;    // J/MeV
    constexpr double ELECTRON_MASS  = 9.1093837015e-31;   // kg
    constexpr double PROTON_MASS    = 1.67262192369e-27;   // kg
    constexpr double NEUTRON_MASS   = 1.67492749804e-27;   // kg
    constexpr double LN2            = 0.6931471805599453;
}

// ═══════════════════════════════════════════════════════════════
// Nuclide — An isotope with nuclear properties
// ═══════════════════════════════════════════════════════════════

struct Nuclide {
    std::string name;
    int Z = 0;       // Protons (atomic number)
    int A = 0;       // Mass number (protons + neutrons)
    int N() const { return A - Z; } // Neutrons

    double atomicMass    = 0.0;  // In AMU
    double halfLife      = 0.0;  // Seconds (0 = stable)
    double bindingEnergy = 0.0;  // MeV/nucleon

    enum DecayMode : uint8_t {
        STABLE, ALPHA, BETA_MINUS, BETA_PLUS, GAMMA,
        ELECTRON_CAPTURE, SPONTANEOUS_FISSION
    };
    DecayMode primaryDecay = STABLE;

    /// Activity: A = λN = (ln2/t½)·N  [Bq = decays/s]
    double activity(double numAtoms) const {
        if (halfLife <= 0.0) return 0.0;
        return (NuclearConstants::LN2 / halfLife) * numAtoms;
    }

    /// Remaining atoms after time t: N(t) = N₀·2^(-t/t½)
    double remainingAtoms(double N0, double time_s) const {
        if (halfLife <= 0.0) return N0; // Stable
        return N0 * std::pow(2.0, -time_s / halfLife);
    }

    /// Decay constant: λ = ln(2)/t½
    double decayConstant() const {
        if (halfLife <= 0.0) return 0.0;
        return NuclearConstants::LN2 / halfLife;
    }

    // ── Common isotope presets ──────────────────────────────
    static Nuclide Uranium238() {
        Nuclide n; n.name = "U-238"; n.Z = 92; n.A = 238;
        n.atomicMass = 238.050787; n.halfLife = 1.41e17; // 4.47 billion years
        n.bindingEnergy = 7.57; n.primaryDecay = ALPHA;
        return n;
    }
    static Nuclide Uranium235() {
        Nuclide n; n.name = "U-235"; n.Z = 92; n.A = 235;
        n.atomicMass = 235.043930; n.halfLife = 2.22e16; // 704 million years
        n.bindingEnergy = 7.59; n.primaryDecay = ALPHA;
        return n;
    }
    static Nuclide Carbon14() {
        Nuclide n; n.name = "C-14"; n.Z = 6; n.A = 14;
        n.atomicMass = 14.003241; n.halfLife = 1.808e11; // 5730 years
        n.bindingEnergy = 7.52; n.primaryDecay = BETA_MINUS;
        return n;
    }
    static Nuclide Radium226() {
        Nuclide n; n.name = "Ra-226"; n.Z = 88; n.A = 226;
        n.atomicMass = 226.025410; n.halfLife = 5.05e10; // 1600 years
        n.bindingEnergy = 7.66; n.primaryDecay = ALPHA;
        return n;
    }
    static Nuclide Iodine131() {
        Nuclide n; n.name = "I-131"; n.Z = 53; n.A = 131;
        n.atomicMass = 131.004154; n.halfLife = 6.93e5; // 8.02 days
        n.bindingEnergy = 8.42; n.primaryDecay = BETA_MINUS;
        return n;
    }
    static Nuclide Cobalt60() {
        Nuclide n; n.name = "Co-60"; n.Z = 27; n.A = 60;
        n.atomicMass = 59.933817; n.halfLife = 1.66e8; // 5.27 years
        n.bindingEnergy = 8.75; n.primaryDecay = BETA_MINUS;
        return n;
    }
    static Nuclide Hydrogen1() {
        Nuclide n; n.name = "H-1"; n.Z = 1; n.A = 1;
        n.atomicMass = 1.007825; n.halfLife = 0; // Stable
        n.bindingEnergy = 0; n.primaryDecay = STABLE;
        return n;
    }
    static Nuclide Helium4() {
        Nuclide n; n.name = "He-4"; n.Z = 2; n.A = 4;
        n.atomicMass = 4.002602; n.halfLife = 0; // Stable
        n.bindingEnergy = 7.07; n.primaryDecay = STABLE;
        return n;
    }
    static Nuclide Deuterium() {
        Nuclide n; n.name = "D"; n.Z = 1; n.A = 2;
        n.atomicMass = 2.014102; n.halfLife = 0;
        n.bindingEnergy = 1.11; n.primaryDecay = STABLE;
        return n;
    }
    static Nuclide Tritium() {
        Nuclide n; n.name = "T"; n.Z = 1; n.A = 3;
        n.atomicMass = 3.016049; n.halfLife = 3.89e8; // 12.32 years
        n.bindingEnergy = 2.83; n.primaryDecay = BETA_MINUS;
        return n;
    }
};

// ═══════════════════════════════════════════════════════════════
// Nuclear Math
// ═══════════════════════════════════════════════════════════════

namespace NuclearMath {

    /// Mass defect: Δm = Z·mₚ + N·mₙ - M_atom (in kg)
    inline double massDefect(int Z, int N, double atomicMass_AMU) {
        double totalNucleonMass = Z * NuclearConstants::PROTON_MASS +
                                   N * NuclearConstants::NEUTRON_MASS;
        return totalNucleonMass - atomicMass_AMU * NuclearConstants::AMU;
    }

    /// Binding energy from mass defect: E = Δm·c² (Joules)
    inline double bindingEnergy_J(int Z, int N, double atomicMass_AMU) {
        double dm = massDefect(Z, N, atomicMass_AMU);
        return dm * NuclearConstants::SPEED_OF_LIGHT * NuclearConstants::SPEED_OF_LIGHT;
    }

    /// Binding energy in MeV
    inline double bindingEnergy_MeV(int Z, int N, double atomicMass_AMU) {
        return bindingEnergy_J(Z, N, atomicMass_AMU) / NuclearConstants::MEV_TO_JOULES;
    }

    /// Energy released in alpha decay Q = (M_parent - M_daughter - M_alpha)·c²
    inline double alphaDecayEnergy_MeV(double parentMass_AMU,
                                        double daughterMass_AMU) {
        double helium4Mass = 4.002602;
        double dm = parentMass_AMU - daughterMass_AMU - helium4Mass;
        return dm * 931.494; // 1 AMU = 931.494 MeV/c²
    }

    /// Q-value for any nuclear reaction: Q = (Σm_reactants - Σm_products)·c²
    inline double reactionQValue_MeV(double totalReactantMass_AMU,
                                      double totalProductMass_AMU) {
        return (totalReactantMass_AMU - totalProductMass_AMU) * 931.494;
    }

    /// Fusion energy: D + T → He-4 + n, Q = 17.6 MeV
    inline double fusionDT_Energy_MeV() {
        return reactionQValue_MeV(
            2.014102 + 3.016049,    // D + T
            4.002602 + 1.008665     // He-4 + neutron
        );
    }

    /// Radiation dose: absorbed dose D = E/m (Gray, Gy = J/kg)
    inline double absorbedDose(double energy_J, double mass_kg) {
        return (mass_kg > 0.0) ? energy_J / mass_kg : 0.0;
    }

    /// Equivalent dose H = D·wR (Sievert, Sv)
    /// wR: radiation weighting factor (1 for γ/β, 20 for α, 10 for neutrons)
    inline double equivalentDose(double absorbedDose_Gy, double weightingFactor) {
        return absorbedDose_Gy * weightingFactor;
    }

} // namespace NuclearMath

} // namespace physics
} // namespace engine
