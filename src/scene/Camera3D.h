#pragma once

#include "math/Matrix4x4.h"
#include "math/Vector3D.h"
#include "math/MathUtils.h"

namespace engine {
namespace scene {

/// Camera3D — Cámara 3D con modos perspective/orthographic.
///
/// Modos de control:
///   - lookAt(eye, target, up) — manual
///   - Orbital: rota alrededor de un punto focal
///   - FPS: posición + yaw/pitch
///
class Camera3D {
public:
    // ── Projection ─────────────────────────────────────────────

    /// Perspective projection
    void setPerspective(float fovDegrees, float aspect, float near, float far) {
        m_fov = fovDegrees;
        m_aspect = aspect;
        m_near = near;
        m_far = far;
        m_ortho = false;
        updateProjection();
    }

    /// Orthographic projection
    void setOrthographic(float size, float aspect, float near, float far) {
        m_orthoSize = size;
        m_aspect = aspect;
        m_near = near;
        m_far = far;
        m_ortho = true;
        updateProjection();
    }

    void setAspect(float aspect) {
        m_aspect = aspect;
        updateProjection();
    }

    // ── View (manual) ──────────────────────────────────────────

    void lookAt(const math::Vector3D& eye, const math::Vector3D& target,
                const math::Vector3D& up = math::Vector3D::Up) {
        m_position = eye;
        m_target = target;
        m_view = math::Matrix4x4::lookAt(eye, target, up);
    }

    // ── Orbital Mode ───────────────────────────────────────────

    void setOrbital(const math::Vector3D& target, float distance,
                    float azimuth, float elevation) {
        m_target = target;
        m_orbitalDist = distance;
        m_orbitalAzimuth = azimuth;
        m_orbitalElevation = elevation;
        updateOrbital();
    }

    void orbit(float deltaAzimuth, float deltaElevation) {
        m_orbitalAzimuth += deltaAzimuth;
        m_orbitalElevation += deltaElevation;
        // Clamp elevation to avoid flipping
        m_orbitalElevation = math::MathUtils::clamp(
            m_orbitalElevation, -1.5f, 1.5f
        );
        updateOrbital();
    }

    void zoom(float deltaDistance) {
        m_orbitalDist = math::MathUtils::clamp(
            m_orbitalDist + deltaDistance, 0.5f, 100.0f
        );
        updateOrbital();
    }

    // ── FPS Mode ───────────────────────────────────────────────

    void setFPS(const math::Vector3D& position, float yaw, float pitch) {
        m_position = position;
        m_fpsYaw = yaw;
        m_fpsPitch = pitch;
        updateFPS();
    }

    void rotateFPS(float deltaYaw, float deltaPitch) {
        m_fpsYaw += deltaYaw;
        m_fpsPitch += deltaPitch;
        m_fpsPitch = math::MathUtils::clamp(m_fpsPitch, -1.5f, 1.5f);
        updateFPS();
    }

    void moveFPS(float forward, float right, float up) {
        math::Vector3D fwd(
            std::cos(m_fpsPitch) * std::sin(m_fpsYaw),
            0,
            std::cos(m_fpsPitch) * std::cos(m_fpsYaw)
        );
        math::Vector3D rgt(std::cos(m_fpsYaw), 0, -std::sin(m_fpsYaw));
        m_position = m_position + fwd * forward + rgt * right + math::Vector3D::Up * up;
        updateFPS();
    }

    // ── Getters ────────────────────────────────────────────────

    const math::Matrix4x4& getViewMatrix() const { return m_view; }
    const math::Matrix4x4& getProjectionMatrix() const { return m_projection; }
    const math::Vector3D& getPosition() const { return m_position; }
    const math::Vector3D& getTarget() const { return m_target; }
    float getFov() const { return m_fov; }
    float getAspect() const { return m_aspect; }
    float getNear() const { return m_near; }
    float getFar() const { return m_far; }

private:
    void updateProjection() {
        if (m_ortho) {
            float w = m_orthoSize * m_aspect;
            float h = m_orthoSize;
            m_projection = math::Matrix4x4::orthographic(-w, w, -h, h, m_near, m_far);
        } else {
            m_projection = math::Matrix4x4::perspective(
                math::MathUtils::degToRad(m_fov), m_aspect, m_near, m_far
            );
        }
    }

    void updateOrbital() {
        float x = std::cos(m_orbitalElevation) * std::sin(m_orbitalAzimuth) * m_orbitalDist;
        float y = std::sin(m_orbitalElevation) * m_orbitalDist;
        float z = std::cos(m_orbitalElevation) * std::cos(m_orbitalAzimuth) * m_orbitalDist;

        m_position = m_target + math::Vector3D(x, y, z);
        m_view = math::Matrix4x4::lookAt(m_position, m_target, math::Vector3D::Up);
    }

    void updateFPS() {
        math::Vector3D front(
            std::cos(m_fpsPitch) * std::sin(m_fpsYaw),
            std::sin(m_fpsPitch),
            std::cos(m_fpsPitch) * std::cos(m_fpsYaw)
        );
        m_target = m_position + front;
        m_view = math::Matrix4x4::lookAt(m_position, m_target, math::Vector3D::Up);
    }

    // Matrices
    math::Matrix4x4 m_view = math::Matrix4x4::identity();
    math::Matrix4x4 m_projection = math::Matrix4x4::identity();

    // State
    math::Vector3D m_position = math::Vector3D(0, 3, 6);
    math::Vector3D m_target   = math::Vector3D::Zero;

    // Projection params
    float m_fov = 50.0f, m_aspect = 16.0f/9.0f;
    float m_near = 0.1f, m_far = 100.0f;
    float m_orthoSize = 5.0f;
    bool  m_ortho = false;

    // Orbital params
    float m_orbitalDist = 6.0f;
    float m_orbitalAzimuth = 0.0f;
    float m_orbitalElevation = 0.4f;

    // FPS params
    float m_fpsYaw = 0.0f, m_fpsPitch = 0.0f;
};

} // namespace scene
} // namespace engine
