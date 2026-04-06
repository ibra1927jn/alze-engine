#pragma once

#include "math/Vector3D.h"
#include <cmath>

namespace engine {
namespace physics {

// ═══════════════════════════════════════════════════════════════
// Optics Constants
// ═══════════════════════════════════════════════════════════════

namespace OpticsConstants {
    constexpr float PI = 3.14159265359f;
    
    // Indices of refraction (at standard wavelength 589nm)
    constexpr float IOR_AIR     = 1.00029f;
    constexpr float IOR_WATER   = 1.33317f;
    constexpr float IOR_GLASS   = 1.52000f;
    constexpr float IOR_DIAMOND = 2.41800f;
    constexpr float IOR_ICE     = 1.31000f;
    
    // Planck's constant (J·s)
    constexpr float PLANCK = 6.62607015e-34f;
    // Speed of light (m/s)
    constexpr float SPEED_OF_LIGHT = 2.99792458e8f;
    // Stefan-Boltzmann constant (W/(m²·K⁴))
    constexpr float STEFAN_BOLTZMANN = 5.67037441918e-8f;
    // Wien's displacement constant (m·K)
    constexpr float WIEN_B = 2.897771955e-3f;
    // Boltzmann constant (J/K)
    constexpr float BOLTZMANN = 1.380649e-23f;
}

// ═══════════════════════════════════════════════════════════════
// Ray — Geometric optics ray
// ═══════════════════════════════════════════════════════════════

struct Ray {
    math::Vector3D origin;
    math::Vector3D direction; // Must be normalized
    
    math::Vector3D at(float t) const { return origin + direction * t; }
};

// ═══════════════════════════════════════════════════════════════
// Refraction Math — Snell, Fresnel, TIR
// ═══════════════════════════════════════════════════════════════

namespace RefractionMath {

    /// Snell's Law: n1·sin(θ1) = n2·sin(θ2)
    /// Returns the refracted ray direction (or zeros if TIR occurs)
    /// incidentDir: normalized incident direction
    /// surfaceNormal: normalized surface normal (pointing from medium 2 into medium 1)
    inline math::Vector3D snellsLaw(float n1, float n2,
                                     const math::Vector3D& incidentDir,
                                     const math::Vector3D& surfaceNormal) {
        if (n2 == 0.0f) return math::Vector3D::Zero;
        float cosI = -incidentDir.dot(surfaceNormal);
        float sinT2 = (n1 / n2) * (n1 / n2) * (1.0f - cosI * cosI);

        if (sinT2 > 1.0f) {
            // Total Internal Reflection
            return math::Vector3D::Zero;
        }

        float cosT = std::sqrt(1.0f - sinT2);
        math::Vector3D refracted = incidentDir * (n1 / n2) + surfaceNormal * ((n1 / n2) * cosI - cosT);
        return refracted;
    }

    /// Fresnel equations — compute reflectance R for unpolarized light
    /// Returns R ∈ [0,1]. Transmittance T = 1 - R.
    /// n1, n2: indices of refraction, cosTheta: cos of angle of incidence
    inline float fresnelReflectance(float n1, float n2, float cosTheta) {
        if (n2 == 0.0f) return 1.0f;
        float sinT2 = (n1 / n2) * (n1 / n2) * (1.0f - cosTheta * cosTheta);
        if (sinT2 > 1.0f) return 1.0f; // TIR

        float cosT = std::sqrt(1.0f - sinT2);

        // Fresnel s-polarization (transverse electric)
        float denomS = n1 * cosTheta + n2 * cosT;
        float Rs = (denomS != 0.0f) ? (n1 * cosTheta - n2 * cosT) / denomS : 0.0f;
        // Fresnel p-polarization (transverse magnetic)
        float denomP = n2 * cosTheta + n1 * cosT;
        float Rp = (denomP != 0.0f) ? (n2 * cosTheta - n1 * cosT) / denomP : 0.0f;

        return 0.5f * (Rs * Rs + Rp * Rp);
    }

    /// Schlick approximation for Fresnel reflectance (faster)
    inline float schlickApproximation(float n1, float n2, float cosTheta) {
        if (n1 + n2 == 0.0f) return 0.0f;
        float r0 = (n1 - n2) / (n1 + n2);
        r0 = r0 * r0;
        return r0 + (1.0f - r0) * std::pow(1.0f - cosTheta, 5.0f);
    }

    /// Test total internal reflection
    inline bool totalInternalReflection(float n1, float n2, float cosTheta) {
        if (n2 == 0.0f) return true;
        float sinT2 = (n1 / n2) * (n1 / n2) * (1.0f - cosTheta * cosTheta);
        return sinT2 > 1.0f;
    }

