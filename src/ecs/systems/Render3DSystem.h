#pragma once

#include "../ECSCoordinator.h"
#include "../Components3D.h"
#include "../../renderer/ForwardRenderer.h"

namespace engine {
namespace ecs {

/// Render3DSystem — Submits 3D ECS entities to the ForwardRenderer.
///
/// Features:
///   - Parallel world matrix computation (pre-pass via parallelForEach)
///   - Dirty flag caching (skip re-computation for static objects)
///   - Automatic point light submission
///
class Render3DSystem {
public:
    Render3DSystem(ECSCoordinator& ecs, renderer::ForwardRenderer& renderer)
        : m_ecs(ecs), m_renderer(renderer) {}

    /// Submit all renderable ECS entities to the ForwardRenderer.
    /// Call between renderer.begin() and renderer.end().
    void run() {
        // Pre-pass: compute world matrices in parallel (safe: no shared writes)
        updateWorldMatrices();

        // Submit meshes (must be single-threaded: renderer is not thread-safe)
        m_ecs.forEach<Transform3DComponent, MeshComponent>(
            [this](Entity, Transform3DComponent& t, MeshComponent& m) {
                if (!m.mesh || !m.mesh->isValid() || !m.visible) return;
                m_renderer.submit(*m.mesh, m.material, t.worldMatrix);
            }
        );

        // Submit point lights
        m_ecs.forEach<Transform3DComponent, PointLightComponent>(
            [this](Entity, Transform3DComponent& t, PointLightComponent& l) {
                renderer::PointLight pl;
                pl.position = t.transform.position;
                pl.color = l.color;
                pl.constant = l.constant;
                pl.linear = l.linear;
                pl.quadratic = l.quadratic;
                m_renderer.addPointLight(pl);
            }
        );
    }

private:
    /// Pre-pass: rebuild world matrices for dirty transforms.
    /// Uses parallelForEach when entity count > 64 (CPU-bound workload).
    void updateWorldMatrices() {
        // Count entities to decide parallel vs serial
        uint32_t count = m_ecs.countEntities<Transform3DComponent, MeshComponent>();

        if (count > 64) {
            // Parallel: each entity's toMatrix() is independent (no shared state)
            m_ecs.parallelForEach<Transform3DComponent, MeshComponent>(
                [](Entity, Transform3DComponent& t, MeshComponent&) {
                    if (t.dirty) {
                        t.worldMatrix = t.transform.toMatrix();
                        t.dirty = false;
                    }
                }, 4  // 4 threads
            );
        } else {
            // Serial fallback for small counts
            m_ecs.forEach<Transform3DComponent, MeshComponent>(
                [](Entity, Transform3DComponent& t, MeshComponent&) {
                    if (t.dirty) {
                        t.worldMatrix = t.transform.toMatrix();
                        t.dirty = false;
                    }
                }
            );
        }
    }

    ECSCoordinator& m_ecs;
    renderer::ForwardRenderer& m_renderer;
};

} // namespace ecs
} // namespace engine
