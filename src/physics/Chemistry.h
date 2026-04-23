#pragma once

#include "math/Vector3D.h"
#include "../core/JobSystem.h"
#include <vector>
#include <cmath>
#include <string>
#include <algorithm>

namespace engine {
namespace physics {

// ═══════════════════════════════════════════════════════════════
// Chemical Constants (SI units, NIST data)
// ═══════════════════════════════════════════════════════════════

namespace ChemConstants {
    constexpr float GAS_CONSTANT     = 8.314462618f;   // R — J/(mol·K)
    constexpr float AVOGADRO         = 6.02214076e23f;  // molecules/mol
    constexpr float FARADAY          = 96485.33212f;    // C/mol
    constexpr float BOLTZMANN        = 1.380649e-23f;   // J/K
    constexpr float PLANCK           = 6.62607015e-34f; // J·s
    constexpr float ROOM_TEMP_K      = 293.15f;         // K (20°C)
    constexpr float WATER_ION_PRODUCT = 1e-14f;         // Kw at 25°C
    constexpr float STANDARD_PRESSURE = 101325.0f;      // Pa (1 atm)
} // namespace ChemConstants

// ═══════════════════════════════════════════════════════════════
// Substance — A chemical species with thermodynamic properties
// ═══════════════════════════════════════════════════════════════

struct Substance {
    std::string name;
    std::string formula;
    float molarMass = 0.0f;            // g/mol
    float density = 0.0f;              // kg/m³

    // Thermodynamic
    float enthalpyFormation = 0.0f;    // ΔHf° kJ/mol (standard enthalpy of formation)
    float entropyStandard = 0.0f;      // S° J/(mol·K)
    float specificHeat = 0.0f;         // Cp J/(mol·K)

    // Phase
    float meltingPoint = 0.0f;         // K
    float boilingPoint = 0.0f;         // K

    // Acid-base
    float pKa = 14.0f;                // Acid dissociation constant (-log Ka). 14 = neutral
    bool isAcid = false;
    bool isBase = false;

    // State at room temperature
    enum Phase : uint8_t { SOLID, LIQUID, GAS };
    Phase standardPhase = SOLID;
};

// ═══════════════════════════════════════════════════════════════
// ChemicalDatabase — Standard substances with real properties
// ═══════════════════════════════════════════════════════════════

namespace ChemDB {

    // ── Elements ────────────────────────────────────────────
    inline Substance Hydrogen() {
        Substance s;
        s.name = "Hydrogen"; s.formula = "H2";
        s.molarMass = 2.016f; s.density = 0.08988f;
        s.enthalpyFormation = 0.0f; s.entropyStandard = 130.68f;
        s.specificHeat = 28.84f;
        s.meltingPoint = 14.01f; s.boilingPoint = 20.28f;
        s.standardPhase = Substance::GAS;
        return s;
    }
    inline Substance Oxygen() {
        Substance s;
        s.name = "Oxygen"; s.formula = "O2";
        s.molarMass = 31.998f; s.density = 1.429f;
        s.enthalpyFormation = 0.0f; s.entropyStandard = 205.15f;
        s.specificHeat = 29.38f;
        s.meltingPoint = 54.36f; s.boilingPoint = 90.19f;
        s.standardPhase = Substance::GAS;
        return s;
    }
    inline Substance Nitrogen() {
        Substance s;
        s.name = "Nitrogen"; s.formula = "N2";
        s.molarMass = 28.014f; s.density = 1.251f;
        s.enthalpyFormation = 0.0f; s.entropyStandard = 191.61f;
        s.specificHeat = 29.12f;
        s.meltingPoint = 63.15f; s.boilingPoint = 77.36f;
        s.standardPhase = Substance::GAS;
        return s;
    }
    inline Substance Carbon() {
        Substance s;
        s.name = "Carbon"; s.formula = "C";
        s.molarMass = 12.011f; s.density = 2267.0f;
        s.enthalpyFormation = 0.0f; s.entropyStandard = 5.74f;
        s.specificHeat = 8.527f;
        s.meltingPoint = 3823.0f; s.boilingPoint = 4098.0f;
        s.standardPhase = Substance::SOLID;
        return s;
    }
    inline Substance Iron() {
        Substance s;
        s.name = "Iron"; s.formula = "Fe";
        s.molarMass = 55.845f; s.density = 7874.0f;
        s.enthalpyFormation = 0.0f; s.entropyStandard = 27.28f;
        s.specificHeat = 25.10f;
        s.meltingPoint = 1811.0f; s.boilingPoint = 3134.0f;
        s.standardPhase = Substance::SOLID;
        return s;
    }
    inline Substance Sodium() {
        Substance s;
        s.name = "Sodium"; s.formula = "Na";
        s.molarMass = 22.990f; s.density = 971.0f;
        s.enthalpyFormation = 0.0f; s.entropyStandard = 51.21f;
        s.specificHeat = 28.24f;
        s.meltingPoint = 370.87f; s.boilingPoint = 1156.0f;
        s.standardPhase = Substance::SOLID;
        return s;
    }