    /// Perfect specular reflection direction
    inline math::Vector3D reflect(const math::Vector3D& incident, const math::Vector3D& normal) {
        return incident - normal * (2.0f * incident.dot(normal));
    }

} // namespace RefractionMath

// ═══════════════════════════════════════════════════════════════
// Blackbody Radiation Math — Wien, Planck, Stefan-Boltzmann
// ═══════════════════════════════════════════════════════════════

namespace BlackbodyMath {

    /// Planck's law — spectral radiance B_λ(λ, T) [W·sr⁻¹·m⁻³]
    /// λ: wavelength (meters), T: temperature (Kelvin)
    inline float planckRadiance(float wavelength_m, float T) {
        if (T <= 0.0f || wavelength_m <= 0.0f) return 0.0f;
        
        // B_λ = (2hc²/λ⁵) · 1/(exp(hc/λkT) - 1)
        float hc_lkT = (OpticsConstants::PLANCK * OpticsConstants::SPEED_OF_LIGHT) /
                       (wavelength_m * OpticsConstants::BOLTZMANN * T);
        
        if (hc_lkT > 700.0f) return 0.0f; // Prevent overflow
        
        float lambda5 = wavelength_m * wavelength_m * wavelength_m * wavelength_m * wavelength_m;
        float numerator = 2.0f * OpticsConstants::PLANCK * OpticsConstants::SPEED_OF_LIGHT *
                          OpticsConstants::SPEED_OF_LIGHT;
        return numerator / (lambda5 * (std::exp(hc_lkT) - 1.0f));
    }

    /// Wien's displacement law — peak wavelength λ_max = b/T [meters]
    inline float wienPeakWavelength(float T) {
        if (T <= 0.0f) return 0.0f;
        return OpticsConstants::WIEN_B / T;
    }

    /// Stefan-Boltzmann law — total radiated power P = ε·σ·A·T⁴ [Watts]
    /// emissivity: 0 (perfect mirror) to 1 (perfect blackbody)
    inline float stefanBoltzmannPower(float T, float surfaceArea_m2, float emissivity = 1.0f) {
        return emissivity * OpticsConstants::STEFAN_BOLTZMANN * surfaceArea_m2 * 
               T * T * T * T;
    }

    /// Net radiated power (body radiates, environment absorbs too)
    /// P_net = ε·σ·A·(T_body⁴ - T_env⁴)
    inline float netRadiationPower(float T_body, float T_env, float surfaceArea_m2,
                                   float emissivity = 1.0f) {
        return emissivity * OpticsConstants::STEFAN_BOLTZMANN * surfaceArea_m2 *
               (T_body * T_body * T_body * T_body - T_env * T_env * T_env * T_env);
    }

    /// Convert wavelength to approximate RGB color (visible range: 380nm-700nm)
    /// Returns {R,G,B} each [0..1]
    struct RGB { float r, g, b; };
    inline RGB wavelengthToRGB(float wavelength_nm) {
        float r = 0, g = 0, b = 0;
        if (wavelength_nm >= 380 && wavelength_nm < 440) {
            r = -(wavelength_nm - 440) / 60.0f;
            b = 1.0f;
        } else if (wavelength_nm >= 440 && wavelength_nm < 490) {
            g = (wavelength_nm - 440) / 50.0f;
            b = 1.0f;
        } else if (wavelength_nm >= 490 && wavelength_nm < 510) {
            g = 1.0f;
            b = -(wavelength_nm - 510) / 20.0f;
        } else if (wavelength_nm >= 510 && wavelength_nm < 580) {
            r = (wavelength_nm - 510) / 70.0f;
            g = 1.0f;
        } else if (wavelength_nm >= 580 && wavelength_nm < 645) {
            r = 1.0f;
            g = -(wavelength_nm - 645) / 65.0f;
        } else if (wavelength_nm >= 645 && wavelength_nm <= 700) {
            r = 1.0f;
        }
        // Intensity correction at edges
        float factor = 1.0f;
        if ((wavelength_nm >= 380 && wavelength_nm < 420))
            factor = 0.3f + 0.7f * (wavelength_nm - 380) / 40.0f;
        else if (wavelength_nm >= 700 && wavelength_nm <= 780)
            factor = 0.3f + 0.7f * (780 - wavelength_nm) / 80.0f;
        return {r * factor, g * factor, b * factor};
    }

} // namespace BlackbodyMath

} // namespace physics
} // namespace engine
