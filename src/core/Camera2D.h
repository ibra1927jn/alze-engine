#pragma once

#include "math/Vector2D.h"
#include "math/AABB.h"
#include <SDL.h>

namespace engine {
namespace core {

/// Camera2D — Viewport con follow, zoom, smoothing y frustum culling.
///
/// Convierte coordenadas mundo ↔ pantalla para que el RenderSystem
/// pueda dibujar entidades relativas a la cámara.
///
/// Uso típico:
///   camera.setFollow(playerEntity);
///   camera.update(dt);
///   // En RenderSystem: rect = camera.worldRectToScreen(aabb);
///
class Camera2D {
public:
    Camera2D() = default;

    Camera2D(float viewW, float viewH)
        : m_viewWidth(viewW), m_viewHeight(viewH) {}

    // ── Follow ────────────────────────────────────────────────
    void setTarget(const math::Vector2D& target) { m_target = target; }
    void setSmoothSpeed(float speed) { m_smoothSpeed = speed; }
    
    // ── Zoom ──────────────────────────────────────────────────
    void setZoom(float z) { m_zoom = math::MathUtils::clamp(z, 0.1f, 10.0f); }
    float getZoom() const { return m_zoom; }
    void zoomBy(float delta) { setZoom(m_zoom + delta); }

    // ── Position ──────────────────────────────────────────────
    void setPosition(const math::Vector2D& pos) { m_position = pos; }
    math::Vector2D getPosition() const { return m_position; }

    // ── Bounds (optional: limitar cámara al mundo) ────────────
    void setWorldBounds(const math::AABB& bounds) { 
        m_worldBounds = bounds; 
        m_hasBounds = true; 
    }

    // ── Shake ─────────────────────────────────────────────────
    void shake(float intensity, float duration) {
        m_shakeIntensity = intensity;
        m_shakeDuration = duration;
        m_shakeTimer = duration;
    }

    // ── Update (llamar cada frame) ────────────────────────────
    void update(float dt) {
        // Smooth follow
        math::Vector2D desired = m_target;
        
        float t = 1.0f - std::pow(1.0f - m_smoothSpeed, dt * 60.0f);
        m_position = math::Vector2D::lerp(m_position, desired, t);

        // Clamp to world bounds
        if (m_hasBounds) {
            float halfW = (m_viewWidth * 0.5f) / m_zoom;
            float halfH = (m_viewHeight * 0.5f) / m_zoom;
            m_position.x = math::MathUtils::clamp(m_position.x,
                m_worldBounds.min.x + halfW, m_worldBounds.max.x - halfW);
            m_position.y = math::MathUtils::clamp(m_position.y,
                m_worldBounds.min.y + halfH, m_worldBounds.max.y - halfH);
        }

        // Shake
        m_shakeOffset = math::Vector2D::Zero;
        if (m_shakeTimer > 0) {
            m_shakeTimer -= dt;
            float factor = m_shakeTimer / m_shakeDuration;
            float rx = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) - 0.5f) * 2.0f;
            float ry = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) - 0.5f) * 2.0f;
            m_shakeOffset = math::Vector2D(rx, ry) * m_shakeIntensity * factor;
        }
    }

    // ── Conversión Mundo → Pantalla ───────────────────────────
    math::Vector2D worldToScreen(const math::Vector2D& world) const {
        math::Vector2D cam = m_position + m_shakeOffset;
        float sx = (world.x - cam.x) * m_zoom + m_viewWidth * 0.5f;
        float sy = (world.y - cam.y) * m_zoom + m_viewHeight * 0.5f;
        return {sx, sy};
    }

    // ── Conversión Pantalla → Mundo ───────────────────────────
    math::Vector2D screenToWorld(const math::Vector2D& screen) const {
        math::Vector2D cam = m_position + m_shakeOffset;
        float wx = (screen.x - m_viewWidth * 0.5f) / m_zoom + cam.x;
        float wy = (screen.y - m_viewHeight * 0.5f) / m_zoom + cam.y;
        return {wx, wy};
    }

    // ── Rect del mundo a rect de pantalla (para SDL) ──────────
    SDL_Rect worldRectToScreen(const math::Vector2D& pos, 
                                const math::Vector2D& size) const {
        math::Vector2D topLeft = worldToScreen(pos - size * 0.5f);
        float w = size.x * m_zoom;
        float h = size.y * m_zoom;
        return {
            static_cast<int>(topLeft.x),
            static_cast<int>(topLeft.y),
            static_cast<int>(w),
            static_cast<int>(h)
        };
    }

    // ── Frustum Culling: ¿es visible en pantalla? ─────────────
    bool isVisible(const math::Vector2D& pos, const math::Vector2D& size) const {
        math::Vector2D cam = m_position + m_shakeOffset;
        float halfW = (m_viewWidth * 0.5f) / m_zoom;
        float halfH = (m_viewHeight * 0.5f) / m_zoom;

        // AABB de la cámara en coordenadas mundo
        float camMinX = cam.x - halfW;
        float camMaxX = cam.x + halfW;
        float camMinY = cam.y - halfH;
        float camMaxY = cam.y + halfH;

        // AABB de la entidad
        float eMinX = pos.x - size.x * 0.5f;
        float eMaxX = pos.x + size.x * 0.5f;
        float eMinY = pos.y - size.y * 0.5f;
        float eMaxY = pos.y + size.y * 0.5f;

        // Overlap test
        return !(eMaxX < camMinX || eMinX > camMaxX ||
                 eMaxY < camMinY || eMinY > camMaxY);
    }

    // ── Viewport dims ─────────────────────────────────────────
    void setViewSize(float w, float h) { m_viewWidth = w; m_viewHeight = h; }
    float getViewWidth() const { return m_viewWidth; }
    float getViewHeight() const { return m_viewHeight; }

private:
    math::Vector2D m_position;
    math::Vector2D m_target;
    math::Vector2D m_shakeOffset;
    
    float m_viewWidth = 1024.0f;
    float m_viewHeight = 768.0f;
    float m_zoom = 1.0f;
    float m_smoothSpeed = 0.08f;   // 0 = sin smooth, 1 = instantáneo
    
    math::AABB m_worldBounds;
    bool  m_hasBounds = false;

    float m_shakeIntensity = 0.0f;
    float m_shakeDuration = 0.0f;
    float m_shakeTimer = 0.0f;
};

} // namespace core
} // namespace engine
