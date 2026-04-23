#pragma once

#include "core/Engine.h"
#include "core/StateManager.h"
#include "core/EventBus.h"
#include "core/Profiler.h"
#include "core/DebugDraw.h"
#include "core/JobSystem.h"
#include "core/AudioSystem.h"
#include "core/ProceduralAudio.h"
#include "core/Camera2D.h"
#include "core/Logger.h"
#include "core/FrameLogger.h"
#include "core/InputRecorder.h"
#include "physics/Raycast.h"
#include "math/Vector2D.h"
#include "math/MathUtils.h"
#include "math/Color.h"
#include "math/AABB.h"
#include "ecs/ECSCoordinator.h"
#include "ecs/Components.h"
#include "ecs/systems/PhysicsSystem.h"
#include "ecs/systems/CollisionSystem.h"
#include "ecs/systems/RenderSystem.h"
#include "ecs/systems/InputSystem.h"
#include "game/Particles.h"
#include "game/PauseState.h"
#include "game/Play3DState.h"
#include "game/SharedWorldState.h"
#include "renderer/SpriteBatch2D.h"
#include "renderer/TextRenderer.h"
#include "renderer/TileMap.h"
#include "renderer/ProceduralTexture.h"
#include <SDL.h>
#include <vector>

namespace engine {
namespace game {

class PlayState : public core::IGameState {
public:
    PlayState(core::Engine& engine, ecs::ECSCoordinator& ecs,
              core::EventBus& eventBus, core::AudioEngine& audio,
              core::StateManager* states = nullptr,
              core::JobSystem* jobs = nullptr,
              SharedWorldState* world = nullptr)
        : m_engine(engine), m_ecs(ecs), m_eventBus(eventBus), m_audio(audio)
        , m_states(states), m_jobs(jobs), m_world(world)
    {}

    const char* getName() const override { return "Play"; }
    bool shouldUpdatePhysics() const override { return true; }

    void onEnter() override;
    void onExit() override;
    void handleInput(float dt) override;
    void update(float dt) override;
    void render(float alpha) override;

private:
    ecs::Entity createPlayer(math::Vector2D pos);
    ecs::Entity createPlatform(math::Vector2D pos, float w, float h, math::Color color);
    void initTileBackground();

    // ── State ──────────────────────────────────────────────────
    core::Engine& m_engine;
    ecs::ECSCoordinator& m_ecs;
    core::EventBus& m_eventBus;
    core::AudioEngine& m_audio;
    core::StateManager* m_states = nullptr;
    core::JobSystem* m_jobs = nullptr;
    SharedWorldState* m_world = nullptr;

    ecs::PhysicsSystem*   m_physics   = nullptr;
    ecs::CollisionSystem* m_collision = nullptr;
    ecs::InputSystem*     m_input     = nullptr;
    ecs::RenderSystem*    m_render    = nullptr;
    core::Camera2D        m_camera;

    ecs::Entity m_player = 0;
    std::vector<ecs::Entity> m_dynPlats;
    ParticlePool m_particles;

    bool  m_onGround = false;
    bool  m_gravityOn = true;
    bool  m_jumpHeld = false;
    float m_coyote = 0;
    float m_jumpBuffer = 0;
    float m_shootCooldown = 0;
    float m_spawnCooldown = 0;
    int   m_hitSoundsThisFrame = 0;
    int   m_frameNum = 0;

    bool m_audioLoaded = false;
    float m_fadeAlpha = 0.0f;
    bool  m_fadingTo3D = false;

    renderer::SpriteBatch2D m_spriteBatch2D;
    renderer::TextRenderer m_textRenderer2D;
    renderer::TileMap m_tileMap;
    renderer::ProceduralTexture m_tilesetTex;

    GLuint m_tileTexId = 0;
};

} // namespace game
} // namespace engine
