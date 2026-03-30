#pragma once

#include "math/Matrix4x4.h"
#include "core/InputManager.h"
#include <SDL.h>

namespace engine {
namespace scene {

/// FPSController — First-person camera with WASD + mouse look.
///
/// Usage:
///   FPSController fps;
///   fps.setPosition({0, 2, 5});
///   // in update loop:
///   fps.update(input, dt);
///   auto view = fps.getViewMatrix();
///   auto proj = fps.getProjectionMatrix(aspect);
///
class FPSController {
public:
    // ── Configuration ──────────────────────────────────────────
    float moveSpeed   = 5.0f;
    float sprintMult  = 2.0f;
    float sensitivity = 0.15f;
    float fov         = 60.0f;
    float nearPlane   = 0.1f;
    float farPlane    = 200.0f;

    // ── State ──────────────────────────────────────────────────
    void setPosition(const math::Vector3D& pos) { m_position = pos; }
    void setYaw(float y) { m_yaw = y; }
    void setPitch(float p) { m_pitch = p; }

    math::Vector3D getPosition() const { return m_position; }
    math::Vector3D getForward() const { return m_front; }
    float getYaw() const { return m_yaw; }
    float getPitch() const { return m_pitch; }

    /// Process input and update camera
    void update(const core::InputManager& input, float dt) {
        // ── Mouse look ─────────────────────────────────────────
        if (input.isMouseCaptured()) {
            math::Vector2D delta = input.getMouseDelta();
            m_yaw   += delta.x * sensitivity;
            m_pitch -= delta.y * sensitivity;

            // Clamp pitch to avoid gimbal lock
            if (m_pitch > 89.0f)  m_pitch = 89.0f;
            if (m_pitch < -89.0f) m_pitch = -89.0f;
        }

        // Recalculate direction vectors
        constexpr float DEG2RAD = 3.14159265358979f / 180.0f;
        float yawRad   = m_yaw * DEG2RAD;
        float pitchRad = m_pitch * DEG2RAD;

        m_front.x = std::cos(pitchRad) * std::sin(yawRad);
        m_front.y = std::sin(pitchRad);
        m_front.z = std::cos(pitchRad) * std::cos(yawRad);
        m_front = m_front.normalized();

        // Right = front × world up
        math::Vector3D worldUp(0, 1, 0);
        m_right = m_front.cross(worldUp).normalized();
        m_up    = m_right.cross(m_front).normalized();

        // ── WASD movement ──────────────────────────────────────
        float speed = moveSpeed;
        if (input.isKeyDown(SDL_SCANCODE_LSHIFT)) speed *= sprintMult;

        math::Vector3D moveDir;
        if (input.isKeyDown(SDL_SCANCODE_W)) moveDir += m_front;
        if (input.isKeyDown(SDL_SCANCODE_S)) moveDir -= m_front;
        if (input.isKeyDown(SDL_SCANCODE_A)) moveDir -= m_right;
        if (input.isKeyDown(SDL_SCANCODE_D)) moveDir += m_right;
        if (input.isKeyDown(SDL_SCANCODE_SPACE)) moveDir += worldUp;
        if (input.isKeyDown(SDL_SCANCODE_LCTRL)) moveDir -= worldUp;

        float len2 = moveDir.x * moveDir.x + moveDir.y * moveDir.y + moveDir.z * moveDir.z;
        if (len2 > 0.001f) {
            moveDir = moveDir.normalized();
            m_position += moveDir * speed * dt;
        }
    }

    /// Get view matrix (look-at from position along front)
    math::Matrix4x4 getViewMatrix() const {
        math::Vector3D target = m_position + m_front;
        return math::Matrix4x4::lookAt(m_position, target, math::Vector3D(0, 1, 0));
    }

    /// Get projection matrix
    math::Matrix4x4 getProjectionMatrix(float aspect) const {
        constexpr float DEG2RAD = 3.14159265358979f / 180.0f;
        return math::Matrix4x4::perspective(fov * DEG2RAD, aspect, nearPlane, farPlane);
    }

private:
    math::Vector3D m_position = {0, 2, 5};
    math::Vector3D m_front    = {0, 0, -1};
    math::Vector3D m_right    = {1, 0, 0};
    math::Vector3D m_up       = {0, 1, 0};
    float m_yaw   = 180.0f;  // Looking toward -Z
    float m_pitch = 0.0f;
};

} // namespace scene
} // namespace engine