    // ── Compounds ───────────────────────────────────────────
    inline Substance Water() {
        Substance s;
        s.name = "Water"; s.formula = "H2O";
        s.molarMass = 18.015f; s.density = 998.0f;
        s.enthalpyFormation = -285.83f; s.entropyStandard = 69.91f;
        s.specificHeat = 75.29f;
        s.meltingPoint = 273.15f; s.boilingPoint = 373.15f;
        s.standardPhase = Substance::LIQUID;
        return s;
    }
    inline Substance CarbonDioxide() {
        Substance s;
        s.name = "Carbon Dioxide"; s.formula = "CO2";
        s.molarMass = 44.009f; s.density = 1.977f;
        s.enthalpyFormation = -393.51f; s.entropyStandard = 213.79f;
        s.specificHeat = 37.11f;
        s.meltingPoint = 194.65f; s.boilingPoint = 194.65f; // sublimes
        s.standardPhase = Substance::GAS;
        return s;
    }
    inline Substance Methane() {
        Substance s;
        s.name = "Methane"; s.formula = "CH4";
        s.molarMass = 16.043f; s.density = 0.657f;
        s.enthalpyFormation = -74.87f; s.entropyStandard = 186.26f;
        s.specificHeat = 35.69f;
        s.meltingPoint = 90.69f; s.boilingPoint = 111.66f;
        s.standardPhase = Substance::GAS;
        return s;
    }
    inline Substance HydrochloricAcid() {
        Substance s;
        s.name = "Hydrochloric Acid"; s.formula = "HCl";
        s.molarMass = 36.461f; s.density = 1490.0f;
        s.enthalpyFormation = -167.16f;
        s.pKa = -6.3f; s.isAcid = true;
        s.standardPhase = Substance::LIQUID;
        return s;
    }
    inline Substance SodiumHydroxide() {
        Substance s;
        s.name = "Sodium Hydroxide"; s.formula = "NaOH";
        s.molarMass = 39.997f; s.density = 2130.0f;
        s.enthalpyFormation = -425.6f;
        s.pKa = 29.0f; s.isBase = true;  // pKb ≈ -1
        s.meltingPoint = 596.0f;
        s.standardPhase = Substance::SOLID;
        return s;
    }
    inline Substance IronOxide() {
        Substance s;
        s.name = "Iron(III) Oxide"; s.formula = "Fe2O3";
        s.molarMass = 159.69f; s.density = 5242.0f;
        s.enthalpyFormation = -824.2f; s.entropyStandard = 87.4f;
        s.meltingPoint = 1839.0f;
        s.standardPhase = Substance::SOLID;
        return s;
    }

} // namespace ChemDB

// ═══════════════════════════════════════════════════════════════
// ChemicalReaction — A reaction with stoichiometry and energetics
// ═══════════════════════════════════════════════════════════════

struct Reactant {
    Substance substance;
    float coefficient = 1.0f;  // Stoichiometric coefficient
    float moles = 0.0f;        // Current amount
};

struct ChemicalReaction {
    std::string name;
    std::vector<Reactant> reactants;
    std::vector<Reactant> products;

