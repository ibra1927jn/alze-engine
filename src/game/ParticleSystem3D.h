#pragma once

#include "../math/Vector3D.h"
#include <vector>
#include <cstdlib>

namespace engine {
namespace game {

/// Particle3D — A single 3D particle.
struct Particle3D {
    math::Vector3D position;
    math::Vector3D velocity;
    math::Vector3D color = {1, 1, 1};
    float size     = 0.1f;
    float life     = 0.0f;
    float maxLife  = 1.0f;
    float alpha    = 1.0f;
    bool  active   = false;

    void update(float dt, const math::Vector3D& gravity) {
        velocity = velocity + gravity * dt;
        velocity = velocity * (1.0f - 0.5f * dt); // drag
        position = position + velocity * dt;
        life -= dt;
        alpha = life / maxLife; // fade out
        if (life <= 0.0f) active = false;
    }
};

/// EmitterShape — How particles are spawned.
enum class EmitterShape {
    POINT,      // All from one point
    SPHERE,     // Random direction from center
    CONE,       // Within a cone angle
    BOX         // Random within a box
};

/// ParticleEmitter3D — Configurable 3D particle emitter.
struct ParticleEmitter3D {
    math::Vector3D position    = math::Vector3D::Zero;
    math::Vector3D direction   = math::Vector3D::Up;
    EmitterShape   shape       = EmitterShape::SPHERE;
    math::Vector3D color       = {1, 0.5f, 0.1f};
    math::Vector3D colorVariation = {0.2f, 0.2f, 0.1f};
    math::Vector3D gravity     = {0, -9.81f, 0};
    float emitRate      = 50.0f;   // particles per second
    float speed         = 5.0f;
    float speedVariation = 2.0f;
    float size          = 0.1f;
    float sizeVariation = 0.05f;
    float lifetime      = 2.0f;
    float lifetimeVariation = 0.5f;
    float coneAngle     = 0.5f;    // radians (for CONE shape)
    math::Vector3D boxExtent = {1, 1, 1}; // for BOX shape
    bool  active        = true;
    bool  loop          = true;
};

/// ParticleSystem3D — High-performance 3D particle system with pooling.
///
/// Usage:
///   ParticleSystem3D particles(5000);
///   ParticleEmitter3D fire;
///   fire.position = {0, 0, 0};
///   fire.color = {1, 0.4, 0.1};
///   particles.update(dt, fire);
///
class ParticleSystem3D {
public:
    explicit ParticleSystem3D(int maxParticles = 2000)
        : m_pool(maxParticles), m_nextFree(maxParticles)
    {
        m_maxParticles = maxParticles;
        m_firstFree = 0;
        for (int i = 0; i < maxParticles - 1; i++)
            m_nextFree[i] = i + 1;
        m_nextFree[maxParticles - 1] = -1;
    }

    /// Update particles and emit new ones from the emitter
    void update(float dt, const ParticleEmitter3D& emitter) {
        // Update existing
        m_activeCount = 0;
        for (int i = 0; i < m_maxParticles; i++) {
            auto& p = m_pool[i];
            if (p.active) {
                p.update(dt, emitter.gravity);
                if (!p.active) {
                    m_nextFree[i] = m_firstFree;
                    m_firstFree = i;
                } else {
                    m_activeCount++;
                }
            }
        }

        // Emit new particles
        if (emitter.active) {
            m_emitAccum += emitter.emitRate * dt;
            int toEmit = static_cast<int>(m_emitAccum);
            m_emitAccum -= toEmit;

            for (int i = 0; i < toEmit; i++) {
                emit(emitter);
            }
        }
    }

    /// Manual burst spawn
    void burst(const ParticleEmitter3D& emitter, int count) {
        for (int i = 0; i < count; i++) emit(emitter);
    }

    void clear() {
        for (auto& p : m_pool) p.active = false;
        m_activeCount = 0;
        m_firstFree = 0;
        for (int i = 0; i < m_maxParticles - 1; i++)
            m_nextFree[i] = i + 1;
        m_nextFree[m_maxParticles - 1] = -1;
    }

    int activeCount() const { return m_activeCount; }
    int maxParticles() const { return m_maxParticles; }
    const std::vector<Particle3D>& pool() const { return m_pool; }

private:
    void emit(const ParticleEmitter3D& emitter) {
        if (m_firstFree < 0) return;

        int idx = m_firstFree;
        m_firstFree = m_nextFree[idx];
        auto& p = m_pool[idx];

        // Position based on shape
        switch (emitter.shape) {
            case EmitterShape::POINT:
                p.position = emitter.position;
                break;
            case EmitterShape::SPHERE:
                p.position = emitter.position + randomDir() * randf(0, 0.1f);
                break;
            case EmitterShape::BOX:
                p.position = emitter.position + math::Vector3D(
                    randf(-emitter.boxExtent.x, emitter.boxExtent.x),
                    randf(-emitter.boxExtent.y, emitter.boxExtent.y),
                    randf(-emitter.boxExtent.z, emitter.boxExtent.z)
                );
                break;
            case EmitterShape::CONE:
                p.position = emitter.position;
                break;

            default:
                p.position = emitter.position;
                break;
        }

        // Velocity
        float spd = emitter.speed + randf(-emitter.speedVariation, emitter.speedVariation);
        if (emitter.shape == EmitterShape::CONE) {
            // Random direction within cone
            math::Vector3D dir = emitter.direction.normalized();
            math::Vector3D perp = dir.cross(math::Vector3D(0, 1, 0.01f)).normalized();
            math::Vector3D perp2 = dir.cross(perp);
            float angle = randf(0, emitter.coneAngle);
            float phi = randf(0, 6.2831853f);
            p.velocity = (dir * cosf(angle) + perp * sinf(angle) * cosf(phi)
                         + perp2 * sinf(angle) * sinf(phi)) * spd;
        } else {
            p.velocity = randomDir() * spd;
        }

        // Color with variation
        p.color = math::Vector3D(
            clampf(emitter.color.x + randf(-emitter.colorVariation.x, emitter.colorVariation.x), 0, 1),
            clampf(emitter.color.y + randf(-emitter.colorVariation.y, emitter.colorVariation.y), 0, 1),
            clampf(emitter.color.z + randf(-emitter.colorVariation.z, emitter.colorVariation.z), 0, 1)
        );

        p.size = emitter.size + randf(-emitter.sizeVariation, emitter.sizeVariation);
        p.life = emitter.lifetime + randf(-emitter.lifetimeVariation, emitter.lifetimeVariation);
        p.maxLife = p.life;
        p.alpha = 1.0f;
        p.active = true;
        m_activeCount++;
    }

    static float randf(float lo, float hi) {
        return lo + static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * (hi - lo);
    }
    static float clampf(float v, float lo, float hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    }
    static math::Vector3D randomDir() {
        float theta = randf(0, 6.2831853f);
        float phi = acosf(randf(-1, 1));
        return math::Vector3D(sinf(phi) * cosf(theta), sinf(phi) * sinf(theta), cosf(phi));
    }

    std::vector<Particle3D> m_pool;
    std::vector<int> m_nextFree;
    int m_maxParticles;
    int m_firstFree = 0;
    int m_activeCount = 0;
    float m_emitAccum = 0.0f;
};

} // namespace game
} // namespace engine
