#pragma once

#include "RigidBody3D.h"
#include "CollisionSolver3D.h"
#include "ContactCache.h"
#include "DynamicBVH3D.h"
#include "Constraints3D.h"
#include "Thermodynamics.h"
#include "FluidSystem.h"
#include "Electromagnetism.h"
#include "SoftBody3D.h"
#include "GravityNBody.h"
#include "WaveSystem.h"
#include "math/Vector3D.h"
#include <vector>
#include <functional>
#include "core/JobSystem.h"

namespace engine {
namespace physics {

/// PhysicsWorld3D — Complete 3D physics simulation.
///
/// Pipeline per step:
///   1. Apply gravity + external forces
///   2. Broadphase (SpatialHash3D → potential pairs)
///   3. Narrowphase (collision detection → Contact3D list)
///   4. Pre-step solver (compute effective masses, warm start)
///   5. Iterative solve (sequential impulse + constraints)
///   6. Integrate (semi-implicit Euler with gyroscopic correction)
///   7. Position correction (direct penetration resolution)
///   8. Sleep check (EMA filtered energy)
///
class PhysicsWorld3D {
public:
    PhysicsWorld3D() : m_broadphase() {}

    // ── Configuration ──────────────────────────────────────────

    math::Vector3D gravity = math::Vector3D(0, -9.81f, 0);

    /// Sub-steps per frame (more = more stable but slower)
    int subSteps = 4;

    /// Solver iterations per sub-step
    void setSolverIterations(int iters) { m_solver.iterations = iters; }

    /// Enable/disable speculative CCD
    bool enableCCD = false;
    float ccdThreshold = 2.0f;  // Bodies faster than this (m/s) get CCD

    // ── Body Management ────────────────────────────────────────

    /// Add a dynamic sphere
    int addDynamicSphere(const math::Vector3D& pos, float radius, float mass,
                         float restitution = 0.3f, float friction = 0.5f)
    {
        RigidBody3D body = RigidBody3D::dynamic(mass);
        body.position = pos;
        body.shape = RigidBody3D::Shape::SPHERE;
        body.sphereRadius = radius;
        body.setSphereInertia(radius);
        body.material.restitution = restitution;
        body.material.staticFriction = friction;
        body.material.kineticFriction = friction * 0.75f;
        return allocateBody(std::move(body));
    }

    /// Add a dynamic box
    int addDynamicBox(const math::Vector3D& pos, const math::Vector3D& halfExtents,
                      float mass, float restitution = 0.3f, float friction = 0.5f)
    {
        RigidBody3D body = RigidBody3D::dynamic(mass);
        body.position = pos;
        body.shape = RigidBody3D::Shape::BOX;
        body.boxHalfExtents = halfExtents;
        body.setBoxInertia(halfExtents.x * 2, halfExtents.y * 2, halfExtents.z * 2);
        body.material.restitution = restitution;
        body.material.staticFriction = friction;
        body.material.kineticFriction = friction * 0.75f;
        return allocateBody(std::move(body));
    }

    /// Add a static box (infinite mass, immovable)
    int addStaticBox(const math::Vector3D& pos, const math::Vector3D& halfExtents,
                     float friction = 0.6f)
    {
        RigidBody3D body = RigidBody3D::staticBody();
        body.position = pos;
        body.shape = RigidBody3D::Shape::BOX;
        body.boxHalfExtents = halfExtents;
        body.material.staticFriction = friction;
        body.material.kineticFriction = friction * 0.75f;
        return allocateBody(std::move(body));
    }

    /// Add a static plane (infinite mass, represented as a thin box)
    int addStaticPlane(float y = 0.0f, float friction = 0.6f) {
        return addStaticBox(
            math::Vector3D(0, y - 0.5f, 0),
            math::Vector3D(500.0f, 0.5f, 500.0f),
            friction
        );
    }

    /// Add a static heightfield terrain
    int addStaticHeightfield(const math::Vector3D& pos, std::shared_ptr<HeightfieldCollider> hf, float friction = 0.6f)
    {
        RigidBody3D body = RigidBody3D::staticBody();
        body.position = pos;
        body.shape = RigidBody3D::Shape::HEIGHTFIELD;
        body.heightfield = hf;
        body.material.staticFriction = friction;
        body.material.kineticFriction = friction * 0.75f;
        return allocateBody(std::move(body));
    }

    /// Add a dynamic capsule (for characters, vehicle chassis, etc.)
    int addDynamicCapsule(const math::Vector3D& pos, float height, float radius,
                          float mass, float restitution = 0.3f, float friction = 0.5f)
    {
        RigidBody3D body = RigidBody3D::dynamic(mass);
        body.position = pos;
        body.shape = RigidBody3D::Shape::CAPSULE;
        body.capsuleHeight = height;
        body.capsuleRadius = radius;
        body.setCapsuleInertia(height, radius);
        body.material.restitution = restitution;
        body.material.staticFriction = friction;
        body.material.kineticFriction = friction * 0.75f;
        return allocateBody(std::move(body));
    }

    /// Add a dynamic convex hull
    int addDynamicConvexHull(const math::Vector3D& pos,
                             std::shared_ptr<ConvexHullCollider> hull,
                             float mass, float restitution = 0.3f, float friction = 0.5f)
    {
        RigidBody3D body = RigidBody3D::dynamic(mass);
        body.position = pos;
        body.shape = RigidBody3D::Shape::CONVEX_HULL;
        body.convexHull = hull;
        body.setConvexHullInertia();
        body.material.restitution = restitution;
        body.material.staticFriction = friction;
        body.material.kineticFriction = friction * 0.75f;
        return allocateBody(std::move(body));
    }

