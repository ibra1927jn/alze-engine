#pragma once

#include "SystemManager.h"
#include "ECSCoordinator.h"
#include "Components.h"
#include "MathUtils.h"
#include "JobSystem.h"
#include <atomic>

namespace engine {
namespace ecs {

/// PhysicsSystem — Integra Newton F=ma con paralelismo por chunks.
///
/// Pipeline (por entidad, parallelizable):
///   1. Guarda previousPosition (para interpolación)
///   2. Aplica gravedad
///   3. Integración Semi-Implícita de Euler
///   4. Aplica drag
///   5. Resetea aceleración
///   6. Sleep check
///
/// Con JobSystem: divide entidades en chunks de 64 y las procesa
/// en threads paralelos. Seguro porque cada entidad es independiente
/// en la fase de integración.
///
class PhysicsSystem : public System {
public:
    PhysicsSystem(ECSCoordinator& ecs) : m_ecs(ecs) {
        priority = 0;  // Se ejecuta primero
    }

    void setGravity(float g) { m_gravity = g; }
    float getGravity() const { return m_gravity; }

    void setSleepThreshold(float velSq) { m_sleepThreshold = velSq; }
    void setSleepTime(float seconds) { m_sleepTime = seconds; }

    /// Inyectar JobSystem para paralelismo (opcional)
    void setJobSystem(core::JobSystem* jobs) { m_jobs = jobs; }

    void update(float dt) override {
        auto& transforms = m_ecs.getStorage<TransformComponent>();
        auto& physics    = m_ecs.getStorage<PhysicsComponent>();

        m_sleepingCount.store(0);

        int count = static_cast<int>(physics.size());

        if (m_jobs && count > PARALLEL_THRESHOLD) {
            // ── Parallel path: divide en chunks de 64 ──────────
            m_jobs->parallel_for(0, count, CHUNK_SIZE, [&](int start, int end) {
                integrateRange(transforms, physics, start, end, dt);
            });
        } else {
            // ── Sequential path ────────────────────────────────
            integrateRange(transforms, physics, 0, count, dt);
        }
    }

    int getSleepingCount() const { return m_sleepingCount.load(); }

private:
    /// Integrar un rango de entidades [start, end)
    /// Thread-safe: cada entidad se accede solo por un thread
    void integrateRange(ComponentStorage<TransformComponent>& transforms,
                        ComponentStorage<PhysicsComponent>& physics,
                        int start, int end, float dt) {
        for (int i = start; i < end; i++) {
            Entity entity = physics.getEntity(static_cast<uint32_t>(i));
            if (!transforms.has(entity)) continue;

            auto& phys = physics.getDense(static_cast<uint32_t>(i));
            auto& tf   = transforms.get(entity);

            if (phys.isStatic) continue;

            if (phys.isSleeping) {
                m_sleepingCount.fetch_add(1);
                continue;
            }

            // 1. Guardar posición anterior
            phys.previousPosition = tf.transform.position;

            // 2. Gravedad
            phys.acceleration.y += m_gravity;

            // 3. Euler Semi-Implícito
            phys.velocity += phys.acceleration * dt;
            tf.transform.position += phys.velocity * dt;

            // 4. Drag (exponential for frame-rate independence)
            phys.velocity *= std::exp(-phys.drag * dt);

            // 5. Reset aceleración
            phys.acceleration = math::Vector2D::Zero;

            // 6. Sleep check
            float speedSq = phys.velocity.sqrMagnitude();
            if (speedSq < m_sleepThreshold) {
                phys.sleepTimer += dt;
                if (phys.sleepTimer >= m_sleepTime) {
                    phys.isSleeping = true;
                    phys.velocity = math::Vector2D::Zero;
                    phys.acceleration = math::Vector2D::Zero;
                }
            } else {
                phys.sleepTimer = 0.0f;
            }
        }
    }

    ECSCoordinator& m_ecs;
    core::JobSystem* m_jobs = nullptr;
    float m_gravity = 980.0f;
    float m_sleepThreshold = 2.0f;  // Vel² bajo la cual se considera reposo
    float m_sleepTime = 0.5f;       // Segundos antes de dormir
    std::atomic<int> m_sleepingCount{0};

    static constexpr int CHUNK_SIZE = 64;
    static constexpr int PARALLEL_THRESHOLD = 128;  // No paralelizar si < 128 entidades
};

} // namespace ecs
} // namespace engine