    // Kinetics (Arrhenius)
    float activationEnergy = 50.0f;   // Ea kJ/mol
    float preExponentialFactor = 1e10f; // A (1/s)

    // Thermodynamics
    float enthalpyReaction = 0.0f;     // ΔH kJ/mol (negative = exothermic)
    bool isReversible = false;

    // ── Arrhenius rate constant ──────────────────────────────
    /// k = A·e^(-Ea/(RT))
    float rateConstant(float temperatureK) const {
        float exponent = -activationEnergy * 1000.0f /
                         (ChemConstants::GAS_CONSTANT * temperatureK);
        return preExponentialFactor * std::exp(exponent);
    }

    /// Compute ΔH from formation enthalpies
    float computeEnthalpyFromFormation() const {
        float Hp = 0, Hr = 0;
        for (const auto& p : products)
            Hp += p.coefficient * p.substance.enthalpyFormation;
        for (const auto& r : reactants)
            Hr += r.coefficient * r.substance.enthalpyFormation;
        return Hp - Hr; // kJ/mol
    }
};

// ═══════════════════════════════════════════════════════════════
// Chemistry Math — pH, equilibrium, Nernst equation
// ═══════════════════════════════════════════════════════════════

namespace ChemMath {

    /// pH from hydrogen ion concentration: pH = -log₁₀[H⁺]
    inline float pH(float hConcentration) {
        if (hConcentration <= 0) return 14.0f;
        return -std::log10(hConcentration);
    }

    /// [H⁺] from pH
    inline float hFromPH(float pH) {
        return std::pow(10.0f, -pH);
    }

    /// pOH from pH (at 25°C): pOH = 14 - pH
    inline float pOH(float pH) {
        return 14.0f - pH;
    }

    /// [OH⁻] from pH
    inline float ohFromPH(float pH) {
        return std::pow(10.0f, -(14.0f - pH));
    }

    /// Mix acid + base: compute final pH (strong acid + strong base)
    /// Returns pH of resulting solution
    inline float neutralizationPH(float acidMoles, float baseMoles, float totalVolumeLiters) {
        float excess = baseMoles - acidMoles; // positive = excess base
        if (totalVolumeLiters <= 0) return 7.0f;
        if (std::abs(excess) < 1e-10f) return 7.0f; // Exact neutralization
        if (excess > 0) {
            // Excess base
            float ohConc = excess / totalVolumeLiters;
            float pOH_val = -std::log10(ohConc);
            return 14.0f - pOH_val;
        }
        // Excess acid
        float hConc = (-excess) / totalVolumeLiters;
        return -std::log10(hConc);
    }

    /// Nernst equation: E = E° - (RT/nF)·ln(Q)
    /// E° = standard potential (V), n = electron transfer, Q = reaction quotient
    inline float nernstPotential(float E0, float temperatureK, int n, float Q) {
        if (n <= 0 || Q <= 0) return E0;
        return E0 - (ChemConstants::GAS_CONSTANT * temperatureK /
                     (static_cast<float>(n) * ChemConstants::FARADAY)) * std::log(Q);
    }

    /// Gibbs free energy: ΔG = ΔH - TΔS (all in kJ)
    inline float gibbsFreeEnergy(float deltaH_kJ, float deltaS_kJ_K, float temperatureK) {
        return deltaH_kJ - temperatureK * deltaS_kJ_K;
    }

    /// Equilibrium constant from Gibbs: K = e^(-ΔG/(RT))
    inline float equilibriumConstant(float deltaG_kJ, float temperatureK) {
        float exponent = -deltaG_kJ * 1000.0f /
                         (ChemConstants::GAS_CONSTANT * temperatureK);
        return std::exp(exponent);
    }

    /// Ideal gas law: PV = nRT → P = nRT/V
    inline float idealGasPressure(float moles, float temperatureK, float volumeM3) {
        if (volumeM3 <= 0) return 0;
        return moles * ChemConstants::GAS_CONSTANT * temperatureK / volumeM3;
    }

