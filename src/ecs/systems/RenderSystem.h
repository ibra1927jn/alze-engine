#pragma once

#include <SDL.h>
#include <sstream>
#include <algorithm>
#include "SystemManager.h"
#include "ECSCoordinator.h"
#include "Components.h"
#include "DebugDraw.h"
#include "Camera2D.h"
#include "Profiler.h"
#include "FrameLogger.h"
#include "InputRecorder.h"
#include "RenderCommand.h"
#include "RenderBackend.h"
#include "game/Particles.h"

namespace engine {
namespace ecs {

class RenderSystem : public System {
public:
    struct HUDState {
        float fps = 0;
        int particleCount = 0;
        int maxParticles = 1;
        bool gravityOn = true;
        bool onGround = false;
        float playerSpeed = 0;
        int entityCount = 0;
    };

    struct DebugState {
        Entity player = 0;
        bool onGround = false;
        float coyoteTime = 0;
        float coyoteMax = 0.12f;
        int broadTests = 0, narrowTests = 0, colResolved = 0;
        int entityCount = 0;
        int particleCount = 0;
    };

    RenderSystem(ECSCoordinator& ecs, SDL_Renderer* renderer)
        : m_ecs(ecs), m_renderer(renderer), m_backend(renderer) {
        priority = 10;
    }

    void setCamera(core::Camera2D* camera) { m_camera = camera; }
    void setParticlePool(const game::ParticlePool* pool) { m_particlePool = pool; }
    void setCollisionSystem(System* col) { m_colSystem = col; }

    void setHUDState(const HUDState& state) { m_hud = state; }
    void setDebugState(const DebugState& state) { m_debug = state; }

    int getDrawCalls() const { return m_backend.getDrawCalls(); }

    void render(float alpha);
    int getVisibleCount() const { return m_visibleCount; }
    void update(float) override {} // Render is called manually

private:
    void renderEntities();
    void renderParticles();
    void renderGrid(int cellWorld);
    void renderHUD();
    void renderDebug();

    void drawRect(math::Vector2D pos, float w, float h, math::Color c, bool fill = true, int16_t z = 0);
    void drawLine(math::Vector2D a, math::Vector2D b, math::Color c, int16_t z = 0);

    struct RenderEntry {
        Entity entity;
        math::Vector2D pos;
        math::Vector2D size;
        math::Color color;
        int16_t zOrder;
        bool isPlayer;
        bool isPlatform;
    };

    ECSCoordinator&            m_ecs;
    SDL_Renderer*              m_renderer;
    core::RenderQueue          m_queue;
    core::RenderQueue          m_hudQueue;
    core::RenderBackend        m_backend;
    core::Camera2D*            m_camera = nullptr;
    const game::ParticlePool*  m_particlePool = nullptr;
    System*                    m_colSystem = nullptr;
    float                      m_alpha = 1.0f;
    int                        m_visibleCount = 0;
    HUDState                   m_hud;
    DebugState                 m_debug;

    std::vector<RenderEntry>   m_renderList;
};

} // namespace ecs
} // namespace engine
