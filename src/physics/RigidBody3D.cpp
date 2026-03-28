#include "RigidBody3D.h"
#include "PhysicsMath.h"
#include <cmath>
#include <algorithm>

namespace engine {
namespace physics {

// ── Static factories ─────────────────────────────────────────────

RigidBody3D RigidBody3D::dynamic(float mass) {
    RigidBody3D b; b.setMass(mass); b.m_type = Type::DYNAMIC; return b;
}

RigidBody3D RigidBody3D::staticBody() {
    RigidBody3D b; b.m_type = Type::STATIC;
    b.m_invMass = 0.0f; b.m_invInertia = math::Vector3D(0, 0, 0);
    return b;
}

// ── Mass & Inertia ───────────────────────────────────────────────

void RigidBody3D::setMass(float mass) {
    m_invMass = (mass <= 0.0f) ? 0.0f : 1.0f / mass;
}

void RigidBody3D::setBoxInertia(float w, float h, float d) {
    float mass = getMass(); if (mass <= 0) return;
    float k = mass / 12.0f;
    float Ix = k * (h*h + d*d), Iy = k * (w*w + d*d), Iz = k * (w*w + h*h);
    m_invInertia = math::Vector3D(1.0f/Ix, 1.0f/Iy, 1.0f/Iz);
}

void RigidBody3D::setSphereInertia(float radius) {
    float mass = getMass(); if (mass <= 0) return;
    float I = 0.4f * mass * radius * radius;
    m_invInertia = math::Vector3D(1.0f/I, 1.0f/I, 1.0f/I);
}

void RigidBody3D::setCapsuleInertia(float h, float r) {
    float m = 1.0f / m_invMass;
    float Iy  = m * r * r * 0.5f;
    float Ixz = (m / 12.0f) * (3.0f * r * r + h * h);
    m_invInertia = math::Vector3D(1.0f/Ixz, 1.0f/Iy, 1.0f/Ixz);
}

math::Vector3D RigidBody3D::applyInvInertiaWorld(const math::Vector3D& v) const {
    if (m_invMass == 0.0f) return math::Vector3D(0, 0, 0);
    math::Quaternion invRot(-m_orientation.x, -m_orientation.y, -m_orientation.z, m_orientation.w);
    math::Vector3D local = invRot.rotateVector(v);
    math::Vector3D scaled(local.x * m_invInertia.x, local.y * m_invInertia.y, local.z * m_invInertia.z);
    return m_orientation.rotateVector(scaled);
}

// ── Transform ────────────────────────────────────────────────────

math::Matrix4x4 RigidBody3D::getModelMatrix() const {
    math::Matrix4x4 r = m_orientation.toMatrix();
    r.set(0, 3, position.x); r.set(1, 3, position.y); r.set(2, 3, position.z);
    return r;
}

math::Matrix4x4 RigidBody3D::getModelMatrix(const math::Vector3D& scale) const {
    math::Matrix4x4 r = m_orientation.toMatrix();
    for (int row = 0; row < 3; row++) {
        r.set(row, 0, r.get(row, 0) * scale.x);
        r.set(row, 1, r.get(row, 1) * scale.y);
        r.set(row, 2, r.get(row, 2) * scale.z);
    }
    r.set(0, 3, position.x); r.set(1, 3, position.y); r.set(2, 3, position.z);
    return r;
}

// ── Force application ────────────────────────────────────────────

void RigidBody3D::applyForce(const math::Vector3D& force) {
    m_forceAccum += force;
}

void RigidBody3D::applyForceAtPoint(const math::Vector3D& force, const math::Vector3D& worldPoint) {
    m_forceAccum += force;
    m_torqueAccum += (worldPoint - position).cross(force);
}

void RigidBody3D::applyTorque(const math::Vector3D& torque) {
    m_torqueAccum += torque;
}

void RigidBody3D::applyImpulse(const math::Vector3D& impulse) {
    velocity += impulse * m_invMass;
}

void RigidBody3D::applyImpulseAtPoint(const math::Vector3D& impulse, const math::Vector3D& worldPoint) {
    velocity += impulse * m_invMass;
    angularVelocity += applyInvInertiaWorld((worldPoint - position).cross(impulse));
}

void RigidBody3D::clearForces() {
    m_forceAccum = math::Vector3D(0, 0, 0);
    m_torqueAccum = math::Vector3D(0, 0, 0);
}

// ── Integration (Velocity-Verlet + Aerodynamics) ────────────────

void RigidBody3D::integrate(float dt) {
    if (m_type != Type::DYNAMIC || m_invMass == 0.0f) return;

    // ── Aerodynamic Forces (skip when nearly stationary) ──────
    float velSq = velocity.x*velocity.x + velocity.y*velocity.y + velocity.z*velocity.z;
    if (velSq > 0.01f) {
        float area = material.crossSectionArea;
        if (area <= 0.0f) {
            if (shape == Shape::SPHERE)
                area = PhysicsMath::sphereCrossSection(sphereRadius);
            else if (shape == Shape::BOX)
                area = PhysicsMath::boxCrossSection(boxHalfExtents.x * 2.0f,
                                                    boxHalfExtents.y * 2.0f);
            else
                area = PhysicsMath::sphereCrossSection(capsuleRadius);
        }
        m_forceAccum += PhysicsMath::dragForce(
            velocity, PhysicsMath::AIR_DENSITY, material.dragCoefficient, area);

        // Magnus Effect (spinning objects curve through air)
        float angVelSq = angularVelocity.x*angularVelocity.x +
                         angularVelocity.y*angularVelocity.y +
                         angularVelocity.z*angularVelocity.z;
        if (angVelSq > 0.01f) {
            float magnusCoeff = 0.5f * PhysicsMath::AIR_DENSITY * area * 0.1f;
            m_forceAccum += PhysicsMath::magnusForce(
                velocity, angularVelocity, magnusCoeff);
        }
    }

    // ── Buoyancy (Archimedes' Principle) ──────────────────────
    if (m_waterLevel > -1e6f) {
        float depthBelowSurface = m_waterLevel - position.y;
        float volume = 0.0f;
        float submerged = 0.0f;
        if (shape == Shape::SPHERE) {
            volume = PhysicsMath::sphereVolume(sphereRadius);
            submerged = PhysicsMath::sphereSubmergedFraction(sphereRadius, depthBelowSurface);
        } else if (shape == Shape::BOX) {
            volume = PhysicsMath::boxVolume(boxHalfExtents.x*2, boxHalfExtents.y*2, boxHalfExtents.z*2);
            float halfH = boxHalfExtents.y;
            submerged = (depthBelowSurface <= -halfH) ? 0.0f :
                        (depthBelowSurface >= halfH) ? 1.0f :
                        (depthBelowSurface + halfH) / (2.0f * halfH);
        }
        if (submerged > 0.0f) {
            math::Vector3D buoyF = PhysicsMath::buoyancyForce(
                PhysicsMath::WATER_DENSITY, volume, submerged,
                PhysicsMath::GRAVITY_EARTH);
            m_forceAccum += buoyF;
            math::Vector3D waterDrag = PhysicsMath::dragForce(
                velocity, PhysicsMath::WATER_DENSITY * submerged,
                material.dragCoefficient * 2.0f,
                (shape == Shape::SPHERE)
                    ? PhysicsMath::sphereCrossSection(sphereRadius)
                    : PhysicsMath::boxCrossSection(boxHalfExtents.x*2, boxHalfExtents.y*2));
            m_forceAccum += waterDrag;
        }
    }

    // ── Velocity-Verlet Integration ──────────────────────────
    math::Vector3D accelOld = m_forceAccum * m_invMass;
    position = PhysicsMath::verletPositionStep(position, velocity, accelOld, dt);

    // Angular
    angularVelocity += applyInvInertiaWorld(m_torqueAccum) * dt;

    // Gyroscopic torque correction
    if (angularVelocity.dot(angularVelocity) > 1e-6f) {
        math::Quaternion invRot(-m_orientation.x, -m_orientation.y, -m_orientation.z, m_orientation.w);
        math::Vector3D wLocal = invRot.rotateVector(angularVelocity);
        math::Vector3D Iw(
            m_invInertia.x > 0 ? wLocal.x / m_invInertia.x : 0,
            m_invInertia.y > 0 ? wLocal.y / m_invInertia.y : 0,
            m_invInertia.z > 0 ? wLocal.z / m_invInertia.z : 0
        );
        math::Vector3D gyroWorld = m_orientation.rotateVector(wLocal.cross(Iw));
        angularVelocity -= applyInvInertiaWorld(gyroWorld) * dt;
    }

    // Velocity update (Verlet half-step)
    velocity += accelOld * dt;

    // Damping
    float linDamp = 1.0f / (1.0f + dt * m_linearDamping * 60.0f);
    float angDamp = 1.0f / (1.0f + dt * m_angularDamping * 60.0f);
    velocity = velocity * linDamp;
    angularVelocity = angularVelocity * angDamp;

    // Integrate orientation
    if (angularVelocity.dot(angularVelocity) > 1e-10f) {
        math::Quaternion omega(angularVelocity.x, angularVelocity.y, angularVelocity.z, 0.0f);
        math::Quaternion spin = (omega * m_orientation).scale(0.5f);
        m_orientation.x += spin.x * dt; m_orientation.y += spin.y * dt;
        m_orientation.z += spin.z * dt; m_orientation.w += spin.w * dt;
        m_orientation = m_orientation.normalized();
    }

    // Velocity clamping
    constexpr float maxLin = 100.0f, maxLinSq = maxLin * maxLin;
    float linSq = velocity.dot(velocity);
    if (linSq > maxLinSq) velocity = velocity * (maxLin / std::sqrt(linSq));

    constexpr float maxAng = 50.0f, maxAngSq = maxAng * maxAng;
    float angSq = angularVelocity.dot(angularVelocity);
    if (angSq > maxAngSq) angularVelocity = angularVelocity * (maxAng / std::sqrt(angSq));

    clearForces();
}

// ── Collision shape queries ──────────────────────────────────────

AABB3D RigidBody3D::getWorldAABB() const {
    if (shape == Shape::SPHERE)
        return AABB3D(position - math::Vector3D(sphereRadius, sphereRadius, sphereRadius),
                      position + math::Vector3D(sphereRadius, sphereRadius, sphereRadius));
    if (shape == Shape::CAPSULE)
        return getWorldCapsule().getAABB();
    if (shape == Shape::HEIGHTFIELD) {
        if (heightfield) {
            math::Vector3D offset(heightfield->numCols * heightfield->colSpacing * 0.5f, 
                                  0.0f, 
                                  heightfield->numRows * heightfield->rowSpacing * 0.5f);
            math::Vector3D min = position - offset;
            min.y += heightfield->minHeight;
            math::Vector3D max = position + offset;
            max.y += heightfield->maxHeight;
            return AABB3D(min, max);
        }
        return AABB3D(position, position);
    }
    if (shape == Shape::CONVEX_HULL && convexHull) {
        return getWorldConvexHull().getAABB();
    }
    // BOX
    math::Vector3D corners[3] = {
        m_orientation.rotateVector(math::Vector3D(boxHalfExtents.x, 0, 0)),
        m_orientation.rotateVector(math::Vector3D(0, boxHalfExtents.y, 0)),
        m_orientation.rotateVector(math::Vector3D(0, 0, boxHalfExtents.z))
    };
    math::Vector3D extent(
        std::abs(corners[0].x) + std::abs(corners[1].x) + std::abs(corners[2].x),
        std::abs(corners[0].y) + std::abs(corners[1].y) + std::abs(corners[2].y),
        std::abs(corners[0].z) + std::abs(corners[1].z) + std::abs(corners[2].z)
    );
    return AABB3D(position - extent, position + extent);
}

OBB3D RigidBody3D::getWorldOBB() const {
    OBB3D obb; obb.center = position; obb.halfExtents = boxHalfExtents;
    obb.axes[0] = m_orientation.rotateVector(math::Vector3D(1, 0, 0));
    obb.axes[1] = m_orientation.rotateVector(math::Vector3D(0, 1, 0));
    obb.axes[2] = m_orientation.rotateVector(math::Vector3D(0, 0, 1));
    return obb;
}

CapsuleCollider RigidBody3D::getWorldCapsule() const {
    math::Vector3D worldAxis = m_orientation.rotateVector(math::Vector3D(0, 1, 0));
    return CapsuleCollider(position, capsuleHeight, capsuleRadius, worldAxis);
}

// ── Type & sleep ─────────────────────────────────────────────────

void RigidBody3D::setType(Type type) {
    m_type = type;
    if (type == Type::STATIC) {
        m_invMass = 0.0f; m_invInertia = math::Vector3D(0, 0, 0);
        velocity = math::Vector3D(0, 0, 0); angularVelocity = math::Vector3D(0, 0, 0);
    }
}

void RigidBody3D::updateSleep(float dt) {
    float energy = velocity.dot(velocity) + angularVelocity.dot(angularVelocity);
    m_filteredEnergy = m_filteredEnergy * 0.8f + energy * 0.2f;
    if (m_filteredEnergy < m_sleepThreshold) {
        m_sleepTimer += dt;
        if (m_sleepTimer > 0.5f) {
            m_sleeping = true;
            velocity = math::Vector3D(0, 0, 0);
            angularVelocity = math::Vector3D(0, 0, 0);
        }
    } else { m_sleepTimer = 0.0f; m_sleeping = false; }
}

} // namespace physics
} // namespace engine