    RigidBody3D& getBody(int index) { return m_bodies[index]; }
    const RigidBody3D& getBody(int index) const { return m_bodies[index]; }
    void setGravity(const math::Vector3D& g) { gravity = g; }
    void setJobSystem(core::JobSystem* js) { m_jobSystem = js; }
    void setThermalSystem(ThermalSystem* ts) { m_thermalSystem = ts; }
    void setFluidSystem(FluidSystem* fs) { m_fluidSystem = fs; }
    void setEMSystem(EMSystem* em) { m_emSystem = em; }
    void setSoftBodySystem(SoftBodySystem* sb) { m_softBodySystem = sb; }
    void setGravityNBodySystem(GravityNBodySystem* gn) { m_gravityNBodySystem = gn; }
    void setWaveSystem(WaveSystem* ws) { m_waveSystem = ws; }

    const std::vector<RigidBody3D>& getBodies() const { return m_bodies; }
    int getBodyCount() const { return static_cast<int>(m_bodies.size()); }

    /// Remove a body by index (marks slot for recycling)
    void removeBody(int index) {
        if (index < 0 || index >= static_cast<int>(m_bodies.size())) return;
        if (m_bodies[index].m_removed) return;
        // Remove from BVH
        if (m_bodies[index].bvhNodeId != -1) {
            m_broadphase.remove(m_bodies[index].bvhNodeId);
            m_bodies[index].bvhNodeId = -1;
        }
        // Mark as removed and neutralize
        m_bodies[index].m_removed = true;
        m_bodies[index].setType(RigidBody3D::Type::STATIC);
        m_bodies[index].velocity = math::Vector3D::Zero;
        m_bodies[index].angularVelocity = math::Vector3D::Zero;
        m_freeSlots.push_back(index);
    }

    /// Check if a body index is still valid (not removed)
    bool isBodyValid(int index) const {
        return index >= 0 && index < static_cast<int>(m_bodies.size()) && !m_bodies[index].m_removed;
    }

    // ── Constraint API ─────────────────────────────────────────

    /// Add a distance constraint (rope, spring)
    int addDistanceConstraint(int bodyA, int bodyB,
                              const math::Vector3D& anchorA,
                              const math::Vector3D& anchorB,
                              float restLength, float stiffness = 1.0f)
    {
        return m_constraintSolver.addDistance(bodyA, bodyB, anchorA, anchorB, restLength, stiffness);
    }

    /// Add a ball-socket joint (ragdoll, pendulum)
    int addBallSocketJoint(int bodyA, int bodyB,
                           const math::Vector3D& anchorA,
                           const math::Vector3D& anchorB)
    {
        return m_constraintSolver.addBallSocket(bodyA, bodyB, anchorA, anchorB);
    }

    /// Add a hinge joint (wheel, door, axle)
    int addHingeJoint(int bodyA, int bodyB,
                      const math::Vector3D& anchorA,
                      const math::Vector3D& anchorB,
                      const math::Vector3D& axisA = math::Vector3D(0,1,0),
                      const math::Vector3D& axisB = math::Vector3D(0,1,0))
    {
        return m_constraintSolver.addHinge(bodyA, bodyB, anchorA, anchorB, axisA, axisB);
    }

    // ── Simulation Step ────────────────────────────────────────

    void step(float dt);  // implementation in PhysicsWorld3D.cpp

    // ── Queries ───────────────────────────────────────

    /// Raycast against all bodies, returns closest hit
    RayHit3D raycast(const Ray3D& ray, int& hitBodyIndex) const;

    // ── Stats ──────────────────────────────────────────────────

    int getContactCount() const { return static_cast<int>(m_contacts.size()); }

    /// Callback for contact events (fires once per step with accumulated impulse)
    using ContactCallback = std::function<void(int bodyA, int bodyB, const math::Vector3D& point, float impulse)>;
    ContactCallback onContact = nullptr;

private:
    std::vector<RigidBody3D> m_bodies;
    std::vector<Contact3D> m_contacts;
    std::vector<Contact3D> m_prevContacts;
    std::vector<int> m_freeSlots;  // Recycled body indices
    DynamicBVH3D m_broadphase;
    CollisionSolver3D m_solver;
    ConstraintSolver3D m_constraintSolver;

    core::JobSystem* m_jobSystem = nullptr;
    ThermalSystem* m_thermalSystem = nullptr;
    FluidSystem* m_fluidSystem = nullptr;
    EMSystem* m_emSystem = nullptr;
    SoftBodySystem* m_softBodySystem = nullptr;
    GravityNBodySystem* m_gravityNBodySystem = nullptr;
    WaveSystem* m_waveSystem = nullptr;

    // Contact cache and constraint state are managed internally by solvers
    uint32_t computeContactHash(int bodyA, int bodyB, const math::Vector3D& contactPoint);

    /// Solve a single contact — in PhysicsWorld3D.cpp (hot path)
    void solveSingleContact(Contact3D& c);

    /// Narrowphase: test collision between two bodies
    ContactInfo narrowphaseTest(int a, int b);

    /// Allocate a body slot (reuses free slots if available)
    int allocateBody(RigidBody3D&& body) {
        if (!m_freeSlots.empty()) {
            int slot = m_freeSlots.back();
            m_freeSlots.pop_back();
            m_bodies[slot] = std::move(body);
            m_bodies[slot].m_removed = false;
            return slot;
        }
        m_bodies.push_back(std::move(body));
        return static_cast<int>(m_bodies.size()) - 1;
    }
};

} // namespace physics
} // namespace engine
