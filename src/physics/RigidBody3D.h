#pragma once

#include "math/Vector3D.h"
#include "math/Quaternion.h"
#include "math/Matrix4x4.h"
#include "Collider3D.h"
#include "PhysicsMaterial.h"
#include <memory>

namespace engine {
namespace physics {

class RigidBody3D {
public:
    enum class Type  { DYNAMIC, STATIC, KINEMATIC };

    RigidBody3D() = default;

    static RigidBody3D dynamic(float mass);
    static RigidBody3D staticBody();

    // Mass & Inertia
    void  setMass(float mass);
    float getMass() const { return m_invMass > 0 ? 1.0f / m_invMass : 0.0f; }
    float getInvMass() const { return m_invMass; }
    void  setBoxInertia(float w, float h, float d);
    void  setSphereInertia(float radius);
    void  setCapsuleInertia(float h, float r);
    math::Vector3D getInvInertia() const { return m_invInertia; }
    math::Vector3D applyInvInertiaWorld(const math::Vector3D& v) const;

    // Position & Orientation
    math::Vector3D   position;
    math::Quaternion m_orientation;
    void setOrientation(const math::Quaternion& q) { m_orientation = q.normalized(); }
    math::Quaternion getOrientation() const { return m_orientation; }
    math::Matrix4x4  getModelMatrix() const;
    math::Matrix4x4  getModelMatrix(const math::Vector3D& scale) const;

    // Velocity
    math::Vector3D velocity;
    math::Vector3D angularVelocity;

    // Forces
    void applyForce(const math::Vector3D& force);
    void applyForceAtPoint(const math::Vector3D& force, const math::Vector3D& worldPoint);
    void applyTorque(const math::Vector3D& torque);
    void applyImpulse(const math::Vector3D& impulse);
    void applyImpulseAtPoint(const math::Vector3D& impulse, const math::Vector3D& worldPoint);
    inline void applyImpulseAtArm(const math::Vector3D& impulse, const math::Vector3D& arm) {
        velocity += impulse * m_invMass;
        angularVelocity += applyInvInertiaWorld(arm.cross(impulse));
    }
    void clearForces();

    // Integration
    void integrate(float dt);

    enum class Shape { SPHERE, BOX, CAPSULE, HEIGHTFIELD, CONVEX_HULL };
    Shape shape = Shape::SPHERE;
    float sphereRadius = 0.5f;
    math::Vector3D boxHalfExtents = math::Vector3D(0.5f, 0.5f, 0.5f);
    float capsuleHeight = 2.0f;
    float capsuleRadius = 0.5f;
    std::shared_ptr<HeightfieldCollider> heightfield = nullptr;
    std::shared_ptr<ConvexHullCollider> convexHull = nullptr; // Local-space convex hull

    AABB3D          getWorldAABB() const;
    SphereCollider  getWorldSphere() const { return SphereCollider(position, sphereRadius); }
    CapsuleCollider getWorldCapsule() const;
    OBB3D           getWorldOBB() const;

    /// Get world-space convex hull (transforms local vertices by position + orientation)
    ConvexHullCollider getWorldConvexHull() const {
        ConvexHullCollider wc;
        if (!convexHull) return wc;
        wc.vertices.resize(convexHull->vertices.size());
        for (size_t i = 0; i < convexHull->vertices.size(); i++) {
            wc.vertices[i] = position + getOrientation().rotate(convexHull->vertices[i]);
        }
        wc.computeCenter();
        return wc;
    }

    /// Set convex hull inertia (approximates as bounding box of the hull)
    void setConvexHullInertia() {
        if (!convexHull || convexHull->vertices.empty()) return;
        AABB3D aabb = convexHull->getAABB();
        math::Vector3D size = aabb.max - aabb.min;
        setBoxInertia(size.x, size.y, size.z);
    }

    // Properties
    Type getType() const { return m_type; }
    void setType(Type type);
    bool isDynamic() const { return m_type == Type::DYNAMIC; }
    bool isStatic()  const { return m_type == Type::STATIC; }

    // Legacy accessors (delegate to material)
    float getRestitution() const { return material.restitution; }
    float getFriction() const { return material.staticFriction; }
    float m_linearDamping  = 0.01f;
    float m_angularDamping = 0.02f;
    void  setLinearDamping(float d) { m_linearDamping = d; }
    void  setAngularDamping(float d) { m_angularDamping = d; }
    float getAngularDamping() const { return m_angularDamping; }
    void  setWaterLevel(float level) { m_waterLevel = level; }
    float getWaterLevel() const { return m_waterLevel; }

    bool isBullet = false; // Enable Continuous Collision Detection (CCD)
    PhysicsMaterial material; // Physical material properties
    int userData = -1;
    int bvhNodeId = -1; // Node ID for Broadphase (DynamicBVH3D)
    int islandId = -1;  // Island ID for constraint solving
    bool m_removed = false; // Marked for removal (slot recycling)
    bool isSleeping() const { return m_sleeping; }
    void wake() { m_sleeping = false; m_sleepTimer = 0.0f; }
    void forceSleep() { m_sleeping = true; }
    void addSleepTimer(float dt) { m_sleepTimer += dt; }
    float getSleepTimer() const { return m_sleepTimer; }
    void updateSleep(float dt);

private:
    Type m_type = Type::DYNAMIC;
    float m_invMass = 1.0f;
    math::Vector3D m_invInertia = math::Vector3D(1, 1, 1);
    math::Vector3D m_forceAccum;
    math::Vector3D m_torqueAccum;
    bool  m_sleeping = false;
    float m_sleepTimer = 0.0f;
    float m_sleepThreshold = 0.01f;
    float m_filteredEnergy = 0.0f;
    float m_waterLevel = -1e7f;  // Below this = no buoyancy; set via setWaterLevel()
};

} // namespace physics
} // namespace engine
