#pragma once

#include "SystemManager.h"
#include "ECSCoordinator.h"
#include "Components.h"
#include "SpatialHash.h"
#include "ContactCache.h"
#include "EventBus.h"

namespace engine {
namespace ecs {

class CollisionSystem : public System {
public:
    CollisionSystem(ECSCoordinator& ecs);

    void update(float dt) override;
    void setEventBus(core::EventBus* bus);

    int getBroadPhaseTests() const { return m_broadPhaseTests; }
    int getNarrowPhaseTests() const { return m_narrowPhaseTests; }
    int getCollisionsResolved() const { return m_collisionsResolved; }
    int getTriggersDetected() const { return m_triggersDetected; }
    int getSpatialHashCells() const { return m_spatialHash.getCellCount(); }
    int getWarmCachedContacts() const { return m_contactCache.getCachedContacts(); }
    float getWarmStartRatio() const { return m_contactCache.getWarmStartRatio(); }

    void setCellSize(float size) { m_spatialHash.setCellSize(size); }

private:
    void resolveCollision(Entity a, ColliderComponent& colA, Entity b, ColliderComponent& colB);
    void applyWarmImpulse(Entity entity, const math::Vector2D& normal, float impulse);
    float resolveVelocityWithFriction(Entity entity, const math::Vector2D& normal, Entity other);

    ECSCoordinator& m_ecs;
    physics::SpatialHash m_spatialHash;
    physics::ContactCache m_contactCache;
    core::EventBus* m_eventBus = nullptr;
    std::vector<Entity> m_activeEntities;

    int m_broadPhaseTests = 0;
    int m_narrowPhaseTests = 0;
    int m_collisionsResolved = 0;
    int m_triggersDetected = 0;
};

} // namespace ecs
} // namespace engine
