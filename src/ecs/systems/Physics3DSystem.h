#pragma once

#include "../ECSCoordinator.h"
#include "../Components3D.h"
#include "../../physics/Collider3D.h"
#include "../../physics/SpatialHash3D.h"
#include <cmath>
#include <vector>
#include <algorithm>

namespace engine {
namespace ecs {

/// Physics3DSystem — Production-grade 3D physics for ECS.
///
/// Incorporates techniques from PhysicsWorld3D into the ECS pipeline:
///   - SpatialHash3D broadphase (O(n) vs O(n²))
///   - OBB narrowphase (SAT 15-axis for rotated boxes)
///   - Sub-stepping (configurable, default 2)
///   - Iterative impulse solver (10 iterations with accumulated clamping)
///   - Contact warm-starting (hash-based, zero-alloc)
///   - Physics sleeping (EMA energy filter, 0.5s threshold)
///   - Gyroscopic torque correction
///   - Frame-rate independent exponential damping
///   - Pre-reserved buffers (zero heap alloc per frame)
///
class Physics3DSystem {
public:
    explicit Physics3DSystem(ECSCoordinator& ecs)
        : m_ecs(ecs), m_spatialHash(2.5f) {
        m_entities.reserve(256);
        m_contacts.reserve(256);
        m_prevContacts.reserve(256);
    }

    // ── Configuration ──────────────────────────────────────────
    void setGravity(const math::Vector3D& g) { m_gravity = g; }
    void setWorldBounds(const math::Vector3D& bmin, const math::Vector3D& bmax) {
        m_boundsMin = bmin; m_boundsMax = bmax; m_hasBounds = true;
    }
    void setFloorY(float y) { m_floorY = y; m_hasFloor = true; }
    void setCellSize(float s) { m_spatialHash.setCellSize(s); }
    void setSubSteps(int n) { m_subSteps = std::max(1, n); }
    void setSolverIterations(int n) { m_solverIterations = std::max(1, n); }
    void setSleepEnabled(bool on) { m_sleepEnabled = on; }
    void setSleepThreshold(float t) { m_sleepThreshold = t; }
    void setMaxLinearVelocity(float v) { m_maxLinVel = v; m_maxLinVelSq = v * v; }
    void setMaxAngularVelocity(float v) { m_maxAngVel = v; m_maxAngVelSq = v * v; }

    // ── Update ─────────────────────────────────────────────────
    void update(float dt);  // implementation in Physics3DSystem.cpp

    // ── Stats ──────────────────────────────────────────────────
    int getCollisionCount()   const { return m_collisionCount; }
    int getBroadphasePairs()  const { return m_broadphasePairs; }
    int getNarrowphaseTests() const { return m_narrowphaseTests; }
    int getSleepingCount()    const { return m_sleepingCount; }

private:
    // ── Contact for iterative solver ──────────────────────────
    struct SolverContact {
        uint32_t idxA = 0, idxB = 0;  // Indices into m_entities
        math::Vector3D normal;
        math::Vector3D contactPoint;
        float penetration = 0.0f;

        // Solver state
        math::Vector3D rA, rB;        // Contact arms
        math::Vector3D tangent1, tangent2;
        float normalMass = 0.0f;
        float tangentMass1 = 0.0f, tangentMass2 = 0.0f;
        float normalImpulse = 0.0f;
        float tangentImpulse1 = 0.0f, tangentImpulse2 = 0.0f;
        float bias = 0.0f;
        float restitution = 0.0f;
        float friction = 0.0f;
        uint32_t contactHash = 0;
    };

    // ── Cache struct for physics entities ──────────────────────
    struct PhysEntity {
        Entity e{};
        Transform3DComponent* t = nullptr;
        Physics3DComponent* p = nullptr;
        Collider3DComponent* c = nullptr;
    };

    // ── Private methods (implemented in Physics3DSystem.cpp) ───
    void collectEntities();
    void integrate(float dt);
    physics::AABB3D computeAABB(const Transform3DComponent& t, const Collider3DComponent& c) const;
    physics::OBB3D buildOBB(const Transform3DComponent& t, const Collider3DComponent& c) const;
    physics::ContactInfo doNarrowphase(const PhysEntity& a, const PhysEntity& b) const;
    void broadphase();
    uint32_t computeContactHash(uint32_t a, uint32_t b, const math::Vector3D& p) const;
    float computeEffectiveMass(const Physics3DComponent& pA, const Physics3DComponent& pB,
                               const math::Vector3D& rA, const math::Vector3D& rB,
                               const math::Vector3D& dir) const;
    void narrowphaseAndSolve(float dt);
    void solveConstraints(float dt);
    void solveDistance(Transform3DComponent& tA, Physics3DComponent& pA,
                       Transform3DComponent& tB, Physics3DComponent& pB,
                       ConstraintComponent& con, const math::Vector3D& wA,
                       const math::Vector3D& wB, float invDt);
    void solveBallSocket(Transform3DComponent& tA, Physics3DComponent& pA,
                         Transform3DComponent& tB, Physics3DComponent& pB,
                         ConstraintComponent& con, const math::Vector3D& wA,
                         const math::Vector3D& wB, float invDt);
    void solveHingeAngular(Transform3DComponent& tA, Physics3DComponent& pA,
                           Transform3DComponent& tB, Physics3DComponent& pB,
                           ConstraintComponent& con);
    void updateSleep(float dt);
    void applyBounds();


    // ── Members ────────────────────────────────────────────────
    ECSCoordinator& m_ecs;
    physics::SpatialHash3D m_spatialHash;

    std::vector<PhysEntity> m_entities;
    std::vector<SolverContact> m_contacts;
    std::vector<SolverContact> m_prevContacts;

    math::Vector3D m_gravity = {0, -9.81f, 0};
    math::Vector3D m_boundsMin = {-10, -1, -10};
    math::Vector3D m_boundsMax = { 10, 50,  10};
    float m_floorY = 0.0f;
    bool m_hasFloor = false;
    bool m_hasBounds = false;
    int m_subSteps = 2;
    int m_solverIterations = 10;
    bool m_sleepEnabled = true;
    float m_sleepThreshold = 0.01f;   // Configurable: energy below this → start sleep timer
    float m_sleepTime = 0.5f;         // Seconds below threshold before sleep
    float m_maxLinVel = 100.0f;       // Maximum linear velocity (m/s)
    float m_maxLinVelSq = 10000.0f;   // Squared cache
    float m_maxAngVel = 50.0f;        // Maximum angular velocity (rad/s)
    float m_maxAngVelSq = 2500.0f;    // Squared cache
    int m_collisionCount = 0;
    int m_broadphasePairs = 0;
    int m_narrowphaseTests = 0;
    int m_sleepingCount = 0;
};

} // namespace ecs
} // namespace engine
