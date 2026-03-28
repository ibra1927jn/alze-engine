#pragma once

#include "math/Vector3D.h"

namespace engine {
namespace renderer {

class Texture2D;
class ShaderProgram;

/// Material — Propiedades PBR metallic-roughness.
///
/// Modelo compatible con glTF/Unreal:
///   - albedo: color base
///   - metallic: 0=dielectric, 1=metal
///   - roughness: 0=espejo, 1=mate
///   - ao: ambient occlusion
///   - normalMap: textura de normales (tangent space)
///
struct Material {
    // ── PBR Properties ─────────────────────────────────────────
    math::Vector3D albedoColor = math::Vector3D(0.8f, 0.8f, 0.8f);
    float metallic    = 0.0f;    // 0=dielectric  1=metal
    float roughness   = 0.5f;    // 0=mirror       1=matte
    float ao          = 1.0f;    // Ambient occlusion (1=full)

    // ── Emissive ───────────────────────────────────────────────
    math::Vector3D emissiveColor = math::Vector3D(0, 0, 0);
    float emissiveIntensity = 1.0f;

    // ── Textures (nullptr = use scalar values) ─────────────────
    const Texture2D* albedoTexture          = nullptr;
    const Texture2D* normalMap              = nullptr;
    const Texture2D* metallicRoughnessMap   = nullptr;  // R=unused, G=roughness, B=metallic (glTF)
    const Texture2D* emissiveTexture        = nullptr;
    const Texture2D* aoTexture              = nullptr;
    const Texture2D* heightMap              = nullptr;   // For parallax occlusion mapping
    float parallaxScale = 0.05f;                         // Height scale for POM

    // ── Comparacion por valor (para skip de material redundante) ──
    bool operator==(const Material& o) const {
        return albedoColor == o.albedoColor
            && metallic == o.metallic && roughness == o.roughness && ao == o.ao
            && emissiveColor == o.emissiveColor && emissiveIntensity == o.emissiveIntensity
            && albedoTexture == o.albedoTexture && normalMap == o.normalMap
            && metallicRoughnessMap == o.metallicRoughnessMap
            && emissiveTexture == o.emissiveTexture && aoTexture == o.aoTexture
            && heightMap == o.heightMap && parallaxScale == o.parallaxScale;
    }
    bool operator!=(const Material& o) const { return !(*this == o); }

    // ── Apply to active shader ─────────────────────────────────
    void apply(const ShaderProgram& shader) const;

    // ════════════════════════════════════════════════════════════
    // PBR Presets (physically accurate values)
    // ════════════════════════════════════════════════════════════

    /// Gold — metal dorado
    static Material gold() {
        Material m;
        m.albedoColor = math::Vector3D(1.0f, 0.766f, 0.336f);
        m.metallic = 1.0f;
        m.roughness = 0.35f;
        return m;
    }

    /// Silver — metal plateado
    static Material silver() {
        Material m;
        m.albedoColor = math::Vector3D(0.972f, 0.960f, 0.915f);
        m.metallic = 1.0f;
        m.roughness = 0.3f;
        return m;
    }

    /// Copper — cobre
    static Material copper() {
        Material m;
        m.albedoColor = math::Vector3D(0.955f, 0.638f, 0.538f);
        m.metallic = 1.0f;
        m.roughness = 0.4f;
        return m;
    }

    /// Iron — hierro oscuro
    static Material iron() {
        Material m;
        m.albedoColor = math::Vector3D(0.560f, 0.570f, 0.580f);
        m.metallic = 1.0f;
        m.roughness = 0.5f;
        return m;
    }

    /// Plastic red — plástico rojo brillante
    static Material plasticRed() {
        Material m;
        m.albedoColor = math::Vector3D(0.9f, 0.1f, 0.08f);
        m.metallic = 0.0f;
        m.roughness = 0.35f;
        return m;
    }

    /// Plastic white — plástico blanco
    static Material plasticWhite() {
        Material m;
        m.albedoColor = math::Vector3D(0.95f, 0.95f, 0.95f);
        m.metallic = 0.0f;
        m.roughness = 0.4f;
        return m;
    }

    /// Rubber — goma negra mate
    static Material rubber() {
        Material m;
        m.albedoColor = math::Vector3D(0.05f, 0.05f, 0.05f);
        m.metallic = 0.0f;
        m.roughness = 0.9f;
        return m;
    }

    /// Ceramic — cerámica blanca brillante
    static Material ceramic() {
        Material m;
        m.albedoColor = math::Vector3D(0.95f, 0.93f, 0.88f);
        m.metallic = 0.0f;
        m.roughness = 0.15f;
        return m;
    }

    /// Emerald green — plástico verde
    static Material emerald() {
        Material m;
        m.albedoColor = math::Vector3D(0.1f, 0.7f, 0.35f);
        m.metallic = 0.0f;
        m.roughness = 0.3f;
        return m;
    }

    /// Ruby — rojo profundo con brillo
    static Material ruby() {
        Material m;
        m.albedoColor = math::Vector3D(0.75f, 0.05f, 0.1f);
        m.metallic = 0.15f;
        m.roughness = 0.25f;
        return m;
    }

    /// Chrome — metal pulido (casi espejo)
    static Material chrome() {
        Material m;
        m.albedoColor = math::Vector3D(0.95f, 0.93f, 0.88f);
        m.metallic = 1.0f;
        m.roughness = 0.25f;
        return m;
    }

    /// Red — material rojo genérico
    static Material red() {
        Material m;
        m.albedoColor = math::Vector3D(0.85f, 0.12f, 0.08f);
        m.metallic = 0.0f;
        m.roughness = 0.5f;
        return m;
    }

    /// Default — gris neutro
    static Material defaultMaterial() {
        return Material();
    }
};

} // namespace renderer
} // namespace engine
