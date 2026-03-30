#pragma once

#include "math/Quaternion.h"
#include "math/Transform3D.h"
#include "math/Matrix4x4.h"
#include "renderer/MeshPrimitives.h"
#include "renderer/Material.h"

namespace engine {
namespace ecs {

// ════════════════════════════════════════════════════════════════
// 3D ECS Components
// ════════════════════════════════════════════════════════════════

/// Transform in 3D space (position, rotation, scale)
struct Transform3DComponent {
    math::Transform3D transform;
    math::Matrix4x4   worldMatrix = math::Matrix4x4::identity();
    bool dirty = true;

    Transform3DComponent() = default;
    explicit Transform3DComponent(const math::Vector3D& pos) {
        transform.position = pos;
    }
    Transform3DComponent(const math::Vector3D& pos, const math::Quaternion& rot) {
        transform.position = pos;
        transform.rotation = rot;
    }
};

/// Reference to a 3D mesh for rendering
struct MeshComponent {
    const renderer::Mesh3D* mesh = nullptr;
    renderer::Material material;
    bool visible = true;

    MeshComponent() = default;
    MeshComponent(const renderer::Mesh3D* m, const renderer::Material& mat)
        : mesh(m), material(mat) {}
};

/// 3D rigid body physics
struct Physics3DComponent {
    math::Vector3D velocity;
    math::Vector3D angularVelocity;
    math::Vector3D acceleration;
    float mass        = 1.0f;
    float invMass     = 1.0f;
    float drag        = 0.01f;
    float angularDrag = 0.05f;
    float restitution = 0.5f;
    float friction    = 0.4f;
    bool  isStatic    = false;
    bool  useGravity  = true;

    // Sleep state (bodies at rest skip simulation)
    bool  sleeping       = false;
    float sleepTimer     = 0.0f;
    float filteredEnergy = 1.0f;  // EMA of kinetic energy

    Physics3DComponent() = default;
    Physics3DComponent(float m, bool stat = false)
        : mass(m)
        , invMass(stat ? 0.0f : (m > 0.0001f ? 1.0f / m : 0.0f))
        , isStatic(stat)
    {}

    void applyForce(const math::Vector3D& force) {
        if (!isStatic && !sleeping) acceleration += force * invMass;
    }
    void applyImpulse(const math::Vector3D& impulse) {
        if (!isStatic) {
            velocity += impulse * invMass;
            sleeping = false;
            sleepTimer = 0.0f;
        }
    }
    void wake() {
        sleeping = false;
        sleepTimer = 0.0f;
        filteredEnergy = 1.0f;
    }
};

/// 3D collider (sphere, box, or capsule)
struct Collider3DComponent {
    enum Shape : uint8_t { SPHERE, BOX, CAPSULE };
    Shape  shape  = SPHERE;
    float  radius = 0.5f;                               // SPHERE / CAPSULE radius
    math::Vector3D halfExtents = {0.5f, 0.5f, 0.5f};   // BOX
    math::Vector3D offset;                               // Offset from transform
    float capsuleHeight = 2.0f;                          // CAPSULE total height
    math::Vector3D capsuleAxis = {0, 1, 0};              // CAPSULE local axis
    uint32_t layer = 1;
    uint32_t mask  = 0xFFFFFFFF;
    bool isTrigger = false;

    static Collider3DComponent sphere(float r) {
        Collider3DComponent c; c.shape = SPHERE; c.radius = r; return c;
    }
    static Collider3DComponent box(const math::Vector3D& he) {
        Collider3DComponent c; c.shape = BOX; c.halfExtents = he; return c;
    }
    static Collider3DComponent capsule(float height, float r,
                                        const math::Vector3D& axis = {0,1,0}) {
        Collider3DComponent c;
        c.shape = CAPSULE; c.capsuleHeight = height; c.radius = r;
        c.capsuleAxis = axis;
        return c;
    }
};

/// Point light attached to an entity
struct PointLightComponent {
    math::Vector3D color = {1, 1, 1};
    float constant  = 1.0f;
    float linear    = 0.09f;
    float quadratic = 0.032f;
};

/// Camera attached to an entity
struct CameraComponent {
    float fov       = 45.0f;
    float nearPlane = 0.1f;
    float farPlane  = 100.0f;
    bool  isActive  = false;
};

/// Tag component for identification
struct TagComponent {
    enum Type : uint8_t { NONE, PLAYER, ENEMY, PROJECTILE, GROUND, OBJECT };
    Type type = NONE;
    explicit TagComponent(Type t = NONE) : type(t) {}
};

/// Constraint between two entities (Distance, BallSocket, Hinge)
struct ConstraintComponent {
    enum Type : uint8_t { DISTANCE, BALL_SOCKET, HINGE };

    Type type = DISTANCE;
    Entity targetEntity = 0;             // The other entity (this entity is bodyA)
    math::Vector3D localAnchorA;         // Anchor in this entity's local space
    math::Vector3D localAnchorB;         // Anchor in target entity's local space
    math::Vector3D hingeAxis = {0,1,0};  // Hinge only: rotation axis (local space)
    float restLength = 1.0f;             // Distance only
    float stiffness  = 1.0f;            // [0..1] — 1=rigid, <1=soft spring
    bool  enabled    = true;

    // Solver state (accumulated for warm-start)
    float accImpulse[3] = {0, 0, 0};

    static ConstraintComponent distance(Entity target, float length,
                                         const math::Vector3D& ancA = {},
                                         const math::Vector3D& ancB = {},
                                         float stiff = 1.0f) {
        ConstraintComponent c;
        c.type = DISTANCE; c.targetEntity = target;
        c.localAnchorA = ancA; c.localAnchorB = ancB;
        c.restLength = length; c.stiffness = stiff;
        return c;
    }
    static ConstraintComponent ballSocket(Entity target,
                                           const math::Vector3D& ancA = {},
                                           const math::Vector3D& ancB = {}) {
        ConstraintComponent c;
        c.type = BALL_SOCKET; c.targetEntity = target;
        c.localAnchorA = ancA; c.localAnchorB = ancB;
        return c;
    }
    static ConstraintComponent hinge(Entity target,
                                      const math::Vector3D& ancA,
                                      const math::Vector3D& ancB,
                                      const math::Vector3D& axis = {0,1,0}) {
        ConstraintComponent c;
        c.type = HINGE; c.targetEntity = target;
        c.localAnchorA = ancA; c.localAnchorB = ancB;
        c.hingeAxis = axis;
        return c;
    }
};

} // namespace ecs
} // namespace engine
