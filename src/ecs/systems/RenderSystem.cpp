#include "RenderSystem.h"

namespace engine {
namespace ecs {

void RenderSystem::render(float alpha) {
    core::Profiler::beginFrame();
    m_alpha = alpha;

    // Clear both command queues
    m_queue.clear();
    m_hudQueue.clear();

    renderGrid(50);      // Emits to m_queue (z=-100)
    renderEntities();    // Emits to m_queue
    renderParticles();   // Emits to m_queue

    // Sort by z-order + type batch, then execute via backend
    m_queue.sort();
    m_backend.execute(m_queue);

    // HUD/Debug → screen-space queue (drawn on top)
    renderHUD();
    if (core::DebugDraw::isEnabled() && m_debug.player != 0 && m_ecs.isAlive(m_debug.player))
        renderDebug();

    // Execute HUD queue (always on top, no z-sort needed)
    m_backend.execute(m_hudQueue);

    core::Profiler::endFrame();
    core::DebugDraw::flush(m_renderer);

    m_visibleCount = static_cast<int>(m_renderList.size());
}

void RenderSystem::renderEntities() {
    auto& sprites    = m_ecs.getStorage<SpriteComponent>();
    auto& transforms = m_ecs.getStorage<TransformComponent>();

    m_renderList.clear();

    for (uint32_t i = 0; i < sprites.size(); i++) {
        Entity entity = sprites.getEntity(i);
        if (!transforms.has(entity)) continue;

        auto& sprite = sprites.getDense(i);
        auto& tf     = transforms.get(entity);

        math::Vector2D worldPos = tf.transform.position;
        if (m_ecs.hasComponent<PhysicsComponent>(entity)) {
            auto& phys = m_ecs.getComponent<PhysicsComponent>(entity);
            worldPos = math::Vector2D::lerp(phys.previousPosition, tf.transform.position, m_alpha);
        }

        if (m_camera && !m_camera->isVisible(worldPos, sprite.size)) continue;

        bool isPlayer = m_ecs.hasTag(entity, TAG_PLAYER);
        bool isPlat   = m_ecs.hasTag(entity, TAG_PLATFORM);

        m_renderList.push_back({entity, worldPos, sprite.size, sprite.color,
                                sprite.zOrder, isPlayer, isPlat});
    }

    std::sort(m_renderList.begin(), m_renderList.end(),
        [](const RenderEntry& a, const RenderEntry& b) {
            return a.zOrder < b.zOrder;
        });

    for (auto& entry : m_renderList) {
        math::Vector2D rp = m_camera ? m_camera->worldToScreen(entry.pos) : entry.pos;
        float zoom = m_camera ? m_camera->getZoom() : 1.0f;
        float zoomW = entry.size.x * zoom;
        float zoomH = entry.size.y * zoom;

        if (entry.isPlatform) {
            float shOff = 2.0f * zoom;
            drawRect(rp + math::Vector2D(shOff, shOff), zoomW, zoomH, math::Color(0,0,0,30));
            drawRect(rp, zoomW, zoomH, entry.color);
            drawRect(rp, zoomW, zoomH, entry.color.brighter(30), false);
            drawLine(rp + math::Vector2D(-zoomW*0.5f, -zoomH*0.5f),
                     rp + math::Vector2D( zoomW*0.5f, -zoomH*0.5f),
                     entry.color.brighter(60));
        }
        else if (entry.isPlayer) {
            float pw = game::Config::PLAYER_W * zoom;
            float ph = game::Config::PLAYER_H * zoom;
            float shOff = 3.0f * zoom;

            // Shadow
            drawRect(rp + math::Vector2D(shOff, shOff), pw, ph, math::Color(0,0,0,50));

            // Glow based on speed
            float speed = 0;
            if (m_ecs.hasComponent<PhysicsComponent>(entry.entity)) {
                speed = m_ecs.getComponent<PhysicsComponent>(entry.entity).velocity.magnitude();
            }
            uint8_t glowA = static_cast<uint8_t>(math::MathUtils::clamp(speed / 10.0f, 15, 60));
            drawRect(rp, pw + 6, ph + 6, entry.color.withAlpha(glowA));

            // Body
            drawRect(rp, pw, ph, entry.color);
            drawRect(rp, pw, ph, entry.color.brighter(60), false);
            drawRect(rp, pw * 0.3f, ph * 0.3f, entry.color.brighter(90));

            // Ground indicator
            if (m_debug.onGround) {
                drawRect(rp + math::Vector2D(0, ph*0.5f + 3), pw*0.5f, 2,
                         math::Color::green().withAlpha(120));
            }
            // Coyote time indicator
            if (!m_debug.onGround && m_debug.coyoteTime < m_debug.coyoteMax) {
                float pct = 1.0f - m_debug.coyoteTime / m_debug.coyoteMax;
                drawRect(rp + math::Vector2D(0, ph*0.5f + 3), pw*0.5f * pct, 2,
                         math::Color::yellow().withAlpha(150));
            }

            // Velocity line
            if (m_ecs.hasComponent<PhysicsComponent>(entry.entity)) {
                auto& phys = m_ecs.getComponent<PhysicsComponent>(entry.entity);
                if (phys.velocity.sqrMagnitude() > 25.0f) {
                    math::Vector2D velEnd = m_camera ?
                        m_camera->worldToScreen(entry.pos + phys.velocity * 0.08f) :
                        entry.pos + phys.velocity * 0.08f;
                    drawLine(rp, velEnd, math::Color(255, 90, 90, 160));
                }
            }
        }
        else {
            // Generic entity — simple rect
            drawRect(rp, zoomW, zoomH, entry.color);
        }
    }
}

void RenderSystem::renderParticles() {
    if (!m_particlePool) return;

    for (const auto& p : m_particlePool->pool()) {
        if (!p.active) continue;
        float t = p.life / p.maxLife;
        math::Vector2D worldP = math::Vector2D::lerp(p.prevPos, p.pos, m_alpha);

        if (m_camera && !m_camera->isVisible(worldP, {p.size * 2, p.size * 2})) continue;

        math::Vector2D rp = m_camera ? m_camera->worldToScreen(worldP) : worldP;
        float zoom = m_camera ? m_camera->getZoom() : 1.0f;
        float sz = p.size * (0.2f + 0.8f * t) * zoom;
        uint8_t a = static_cast<uint8_t>(t * 255);

        if (sz > 3) drawRect(rp, sz + 3, sz + 3, p.color.withAlpha(a / 5));
        drawRect(rp, sz, sz, p.color.withAlpha(a));
    }
}

void RenderSystem::renderGrid(int cellWorld) {
    if (!m_camera) return;
    SDL_SetRenderDrawColor(m_renderer, 25, 25, 40, 60);
    float zoom = m_camera->getZoom();
    float cellScreen = cellWorld * zoom;
    if (cellScreen < 4) return;

    math::Color gridColor(25, 25, 40, 60);
    math::Vector2D topLeft = m_camera->screenToWorld({0, 0});
    math::Vector2D botRight = m_camera->screenToWorld(
        {m_camera->getViewWidth(), m_camera->getViewHeight()});

    int startX = static_cast<int>(std::floor(topLeft.x / cellWorld)) * cellWorld;
    int startY = static_cast<int>(std::floor(topLeft.y / cellWorld)) * cellWorld;
    int endX = static_cast<int>(std::ceil(botRight.x / cellWorld)) * cellWorld;
    int endY = static_cast<int>(std::ceil(botRight.y / cellWorld)) * cellWorld;

    for (int wx = startX; wx <= endX; wx += cellWorld) {
        math::Vector2D s = m_camera->worldToScreen({static_cast<float>(wx), topLeft.y});
        math::Vector2D e = m_camera->worldToScreen({static_cast<float>(wx), botRight.y});
        drawLine(s, e, gridColor, -100);
    }
    for (int wy = startY; wy <= endY; wy += cellWorld) {
        math::Vector2D s = m_camera->worldToScreen({topLeft.x, static_cast<float>(wy)});
        math::Vector2D e = m_camera->worldToScreen({botRight.x, static_cast<float>(wy)});
        drawLine(s, e, gridColor, -100);
    }
}

void RenderSystem::renderHUD() {
    // HUD background → screen-space queue
    m_hudQueue.pushRect({135, 52}, 260, 95, math::Color(8, 8, 16, 210));
    m_hudQueue.pushRectOutline({135, 52}, 260, 95, math::Color(45, 45, 75));

    // FPS bar
    math::Color fc = m_hud.fps >= 55 ? math::Color::green() :
                     m_hud.fps >= 30 ? math::Color::yellow() : math::Color::red();
    float fW = math::MathUtils::clamp(m_hud.fps / 60.0f, 0, 1) * 190;
    m_hudQueue.pushRect({60 + fW * 0.5f, 18}, fW, 8, fc.withAlpha(200));
    core::DebugDraw::drawText({12, 12}, "FPS:" + std::to_string((int)m_hud.fps), fc);

    // Particle bar
    float pW = math::MathUtils::clamp(m_hud.particleCount / (float)m_hud.maxParticles, 0, 1) * 190;
    m_hudQueue.pushRect({60 + pW * 0.5f, 32}, pW, 8, math::Color(80, 220, 120, 200));
    core::DebugDraw::drawText({12, 26}, "PTL:" + std::to_string(m_hud.particleCount), math::Color(80, 220, 120));

    // Gravity indicator
    math::Color gc = m_hud.gravityOn ? math::Color::green() : math::Color(60, 60, 80);
    m_hudQueue.pushRect({64, 46}, 8, 8, gc);
    core::DebugDraw::drawText({12, 40}, std::string("GRV:") + (m_hud.gravityOn ? "ON" : "OFF"), gc);

    // Speed bar
    float sW = math::MathUtils::clamp(m_hud.playerSpeed / 600.0f, 0, 1) * 178;
    m_hudQueue.pushRect({72 + sW * 0.5f, 46}, sW, 8, math::Color(255, 150, 80, 200));

    // Ground state
    math::Color stateCol = m_hud.onGround ? math::Color::green() : math::Color::red();
    m_hudQueue.pushRect({64, 60}, 8, 8, stateCol);
    core::DebugDraw::drawText({12, 54}, std::string("GND:") + (m_hud.onGround ? "YES" : "NO"), stateCol);

    // Entity count
    core::DebugDraw::drawText({12, 68}, "ENT:" + std::to_string(m_hud.entityCount), math::Color(180, 200, 230));

    // Controls hint
    core::DebugDraw::drawText({12, 82}, "F3:Debug F5:CSV F6:Rec F7:Play", math::Color(100, 100, 120));

    // Diagnostics indicators
    float diagX = 170;
    if (core::InputRecorder::isRecording()) {
        core::DebugDraw::drawText({diagX, 12}, "REC", math::Color::red());
        diagX += 30;
    }
    if (core::FrameLogger::isActive()) {
        core::DebugDraw::drawText({diagX, 12}, "CSV", math::Color::cyan());
        diagX += 30;
    }
    if (core::InputRecorder::isPlaying()) {
        std::string prog = "PLAY " + std::to_string(core::InputRecorder::getPlaybackIndex()) +
                           "/" + std::to_string(core::InputRecorder::getPlaybackTotal());
        core::DebugDraw::drawText({diagX, 12}, prog, math::Color::yellow());
    }
}

void RenderSystem::renderDebug() {
    auto& phys = m_ecs.getComponent<PhysicsComponent>(m_debug.player);
    auto& tf   = m_ecs.getComponent<TransformComponent>(m_debug.player);
    auto& col  = m_ecs.getComponent<ColliderComponent>(m_debug.player);

    core::DebugDraw::drawAABB(col.aabb, math::Color::cyan().withAlpha(120));
    if (phys.velocity.sqrMagnitude() > 25.0f)
        core::DebugDraw::drawVector(tf.transform.position, phys.velocity,
                                     math::Color(255, 90, 90, 200), 0.08f);
    if (m_debug.onGround) {
        math::Vector2D feet = tf.transform.position +
            math::Vector2D(0, game::Config::PLAYER_H * 0.5f + 2);
        core::DebugDraw::drawLine(feet + math::Vector2D(-8, 0),
                                  feet + math::Vector2D(8, 0), math::Color::green());
    }

    // ── Sensor text overlay ──
    float px = m_camera ? m_camera->getViewWidth() - 215.0f : 585.0f;
    float py = 8.0f;
    int line = 0;
    auto sensor = [&](const std::string& text, math::Color c = {180, 200, 230, 255}) {
        core::DebugDraw::drawTextBg({px, py + line * 12.0f}, text, c);
        line++;
    };

    std::ostringstream o;
    o.precision(1); o << std::fixed;

    math::Color fc = m_hud.fps >= 55 ? math::Color::green() :
                     m_hud.fps >= 30 ? math::Color::yellow() : math::Color::red();

    sensor("=== SENSOR ===", math::Color::cyan());
    o << "Pos: " << (int)tf.transform.position.x << "," << (int)tf.transform.position.y; sensor(o.str()); o.str("");
    o << "Vel: " << (int)phys.velocity.x << "," << (int)phys.velocity.y; sensor(o.str(), math::Color(255, 150, 80)); o.str("");
    o << "Speed: " << (int)phys.velocity.magnitude(); sensor(o.str(), math::Color(255, 150, 80)); o.str("");
    o << "Ground: " << (m_debug.onGround ? "YES" : "NO"); sensor(o.str(), m_debug.onGround ? math::Color::green() : math::Color::red()); o.str("");
    o << "Coyote: " << m_debug.coyoteTime; sensor(o.str(), m_debug.coyoteTime < m_debug.coyoteMax ? math::Color::yellow() : math::Color(100,100,100)); o.str("");
    o << "Sleep: " << (phys.isSleeping ? "YES" : "no") << " t=" << phys.sleepTimer; sensor(o.str(), phys.isSleeping ? math::Color::red() : math::Color(100,100,100)); o.str("");
    o << "Broad:" << m_debug.broadTests << " Narrow:" << m_debug.narrowTests << " Hits:" << m_debug.colResolved;
    sensor(o.str(), math::Color(180, 180, 255)); o.str("");
    o << "Entities: " << m_debug.entityCount; sensor(o.str()); o.str("");
    o << "Particles: " << m_debug.particleCount; sensor(o.str()); o.str("");
    o << "FPS: " << (int)m_hud.fps; sensor(o.str(), fc); o.str("");

    line++;
    sensor("=== PROFILER ===", math::Color::cyan());
    auto pm = core::Profiler::getMetric("Physics");
    o << "Physics: " << pm.avgMs << "ms"; sensor(o.str(), math::Color(120, 200, 255)); o.str("");
    auto cm = core::Profiler::getMetric("Collision");
    o << "Collision: " << cm.avgMs << "ms"; sensor(o.str(), math::Color(120, 200, 255)); o.str("");
    auto fm = core::Profiler::getFrameMetric();
    o << "Frame: " << fm.avgMs << "ms (max " << fm.maxMs << ")"; sensor(o.str(), math::Color(255, 200, 100)); o.str("");

    // Frame time graph
    auto hist = core::Profiler::getFrameHistory();
    if (hist.size() > 2) {
        float gx = px, gy = py + line * 12.0f + 4, gw = 200.0f, gh = 30.0f;
        m_hudQueue.pushRect({gx + gw * 0.5f, gy + gh * 0.5f}, gw, gh, math::Color(0, 0, 0, 180));
        float targetY = gy + gh - (16.67f / 33.33f * gh);
        m_hudQueue.pushLine({gx, targetY}, {gx + gw, targetY}, math::Color(80, 80, 80, 200));
        float barW = gw / static_cast<float>(hist.size());
        for (size_t i = 0; i < hist.size(); i++) {
            float ms = hist[i];
            float barH = math::MathUtils::clamp(ms / 33.33f, 0.0f, 1.0f) * gh;
            math::Color bc = ms < 16.67f ? math::Color(80, 200, 120) :
                             ms < 33.33f ? math::Color(255, 200, 80) : math::Color(255, 80, 80);
            m_hudQueue.pushRect({gx + i * barW + barW * 0.5f, gy + gh - barH * 0.5f}, barW + 1, barH, bc.withAlpha(200));
        }
    }
}

void RenderSystem::drawRect(math::Vector2D pos, float w, float h, math::Color c, bool fill, int16_t z) {
    if (fill)
        m_queue.pushRect(pos, w, h, c, z);
    else
        m_queue.pushRectOutline(pos, w, h, c, z);
}

void RenderSystem::drawLine(math::Vector2D a, math::Vector2D b, math::Color c, int16_t z) {
    m_queue.pushLine(a, b, c, z);
}

} // namespace ecs
} // namespace engine
