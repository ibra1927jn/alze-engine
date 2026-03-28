#pragma once
// ════════════════════════════════════════════════════════════════════════
//  SceneSetup3D.h  —  Renderer + IBL + Asset initialization helpers
//
//  Propósito: SEPARAR la inicialización del motor de la lógica de juego.
//
//  Play3DState incluye este header y llama las funciones aquí definidas.
//  Ventajas:
//    - Play3DState se queda como coordinador delgado
//    - Fácil reutilizar la configuración en otros GameStates
//    - Cambiar IBL, meshes o post-process sin tocar la lógica de juego
//
//  Uso:
//    #include "game/SceneSetup3D.h"
//    SceneSetup3D::initRenderer(m_renderer, m_postProcess, m_ssao, w, h);
//    SceneSetup3D::initMeshes(m_sphereHigh, m_sphereMed, ...);
//    SceneSetup3D::initIBL(m_envMap, m_skyParams, m_renderer);
// ════════════════════════════════════════════════════════════════════════

#include "renderer/ForwardRenderer.h"
#include "renderer/PostProcess.h"
#include "renderer/SSAO.h"
#include "renderer/Skybox.h"
#include "renderer/EnvironmentMap.h"
#include "renderer/MeshPrimitives.h"
#include "renderer/ProceduralTexture.h"
#include "renderer/InstancedRenderer.h"
#include "renderer/ScreenEffects.h"
#include "renderer/LODSystem.h"
#include "renderer/DecalRenderer.h"
#include "renderer/ModelLoader.h"
#include "renderer/Texture2D.h"
#include "core/Logger.h"
#include "math/Vector3D.h"

