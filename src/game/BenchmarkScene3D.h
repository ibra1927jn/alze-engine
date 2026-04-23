#pragma once
// ════════════════════════════════════════════════════════════════════════
//  BenchmarkScene3D.h  —  Escena de stress test usando SceneSetup3D +
//                         WorldScene3D. Prueba que la separación
//                         engine/game funciona: este state NO toca
//                         ningún sistema interno directamente.
//
//  Uso (en main o en un StateManager):
//    #include "game/BenchmarkScene3D.h"
//    states.push(std::make_unique<game::BenchmarkScene3D>(engine));
//
//  Lo que hace:
//    - Inicializa renderer PBR completo con IBL usando SceneSetup3D
//    - Lanza 200 esferas + 50 cubos físicos usando WorldScene3D
//    - Muestra FPS, entidades, contacts y sleeping-bodies en HUD
//    - F5 resetea la escena, Tab toggle debug colliders
// ════════════════════════════════════════════════════════════════════════

#include "core/StateManager.h"
#include "core/Engine.h"
#include "core/Logger.h"
#include "core/Profiler.h"
#include "core/DebugDraw3D.h"
#include "renderer/ForwardRenderer.h"
#include "renderer/PostProcess.h"
#include "renderer/SSAO.h"
#include "renderer/Skybox.h"
#include "renderer/EnvironmentMap.h"
#include "renderer/MeshPrimitives.h"
#include "renderer/ScreenEffects.h"
#include "renderer/LODSystem.h"
#include "renderer/DecalRenderer.h"
#include "renderer/SpriteBatch2D.h"
#include "renderer/TextRenderer.h"
#include "renderer/InstancedRenderer.h"
#include "ecs/ECSCoordinator.h"
#include "ecs/Components3D.h"
#include "ecs/systems/Render3DSystem.h"
#include "ecs/systems/Physics3DSystem.h"
#include "scene/FPSController.h"
#include "math/Matrix4x4.h"
#include "math/MathUtils.h"
#include "game/SceneSetup3D.h"
#include "game/WorldScene3D.h"

#include <memory>
#include <string>
#include <vector>
#include <SDL.h>

namespace engine {
namespace game {

class BenchmarkScene3D : public core::IGameState {
public:
    static constexpr int   SPHERE_COUNT  = 200;
    static constexpr int   CUBE_COUNT    = 50;
    static constexpr float SPAWN_HEIGHT  = 30.0f;

    explicit BenchmarkScene3D(core::Engine& engine)
        : m_engine(engine) {}

    const char* getName() const override { return "BenchmarkScene3D"; }

    void onEnter() override;
    void handleInput(float dt) override;
    void update(float dt) override;
    void render(float) override;
    void onExit() override;

private:
    void spawnAll();
    void reset();
    void renderHUD(int sw, int sh);
    void logResults();

    core::Engine&                       m_engine;
    renderer::ForwardRenderer           m_renderer;
    renderer::PostProcess               m_postProcess;
    renderer::SSAO                      m_ssao;
    renderer::Skybox                    m_skybox;
    renderer::EnvironmentMap            m_envMap;
    renderer::SkyParams                 m_skyParams;
    renderer::InstancedRenderer         m_instancer;
    renderer::MotionBlur                m_motionBlur;
    renderer::DecalRenderer             m_decalRenderer;

    renderer::Mesh3D                    m_sphereHigh, m_sphereMed, m_sphereLow;
    renderer::Mesh3D                    m_cube, m_plane, m_particleMesh;
    renderer::LODGroup                  m_sphereLOD;
    renderer::ProceduralTexture         m_floorNormal;
    renderer::Texture2D                 m_floorNormalWrap;

    ecs::ECSCoordinator                 m_ecs;
    std::unique_ptr<ecs::Physics3DSystem>  m_physics;
    std::unique_ptr<ecs::Render3DSystem>   m_renderSystem;
    std::vector<ecs::Entity>            m_benchEntities;

    scene::FPSController                m_camera;
    renderer::SpriteBatch2D             m_spriteBatch;
    renderer::TextRenderer              m_textRenderer;
    math::Matrix4x4                     m_prevVP;

    bool     m_showDebug   = false;
    bool     m_ssaoEnabled = true;
    bool     m_hdriLoaded  = false;
    float    m_time        = 0.0f;

    float    m_currentFPS  = 0.0f;
    int      m_frameCount  = 0;
    float    m_elapsedSec  = 0.0f;
    int      m_resetCount  = 0;
    uint32_t m_startTime   = 0;
};

} // namespace game
} // namespace engine