    /// Ideal gas: V = nRT/P
    inline float idealGasVolume(float moles, float temperatureK, float pressurePa) {
        if (pressurePa <= 0) return 0;
        return moles * ChemConstants::GAS_CONSTANT * temperatureK / pressurePa;
    }

} // namespace ChemMath

// ═══════════════════════════════════════════════════════════════
// Predefined Reactions
// ═══════════════════════════════════════════════════════════════

namespace ChemReactions {

    /// 2H₂ + O₂ → 2H₂O  ΔH = -572 kJ/mol
    inline ChemicalReaction HydrogenCombustion() {
        ChemicalReaction r;
        r.name = "Hydrogen Combustion";
        r.reactants = {{ChemDB::Hydrogen(), 2.0f}, {ChemDB::Oxygen(), 1.0f}};
        r.products = {{ChemDB::Water(), 2.0f}};
        r.enthalpyReaction = -571.6f; // kJ (exothermic)
        r.activationEnergy = 75.0f;
        r.preExponentialFactor = 1e11f;
        return r;
    }

    /// CH₄ + 2O₂ → CO₂ + 2H₂O  ΔH = -890 kJ/mol
    inline ChemicalReaction MethaneCombustion() {
        ChemicalReaction r;
        r.name = "Methane Combustion";
        r.reactants = {{ChemDB::Methane(), 1.0f}, {ChemDB::Oxygen(), 2.0f}};
        r.products = {{ChemDB::CarbonDioxide(), 1.0f}, {ChemDB::Water(), 2.0f}};
        r.enthalpyReaction = -890.3f;
        r.activationEnergy = 80.0f;
        r.preExponentialFactor = 1e12f;
        return r;
    }

    /// 4Fe + 3O₂ → 2Fe₂O₃  ΔH = -1648.4 kJ  (rusting)
    inline ChemicalReaction IronOxidation() {
        ChemicalReaction r;
        r.name = "Iron Oxidation (Rust)";
        r.reactants = {{ChemDB::Iron(), 4.0f}, {ChemDB::Oxygen(), 3.0f}};
        r.products = {{ChemDB::IronOxide(), 2.0f}};
        r.enthalpyReaction = -1648.4f;
        r.activationEnergy = 30.0f;  // Low Ea (slow but spontaneous)
        r.preExponentialFactor = 1e6f;
        return r;
    }

    /// HCl + NaOH → NaCl + H₂O  ΔH = -57.1 kJ (neutralization)
    inline ChemicalReaction AcidBaseNeutralization() {
        ChemicalReaction r;
        r.name = "Acid-Base Neutralization";
        r.reactants = {{ChemDB::HydrochloricAcid(), 1.0f}, {ChemDB::SodiumHydroxide(), 1.0f}};
        r.products = {{ChemDB::Water(), 1.0f}}; // NaCl omitted for simplicity
        r.enthalpyReaction = -57.1f;
        r.activationEnergy = 5.0f;   // Very fast
        r.preExponentialFactor = 1e13f;
        return r;
    }

} // namespace ChemReactions

// ═══════════════════════════════════════════════════════════════
// ReactionSystem — Runtime chemical reaction manager
//
// Tracks reactive volumes, computes reaction rates,
// and generates heat/products over time.
// ═══════════════════════════════════════════════════════════════

struct ReactiveVolume {
    std::string name;
    float temperatureK = ChemConstants::ROOM_TEMP_K;
    float volumeLiters = 1.0f;
    float pressurePa = ChemConstants::STANDARD_PRESSURE;

    // Spatial properties (for cross-system coupling)
    math::Vector3D position = math::Vector3D::Zero; // World position of the volume
    float radius = 1.0f; // Effect radius (meters)

    // Substance amounts (moles)
    struct SpeciesAmount {
        Substance substance;
        float moles = 0.0f;
    };
    std::vector<SpeciesAmount> species;

    // Registered reactions inside this volume
    std::vector<ChemicalReaction> reactions;

    // Events
    bool reactionOccurred = false;
    float lastHeatReleased = 0.0f; // kJ in last step

    /// Add a substance to the volume
    void addSubstance(const Substance& sub, float moles) {
        for (auto& sp : species) {
            if (sp.substance.formula == sub.formula) {
                sp.moles += moles;
                return;
            }
        }
        species.push_back({sub, moles});
    }