namespace engine {
namespace game {

/// SceneSetup3D — Funciones puras de inicialización del entorno 3D.
/// Todas sin estado propio: reciben referencias a los sistemas del estado.
namespace SceneSetup3D {

// ────────────────────────────────────────────────────────────────────────
//  Renderer + PostProcess + SSAO
// ────────────────────────────────────────────────────────────────────────

/// Inicializar todos los subsistemas de rendering.
/// @param w Ancho de pantalla, h Alto de pantalla
inline void initRenderer(renderer::ForwardRenderer& renderer,
                         renderer::PostProcess&    postProcess,
                         renderer::SSAO&           ssao,
                         renderer::Skybox&         skybox,
                         renderer::InstancedRenderer& instancer,
                         renderer::MotionBlur&     motionBlur,
                         renderer::DecalRenderer&  decalRenderer,
                         int w, int h)
{
    renderer.init(w, h);
    postProcess.init(w, h);
    ssao.init(w, h);
    skybox.init();
    instancer.init();
    motionBlur.init(w, h);
    decalRenderer.init();
    core::Logger::info("SceneSetup3D", "Renderer subsystems initialized (" +
        std::to_string(w) + "x" + std::to_string(h) + ")");
}

// ────────────────────────────────────────────────────────────────────────
//  Directional Light (sun)
// ────────────────────────────────────────────────────────────────────────

inline void setupSunLight(renderer::ForwardRenderer& renderer) {
    renderer::DirectionalLight sun;
    sun.direction       = math::Vector3D(-0.4f, -0.7f, -0.3f).normalized();
    sun.color           = {1.0f, 0.95f, 0.88f};
    sun.skyColor        = {0.3f, 0.35f, 0.50f};
    sun.groundColor     = {0.15f, 0.13f, 0.10f};
    sun.ambientIntensity= 0.35f;
    renderer.setDirectionalLight(sun);

    auto& s = renderer.getSettings();
    s.shadows         = true;
    s.shadowResolution= 2048;
    s.shadowDistance  = 40.0f;
    s.frustumCulling  = true;
    s.sortObjects     = true;
    s.iblIntensity    = 1.0f;
    s.fogDensity      = 0.0003f;
}

// ────────────────────────────────────────────────────────────────────────
//  IBL — Intentar cargar HDRI, si no disponible generar proceduralmente
// ────────────────────────────────────────────────────────────────────────

struct IBLResult {
    bool      hdriLoaded = false;
    renderer::SkyParams skyParams;
};

/// Intenta cargar un .hdr HDRI. Si falla, genera el IBL procedurально.
/// Conecta el EnvironmentMap al renderer.
inline IBLResult initIBL(renderer::EnvironmentMap& envMap,
                         renderer::ForwardRenderer& renderer)
{
    IBLResult result;

    // Intentar cargar HDRI (orden de prioridad)
    bool ok = envMap.generateFromHDRI("assets/hdri/sky_2k.hdr", 512, 32, 128);
    if (!ok) ok = envMap.generateFromHDRI("assets/hdri/sky.hdr", 512, 32, 128);

    result.hdriLoaded = ok;

    // Parámetros de cielo (usados tanto si hay HDRI como si no)
    result.skyParams.topColor    = {0.08f, 0.18f, 0.55f};
    result.skyParams.horizonColor= {0.70f, 0.65f, 0.60f};
    result.skyParams.bottomColor = {0.35f, 0.32f, 0.30f};
    result.skyParams.sunDir      = {0.4f,  0.45f, -0.5f};
    result.skyParams.sunColor    = {1.5f,  1.1f,  0.7f};
    result.skyParams.sunSize     = 256.0f;

    if (!ok) {
        // Generar IBL proceduralmente con los sky params
        envMap.generate(result.skyParams, 256, 32, 128);
        core::Logger::warn("SceneSetup3D", "HDRI not found — using procedural sky IBL");
    } else {
        core::Logger::info("SceneSetup3D", "HDRI IBL loaded OK");
    }

    renderer.setEnvironmentMap(&envMap);
    return result;
}

// ────────────────────────────────────────────────────────────────────────
//  Meshes & LOD
// ────────────────────────────────────────────────────────────────────────

/// Crear todas las primitivas de malla usadas en la escena.
inline void initMeshes(renderer::Mesh3D& sphereHigh,
                       renderer::Mesh3D& sphereMed,
                       renderer::Mesh3D& sphereLow,
                       renderer::Mesh3D& cube,
                       renderer::Mesh3D& plane,
                       renderer::Mesh3D& particleMesh)
{
    renderer::MeshPrimitives::createSphere(sphereHigh, 128, 64);
    renderer::MeshPrimitives::createSphere(sphereMed,   32, 16);
    renderer::MeshPrimitives::createSphere(sphereLow,   12,  6);
    renderer::MeshPrimitives::createCube(cube);
    renderer::MeshPrimitives::createPlane(plane, 10);
    renderer::MeshPrimitives::createSphere(particleMesh, 8, 4);
    core::Logger::info("SceneSetup3D", "Meshes created (sphere LOD 3x, cube, plane, particle)");
}

/// Crear LOD group para las esferas.
inline void initSphereLOD(renderer::LODGroup& lodGroup,
                          renderer::Mesh3D& sphereHigh,
                          renderer::Mesh3D& sphereMed,
                          renderer::Mesh3D& sphereLow)
{
    lodGroup.addLevel(&sphereHigh,  0.0f, 12.0f);
    lodGroup.addLevel(&sphereMed,  10.0f, 30.0f);
    lodGroup.addLevel(&sphereLow,  25.0f, 80.0f);
}

// ────────────────────────────────────────────────────────────────────────
//  Textures (procedurales)
// ────────────────────────────────────────────────────────────────────────

/// Crear textura de normal map procedural para el suelo.
/// Devuelve el ProceduralTexture; el Wrapper debe envolverlo externamente.
inline renderer::ProceduralTexture createFloorNormalMap() {
    return renderer::ProceduralTexture::normalMap(512, 6.0f, 0.12f);
}

/// Crear textura de decal procedural.
inline renderer::ProceduralTexture createDecalTexture() {
    return renderer::ProceduralTexture::normalMap(64, 2.0f, 0.5f);
}

// ────────────────────────────────────────────────────────────────────────
//  Aplicar visual style a PostProcess (no depende de ningún estado)
// ────────────────────────────────────────────────────────────────────────

inline renderer::PostProcessSettings makeVisualStyle(int visualStyle,
                                                      bool ssaoEnabled,
                                                      float fadeIn = 0.0f)
{
    renderer::PostProcessSettings pp;
    // Base (PBR Realistic)
    pp.exposure          = 1.0f;
    pp.colorSaturation   = 1.05f;
    pp.colorContrast     = 1.08f;
    pp.bloomIntensity    = 0.18f;
    pp.bloomThreshold    = 0.9f;
    pp.vignetteStrength  = 0.22f;
    pp.filmGrain         = 0.008f;
    pp.fxaaEnabled       = true;
    pp.ssaoEnabled       = ssaoEnabled;

    switch (visualStyle) {
        case 1: // Cinematic
            pp.exposure         = 0.85f;
            pp.colorSaturation  = 0.9f;
            pp.colorContrast    = 1.2f;
            pp.bloomIntensity   = 0.25f;
            pp.bloomThreshold   = 0.8f;
            pp.vignetteStrength = 0.35f;
            pp.filmGrain        = 0.015f;
            pp.chromaticAberration= 0.0005f;
            break;
        case 2: // Monochrome
            pp.colorSaturation  = 0.0f;
            pp.colorContrast    = 1.35f;
            pp.vignetteStrength = 0.3f;
            pp.filmGrain        = 0.02f;
            pp.bloomIntensity   = 0.1f;
            break;
        case 3: // Neon Vibrant
            pp.exposure         = 1.5f;
            pp.colorSaturation  = 1.8f;
            pp.colorContrast    = 1.25f;
            pp.bloomIntensity   = 0.35f;
            pp.bloomThreshold   = 0.6f;
            pp.vignetteStrength = 0.15f;
            pp.chromaticAberration= 0.001f;
            break;
        default: break; // 0 = PBR ya configurado arriba
    }

    // Fade-in al arranque
    if (fadeIn > 0.01f) {
        float t = 1.0f - fadeIn;
        pp.colorContrast  *= t;
        pp.bloomIntensity *= t;
    }

    return pp;
}

} // namespace SceneSetup3D
} // namespace game
} // namespace engine
