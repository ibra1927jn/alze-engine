#include "CollisionSystem.h"
#include "math/MathUtils.h"

namespace engine {
namespace ecs {

CollisionSystem::CollisionSystem(ECSCoordinator& ecs) : m_ecs(ecs), m_spatialHash(64.0f) {
    priority = 1;
}

void CollisionSystem::update(float dt) {
    (void)dt;

    auto& colliders  = m_ecs.getStorage<ColliderComponent>();
    auto& transforms = m_ecs.getStorage<TransformComponent>();

    m_broadPhaseTests = 0;
    m_narrowPhaseTests = 0;
    m_collisionsResolved = 0;
    m_triggersDetected = 0;
    m_contactCache.beginFrame();
    m_contactCache.resetStats();

    m_spatialHash.clear();
    m_activeEntities.clear();

    for (uint32_t i = 0; i < colliders.size(); i++) {
        Entity entity = colliders.getEntity(i);
        if (!transforms.has(entity)) continue;

        auto& col = colliders.getDense(i);
        auto& tf  = transforms.get(entity);

        math::Vector2D halfSize = col.aabb.halfSize();
        math::Vector2D center = tf.transform.position + col.offset;
        col.aabb = math::AABB::fromCenter(center, halfSize);

        m_spatialHash.insert(entity, col.aabb);

        bool isActive = true;
        if (m_ecs.hasComponent<PhysicsComponent>(entity)) {
            auto& phys = m_ecs.getComponent<PhysicsComponent>(entity);
            if (phys.isSleeping || phys.isStatic) isActive = false;
        } else {
            isActive = false;
        }
        if (isActive) {
            m_activeEntities.push_back(entity);
        }
    }

    m_spatialHash.buildGrid();

    for (Entity entityA : m_activeEntities) {
        if (!m_ecs.hasComponent<ColliderComponent>(entityA)) continue;
        auto& colA = m_ecs.getComponent<ColliderComponent>(entityA);

        auto& neighbors = m_spatialHash.query(colA.aabb);
        m_broadPhaseTests += static_cast<int>(neighbors.size());

        for (Entity entityB : neighbors) {
            if (entityA >= entityB) continue;

            if (!m_ecs.hasComponent<ColliderComponent>(entityB)) continue;
            auto& colB = m_ecs.getComponent<ColliderComponent>(entityB);

            if (!colA.shouldCollide(colB)) continue;
            if (colA.isStatic && colB.isStatic) continue;

            m_narrowPhaseTests++;

            if (!colA.aabb.overlaps(colB.aabb)) continue;

            if (colA.isTrigger || colB.isTrigger) {
                m_triggersDetected++;
                if (m_eventBus) {
                    m_eventBus->emit(core::TriggerEvent{entityA, entityB});
                }
                continue;
            }

            resolveCollision(entityA, colA, entityB, colB);
            m_collisionsResolved++;

            if (m_eventBus) {
                math::Vector2D normal = colA.aabb.getCollisionNormal(colB.aabb);
                float impulse = 0;
                if (m_ecs.hasComponent<PhysicsComponent>(entityA)) {
                    impulse = m_ecs.getComponent<PhysicsComponent>(entityA).velocity.magnitude();
                }
                m_eventBus->emit(core::CollisionEvent{entityA, entityB, impulse, normal.x, normal.y});
            }
        }
    }
}

void CollisionSystem::setEventBus(core::EventBus* bus) { m_eventBus = bus; }

void CollisionSystem::resolveCollision(Entity a, ColliderComponent& colA, Entity b, ColliderComponent& colB) {
    auto& transforms = m_ecs.getStorage<TransformComponent>();

    math::Vector2D overlap = colA.aabb.getOverlap(colB.aabb);
    if (overlap == math::Vector2D::Zero) return;

    math::Vector2D normal = colA.aabb.getCollisionNormal(colB.aabb);
    float mtvMag = (math::MathUtils::abs(normal.x) > 0.5f) ? overlap.x : overlap.y;
    math::Vector2D pushDir = -normal;

    auto cached = m_contactCache.getWarmStart(a, b);
    bool hadWarm = (cached.normalImpulse != 0.0f);
    m_contactCache.recordQuery(hadWarm);

    if (hadWarm) {
        float warmFactor = 0.8f;
        applyWarmImpulse(a, normal, cached.normalImpulse * warmFactor);
        applyWarmImpulse(b, -normal, cached.normalImpulse * warmFactor);
    }

    if (colA.isStatic) {
        auto& tfB = transforms.get(b);
        tfB.transform.position += normal * mtvMag;
        float ni = resolveVelocityWithFriction(b, normal, a);
        m_contactCache.store(a, b, ni, 0, normal.x, normal.y);
    } else if (colB.isStatic) {
        auto& tfA = transforms.get(a);
        tfA.transform.position += pushDir * mtvMag;
        float ni = resolveVelocityWithFriction(a, pushDir, b);
        m_contactCache.store(a, b, ni, 0, normal.x, normal.y);
    } else {
        auto& tfA = transforms.get(a);
        auto& tfB = transforms.get(b);
        float invA = m_ecs.hasComponent<PhysicsComponent>(a) ? m_ecs.getComponent<PhysicsComponent>(a).invMass : 0.0f;
        float invB = m_ecs.hasComponent<PhysicsComponent>(b) ? m_ecs.getComponent<PhysicsComponent>(b).invMass : 0.0f;
        float totalInv = invA + invB;
        float ratioA = totalInv > 0.0f ? invA / totalInv : 0.5f;
        float ratioB = totalInv > 0.0f ? invB / totalInv : 0.5f;
        tfA.transform.position += pushDir * (mtvMag * ratioA);
        tfB.transform.position += normal * (mtvMag * ratioB);
        float ni1 = resolveVelocityWithFriction(a, pushDir, b);
        float ni2 = resolveVelocityWithFriction(b, normal, a);
        m_contactCache.store(a, b, (ni1 + ni2) * 0.5f, 0, normal.x, normal.y);
    }
}

void CollisionSystem::applyWarmImpulse(Entity entity, const math::Vector2D& normal, float impulse) {
    if (!m_ecs.hasComponent<PhysicsComponent>(entity)) return;
    auto& phys = m_ecs.getComponent<PhysicsComponent>(entity);
    if (phys.invMass == 0.0f) return;

    phys.velocity += normal * impulse * phys.invMass * 0.1f;
}

float CollisionSystem::resolveVelocityWithFriction(Entity entity, const math::Vector2D& normal, Entity other) {
    if (!m_ecs.hasComponent<PhysicsComponent>(entity)) return 0.0f;
    auto& phys = m_ecs.getComponent<PhysicsComponent>(entity);

    float velAlongNormal = phys.velocity.dot(normal);
    if (velAlongNormal > 0) return 0.0f;

    float restitution = phys.restitution;
    float normalImpulse = velAlongNormal * (1.0f + restitution);
    phys.velocity -= normal * normalImpulse;

    math::Vector2D tangent = phys.velocity - normal * phys.velocity.dot(normal);
    float tangentSpeed = tangent.magnitude();

    if (tangentSpeed > 0.1f) {
        float friction = phys.friction;
        if (m_ecs.hasComponent<PhysicsComponent>(other)) {
            friction = (friction + m_ecs.getComponent<PhysicsComponent>(other).friction) * 0.5f;
        }

        float frictionImpulse = friction * math::MathUtils::abs(velAlongNormal);
        math::Vector2D tangentDir = tangent * (1.0f / tangentSpeed);

        if (tangentSpeed < frictionImpulse) {
            phys.velocity -= tangent;
        } else {
            phys.velocity -= tangentDir * frictionImpulse;
        }
    }

    float absImpulse = math::MathUtils::abs(normalImpulse);
    if (absImpulse > 5.0f) {
        ecs::PhysicsOps::wake(phys);
    }
    return absImpulse;
}

} // namespace ecs
} // namespace engine