    /// Get moles of a substance by formula
    float getMoles(const std::string& formula) const {
        for (const auto& sp : species) {
            if (sp.substance.formula == formula) return sp.moles;
        }
        return 0.0f;
    }

    /// Compute pH (based on H⁺ and OH⁻ concentrations from acids/bases)
    float computePH() const {
        float hConc = 0;
        float ohConc = 0;
        for (const auto& sp : species) {
            if (sp.substance.isAcid && sp.moles > 0) {
                // Strong acid: fully dissociates
                hConc += sp.moles / volumeLiters;
            }
            if (sp.substance.isBase && sp.moles > 0) {
                ohConc += sp.moles / volumeLiters;
            }
        }
        // Net
        if (hConc > ohConc) {
            float netH = hConc - ohConc;
            return ChemMath::pH(netH);
        }
        if (ohConc > hConc) {
            float netOH = ohConc - hConc;
            float pOH_val = -std::log10(netOH);
            return 14.0f - pOH_val;
        }
        return 7.0f; // Neutral
    }
};

class ReactionSystem {
public:
    /// Attach a JobSystem to parallelize independent volumes
    void setJobSystem(engine::core::JobSystem* jobs) { m_jobs = jobs; }

    /// Add a reactive volume
    int addVolume(const ReactiveVolume& vol) {
        int idx = static_cast<int>(m_volumes.size());
        m_volumes.push_back(vol);
        return idx;
    }

    /// Step all reactions in all volumes
    void step(float dt) {
        int nVols = static_cast<int>(m_volumes.size());

        auto process = [&](int start, int end) {
            for (int vi = start; vi < end; vi++) {
                auto& vol = m_volumes[vi];
                vol.reactionOccurred = false;
                vol.lastHeatReleased = 0.0f;

                for (const auto& rxn : vol.reactions) {
                    bool canReact = true;
                    float limitingFactor = 1e30f;

                    for (const auto& reactant : rxn.reactants) {
                        float available = vol.getMoles(reactant.substance.formula);
                        float needed = reactant.coefficient;
                        if (available < needed * 1e-10f) { canReact = false; break; }
                        limitingFactor = std::min(limitingFactor, available / needed);
                    }
                    if (!canReact) continue;

                    float k = rxn.rateConstant(vol.temperatureK);
                    float reactionAmount = k * dt;
                    reactionAmount = std::min(reactionAmount, limitingFactor);
                    if (reactionAmount < 1e-15f) continue;

                    for (const auto& reactant : rxn.reactants) {
                        for (auto& sp : vol.species) {
                            if (sp.substance.formula == reactant.substance.formula) {
                                sp.moles -= reactionAmount * reactant.coefficient;
                                if (sp.moles < 0) sp.moles = 0;
                            }
                        }
                    }

                    for (const auto& product : rxn.products) {
                        vol.addSubstance(product.substance, reactionAmount * product.coefficient);
                    }

                    float heat = rxn.enthalpyReaction * reactionAmount;
                    vol.lastHeatReleased += heat;

                    float totalMoles = 0;
                    float totalCp = 0;
                    for (const auto& sp : vol.species) {
                        totalMoles += sp.moles;
                        totalCp += sp.moles * sp.substance.specificHeat;
                    }
                    if (totalCp > 1e-6f) {
                        vol.temperatureK -= heat * 1000.0f / totalCp;
                    }

                    vol.reactionOccurred = true;
                }
            }
        };

        if (m_jobs && nVols > 4)
            m_jobs->parallel_for(0, nVols, 1, process);
        else
            process(0, nVols);
    }

    // ── Accessors ───────────────────────────────────────────
    int getVolumeCount() const { return static_cast<int>(m_volumes.size()); }
    ReactiveVolume& getVolume(int i) { return m_volumes[i]; }
    const ReactiveVolume& getVolume(int i) const { return m_volumes[i]; }

private:
    std::vector<ReactiveVolume> m_volumes;
    engine::core::JobSystem* m_jobs = nullptr;
};

} // namespace physics
} // namespace engine
