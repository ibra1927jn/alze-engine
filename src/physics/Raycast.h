#pragma once

#include "math/Vector2D.h"
#include "math/AABB.h"
#include "math/MathUtils.h"
#include "ecs/ECSCoordinator.h"
#include "ecs/Components.h"
#include <vector>

namespace engine {
namespace physics {

/// RayHit — Resultado de un raycast exitoso.
struct RayHit {
    ecs::Entity entity;
    math::Vector2D point;      // Punto de impacto en mundo
    math::Vector2D normal;     // Normal de la superficie AABB impactada
    float distance;            // Distancia desde el origen del rayo
};

/// Raycast — Lanzar rayos contra el mundo físico.
///
/// Usa el SpatialHash para filtrar solo entidades relevantes (broad phase).
/// Internamente usa el Slab Method para intersección ray-AABB.
///
/// Ejemplo:
///   RayHit hit;
///   if (Raycast::castFirst(ecs, grid, origin, direction, 500.0f, hit)) {
///       // hit.entity, hit.point, hit.normal, hit.distance
///   }
///
class Raycast {
public:
    /// Lanzar rayo y obtener TODOS los hits (sin ordenar)
    static std::vector<RayHit> castAll(
        ecs::ECSCoordinator& ecs,
        const math::Vector2D& origin,
        const math::Vector2D& direction,
        float maxDistance)
    {
        std::vector<RayHit> results;
        math::Vector2D dir = direction.normalized();
        math::Vector2D end = origin + dir * maxDistance;

        // Calcular AABB del rayo para query espacial
        math::AABB rayAABB;
        rayAABB.min.x = math::MathUtils::min(origin.x, end.x) - 1.0f;
        rayAABB.min.y = math::MathUtils::min(origin.y, end.y) - 1.0f;
        rayAABB.max.x = math::MathUtils::max(origin.x, end.x) + 1.0f;
        rayAABB.max.y = math::MathUtils::max(origin.y, end.y) + 1.0f;

        auto& colliders = ecs.getStorage<ecs::ColliderComponent>();

        // Iterar todos los colliders (sin grid — funciona sin buildGrid)
        for (uint32_t i = 0; i < colliders.size(); i++) {
            ecs::Entity entity = colliders.getEntity(i);
            auto& col = colliders.getDense(i);

            float tMin, tMax;
            if (rayAABBIntersect(origin, dir, col.aabb, tMin, tMax)) {
                if (tMin >= 0.0f && tMin <= maxDistance) {
                    RayHit hit;
                    hit.entity = entity;
                    hit.distance = tMin;
                    hit.point = origin + dir * tMin;
                    hit.normal = computeHitNormal(hit.point, col.aabb);
                    results.push_back(hit);
                }
            }
        }

        return results;
    }

    /// Lanzar rayo y obtener solo el HIT MÁS CERCANO
    static bool castFirst(
        ecs::ECSCoordinator& ecs,
        const math::Vector2D& origin,
        const math::Vector2D& direction,
        float maxDistance,
        RayHit& outHit,
        ecs::Entity ignoreEntity = UINT32_MAX)
    {
        math::Vector2D dir = direction.normalized();
        float closestT = maxDistance + 1.0f;
        bool found = false;

        auto& colliders = ecs.getStorage<ecs::ColliderComponent>();

        for (uint32_t i = 0; i < colliders.size(); i++) {
            ecs::Entity entity = colliders.getEntity(i);
            if (entity == ignoreEntity) continue;
            auto& col = colliders.getDense(i);

            float tMin, tMax;
            if (rayAABBIntersect(origin, dir, col.aabb, tMin, tMax)) {
                if (tMin >= 0.0f && tMin < closestT) {
                    closestT = tMin;
                    outHit.entity = entity;
                    outHit.distance = tMin;
                    outHit.point = origin + dir * tMin;
                    outHit.normal = computeHitNormal(outHit.point, col.aabb);
                    found = true;
                }
            }
        }

        return found;
    }

    /// Ray-AABB intersection (Slab Method)
    /// Retorna true si el rayo intersecta el AABB.
    /// tMin = distancia de entrada, tMax = distancia de salida
    static bool rayAABBIntersect(
        const math::Vector2D& origin,
        const math::Vector2D& dir,
        const math::AABB& box,
        float& tMin, float& tMax)
    {
        float invDirX = (std::abs(dir.x) > 1e-8f) ? 1.0f / dir.x : 1e8f;
        float invDirY = (std::abs(dir.y) > 1e-8f) ? 1.0f / dir.y : 1e8f;

        float t1 = (box.min.x - origin.x) * invDirX;
        float t2 = (box.max.x - origin.x) * invDirX;
        float t3 = (box.min.y - origin.y) * invDirY;
        float t4 = (box.max.y - origin.y) * invDirY;

        tMin = math::MathUtils::max(math::MathUtils::min(t1, t2), 
                                     math::MathUtils::min(t3, t4));
        tMax = math::MathUtils::min(math::MathUtils::max(t1, t2), 
                                     math::MathUtils::max(t3, t4));

        return tMax >= 0 && tMin <= tMax;
    }

private:
    /// Calcular normal de la superficie AABB más cercana al punto de impacto
    static math::Vector2D computeHitNormal(const math::Vector2D& point, 
                                            const math::AABB& box) {
        math::Vector2D center = box.center();
        math::Vector2D half = box.halfSize();
        math::Vector2D local = point - center;

        // Normalizar a [-1, 1]
        float nx = local.x / half.x;
        float ny = local.y / half.y;

        // La cara con mayor componente normalizada es la cara de impacto
        if (std::abs(nx) > std::abs(ny)) {
            return {nx > 0 ? 1.0f : -1.0f, 0.0f};
        } else {
            return {0.0f, ny > 0 ? 1.0f : -1.0f};
        }
    }
};

} // namespace physics
} // namespace engine
