#pragma once

#include "math/Vector3D.h"
#include <cmath>

namespace engine {
namespace physics {

// ═══════════════════════════════════════════════════════════════
// Hyperelasticity — Non-linear elastic material models
//
// For large deformations of rubber, biological tissue, gels.
// Linear Hooke's law (σ=Eε) fails above ~5% strain.
// These models use strain energy density functions W(F)
// where F is the deformation gradient tensor.
// ═══════════════════════════════════════════════════════════════

namespace HyperelasticMath {

    // ── Invariants of deformation ─────────────────────────────
    // For a stretch tensor with principal stretches λ₁, λ₂, λ₃:
    // I₁ = λ₁² + λ₂² + λ₃²
    // I₂ = λ₁²λ₂² + λ₂²λ₃² + λ₃²λ₁²
    // I₃ = λ₁²λ₂²λ₃² = J²  (J = det(F) = volume ratio)

    struct PrincipalStretches {
        float lambda1 = 0.0f, lambda2 = 0.0f, lambda3 = 0.0f;

        float I1() const { return lambda1*lambda1 + lambda2*lambda2 + lambda3*lambda3; }
        float I2() const {
            float l1sq = lambda1*lambda1, l2sq = lambda2*lambda2, l3sq = lambda3*lambda3;
            return l1sq*l2sq + l2sq*l3sq + l3sq*l1sq;
        }
        float J() const { return lambda1 * lambda2 * lambda3; } // Volume ratio
    };

    // ── Neo-Hookean Model ────────────────────────────────────
    // W = μ/2·(I₁ - 3) - μ·ln(J) + λ/2·ln²(J)
    // μ: shear modulus, λ: first Lamé parameter
    // Good for rubber up to ~100% strain

    struct NeoHookeanParams {
        float mu = 0.0f;      // Shear modulus (Pa)
        float lambda = 0.0f;  // First Lamé parameter (Pa)

        /// From Young's modulus E and Poisson ratio ν
        static NeoHookeanParams fromEngineering(float E, float nu) {
            NeoHookeanParams p;
            p.mu = E / (2.0f * (1.0f + nu));
            p.lambda = E * nu / ((1.0f + nu) * (1.0f - 2.0f * nu));
            return p;
        }

        static NeoHookeanParams Rubber()     { return fromEngineering(1e6f, 0.49f); }
        static NeoHookeanParams SoftTissue() { return fromEngineering(1e4f, 0.45f); }
        static NeoHookeanParams Gel()        { return fromEngineering(1e3f, 0.48f); }
        static NeoHookeanParams Cartilage()  { return fromEngineering(1e7f, 0.40f); }
    };

    /// Strain energy density W for Neo-Hookean model
    inline float neoHookeanEnergy(const PrincipalStretches& s, const NeoHookeanParams& p) {
        float J = s.J();
        float logJ = std::log(J);
        return 0.5f * p.mu * (s.I1() - 3.0f) - p.mu * logJ + 0.5f * p.lambda * logJ * logJ;
    }

    /// Cauchy stress for uniaxial Neo-Hookean:
    /// σ = μ(λ² - 1/λ) + λ_param·ln(λ)/λ  (simplified 1D)
    inline float neoHookeanStress1D(float stretch, const NeoHookeanParams& p) {
        if (stretch <= 0.0f) return 0.0f;
        float stress = p.mu * (stretch * stretch - 1.0f / stretch);
        stress += p.lambda * std::log(stretch) / stretch;
        return stress;
    }

    // ── Mooney-Rivlin Model ─────────────────────────────────
    // W = C₁₀(I₁-3) + C₀₁(I₂-3)
    // More accurate than Neo-Hookean for moderate strains
    // C₁₀, C₀₁: material constants (Pa)

    struct MooneyRivlinParams {
        float C10 = 0.0f;  // Pa
        float C01 = 0.0f;  // Pa

        // For incompressible materials: μ = 2(C10 + C01)
        float shearModulus() const { return 2.0f * (C10 + C01); }

        static MooneyRivlinParams NaturalRubber() { return {0.16e6f, 0.004e6f}; }
        static MooneyRivlinParams SiliconeRubber() { return {0.08e6f, 0.02e6f}; }
        static MooneyRivlinParams Muscle() { return {5e3f, 1e3f}; }
    };

    /// Strain energy density for Mooney-Rivlin
    inline float mooneyRivlinEnergy(const PrincipalStretches& s, const MooneyRivlinParams& p) {
        return p.C10 * (s.I1() - 3.0f) + p.C01 * (s.I2() - 3.0f);
    }

    /// Uniaxial stress for Mooney-Rivlin (simplified 1D for incompressible)
    /// σ = 2(λ - 1/λ²)(C₁₀ + C₀₁/λ)
    inline float mooneyRivlinStress1D(float stretch, const MooneyRivlinParams& p) {
        if (stretch <= 0.0f) return 0.0f;
        float invLSq = 1.0f / (stretch * stretch);
        return 2.0f * (stretch - invLSq) * (p.C10 + p.C01 / stretch);
    }

    // ── Ogden Model ─────────────────────────────────────────
    // W = Σᵢ μᵢ/αᵢ · (λ₁^αᵢ + λ₂^αᵢ + λ₃^αᵢ - 3)
    // Most general; excellent fit for all rubber-like materials

    struct OgdenTerm {
        float mu = 0.0f;     // Modulus (Pa)
        float alpha = 0.0f;  // Exponent (dimensionless)
    };

    /// Strain energy for Ogden (multi-term)
    inline float ogdenEnergy(const PrincipalStretches& s,
                              const OgdenTerm* terms, int numTerms) {
        float W = 0.0f;
        for (int i = 0; i < numTerms; i++) {
            float a = terms[i].alpha;
            float mu = terms[i].mu;
            W += (mu / a) * (std::pow(s.lambda1, a) + std::pow(s.lambda2, a) +
                              std::pow(s.lambda3, a) - 3.0f);
        }
        return W;
    }

} // namespace HyperelasticMath

} // namespace physics
} // namespace engine
