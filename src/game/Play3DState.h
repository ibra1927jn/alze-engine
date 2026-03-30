#pragma once

#include "core/StateManager.h"
#include "core/Engine.h"
#include "core/InputManager.h"
#include "core/Logger.h"
#include "core/Serializer.h"
#include "core/EventBus.h"
#include "core/DebugDraw3D.h"
#include "core/Profiler.h"
#include "core/ResourceManager.h"
#include "core/ObjectPool.h"
#include "renderer/ForwardRenderer.h"
#include "renderer/PostProcess.h"
#include "renderer/MeshPrimitives.h"
#include "renderer/Material.h"
#include "renderer/Skybox.h"
#include "renderer/EnvironmentMap.h"
#include "renderer/Texture2D.h"
#include "renderer/ProceduralTexture.h"
#include "renderer/ModelLoader.h"
#include "renderer/SSAO.h"
#include "renderer/InstancedRenderer.h"
#include "renderer/ScreenEffects.h"
#include "renderer/LODSystem.h"
#include "renderer/DecalRenderer.h"
#include "ecs/ECSCoordinator.h"
#include "ecs/Components3D.h"
#include "ecs/systems/Render3DSystem.h"
#include "ecs/systems/Physics3DSystem.h"
#include "game/ParticleSystem3D.h"
#include "game/SharedWorldState.h"
#include "game/SceneSetup3D.h"
#include "game/WorldScene3D.h"
#include "scene/SceneGraph.h"
#include "scene/FPSController.h"
#include "math/Matrix4x4.h"
#include "math/Vector3D.h"
#include "math/MathUtils.h"
#include "renderer/SpriteBatch2D.h"
#include "renderer/TextRenderer.h"

namespace engine {
namespace game {

struct Collision3DEvent {
    ecs::Entity entityA, entityB;
    math::Vector3D contactPoint, normal;
    float impulse;
};

class Play3DState : public core::IGameState {
public:
    Play3DState(core::Engine& engine, core::StateManager* states,
                game::SharedWorldState* world = nullptr)
        : m_engine(engine), m_states(states), m_particles(3000), m_world(world) {}

    const char* getName() const override { return "Play3DState"; }

    void onEnter() override;
    void onExit() override;
    void handleInput(float dt) override;
    void update(float dt) override;
    void render(float) override;

private:
    void setupECS();
    void shootCube();
    void emitCollisionEvents();
    void renderInstancedField(const math::Matrix4x4& view, const math::Matrix4x4& proj);
    void saveScene3D(const std::string& path);
    int loadScene3D(const std::string& path);
    ecs::Entity findGrabbable();
    void triggerExplosion(math::Vector3D center, float radius, float force);

    struct LODSphereData {
        math::Vector3D position;
        float scale;
        renderer::Material material;
    };

    // ── Members ────────────────────────────────────────────────
    core::Engine& m_engine;
    core::StateManager* m_states;

    // Rendering
    renderer::ForwardRenderer m_renderer;
    renderer::PostProcess m_postProcess;
    renderer::SSAO m_ssao;
    renderer::Skybox m_skybox;
    renderer::EnvironmentMap m_envMap;
    renderer::SkyParams m_skyParams;
    renderer::InstancedRenderer m_instancer;
    renderer::MotionBlur m_motionBlur;
    renderer::DecalRenderer m_decalRenderer;
    renderer::LODGroup m_sphereLOD;
    scene::FPSController m_camera;

    // Meshes & Models
    renderer::Mesh3D m_sphereHigh, m_sphereMed, m_sphereLow;
    renderer::Mesh3D m_cube, m_plane, m_particleMesh;
    renderer::LoadedModel m_helmet;
    renderer::ProceduralTexture m_floorNormal;
    renderer::Texture2D m_floorNormalWrap;
    renderer::ProceduralTexture m_decalTex;

    // ECS
    ecs::ECSCoordinator m_ecs;
    std::unique_ptr<ecs::Physics3DSystem> m_physics;
    std::unique_ptr<ecs::Render3DSystem> m_renderSystem;
    ecs::Entity m_helmetEntity = 0;
    ecs::Entity m_lightEntities[3] = {};

    // SceneGraph
    scene::SceneGraph m_sceneGraph;

    // Particles
    ParticleSystem3D m_particles;
    ParticleEmitter3D m_fireEmitter;
    ParticleEmitter3D m_obeliskEmitters[6];

    // EventBus
    core::EventBus m_eventBus;

    // ResourceManager
    core::ResourceManager<renderer::Texture2D> m_texCache;

    // ObjectPool
    core::ObjectPool<ecs::Entity> m_projectilePool;

    // LOD spheres
    std::vector<LODSphereData> m_lodSpheres;

    // Motion blur
    math::Matrix4x4 m_prevVP;

    // State
    float m_time = 0, m_shootCd = 0, m_gcTimer = 0, m_lastProfileLog = 0;
    bool m_wireframe = false, m_mouseCaptured = true;
    bool m_hdriLoaded = false, m_helmetLoaded = false;
    bool m_ssaoEnabled = true, m_showDebug = false;
    bool m_showProfiler = false;
    float m_fadeIn = 1.0f;
    int m_visualStyle = 0;
    bool m_showHUD = true;

    game::SharedWorldState* m_world = nullptr;

    renderer::SpriteBatch2D m_spriteBatch;
    renderer::TextRenderer m_textRenderer;

    ecs::Entity m_grabbedEntity = 0;
};

} // namespace game
} // namespace engine
