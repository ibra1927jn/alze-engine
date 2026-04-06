#include "RigidBody3D.h"
#include "PhysicsMath.h"

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
    m_invInertia = math::Vector3D(Ix > 1e-12f ? 1.0f/Ix : 0.0f, Iy > 1e-12f ? 1.0f/Iy : 0.0f, Iz > 1e-12f ? 1.0f/Iz : 0.0f);
}

void RigidBody3D::setSphereInertia(float radius) {
    float mass = getMass(); if (mass <= 0) return;
    float I = 0.4f * mass * radius * radius;
    float invI = I > 1e-12f ? 1.0f/I : 0.0f;
    m_invInertia = math::Vector3D(invI, invI, invI);
}

void RigidBody3D::setCapsuleInertia(float h, float r) {
    if (m_invMass <= 0.0f) return;
    float m = 1.0f / m_invMass;
    // Capsule = cylinder of height cylH + two hemispheres of radius r
    float cylH = h - 2.0f * r;
    if (cylH < 0.0f) cylH = 0.0f;
    float cylVol = 3.14159265358979f * r * r * cylH;
    float sphVol = (4.0f / 3.0f) * 3.14159265358979f * r * r * r;
    float totalVol = cylVol + sphVol;
    if (totalVol < 1e-10f) { setSphereInertia(r); return; }
    float mCyl = m * cylVol / totalVol;
    float mSph = m * sphVol / totalVol;
    // Cylinder contributions
    float Iy_cyl = 0.5f * mCyl * r * r;
    float Ixz_cyl = mCyl * (3.0f * r * r + cylH * cylH) / 12.0f;
    // Two hemispheres with parallel axis theorem
    float halfM = mSph * 0.5f;
    float Iy_sph = 0.4f * mSph * r * r;
    float comOffset = cylH * 0.5f + 3.0f * r / 8.0f;
    float Ixz_sph = 2.0f * (0.4f * halfM * r * r + halfM * comOffset * comOffset);
    float Iy = Iy_cyl + Iy_sph;
    float Ixz = Ixz_cyl + Ixz_sph;
    constexpr float minI = 1e-8f;
    m_invInertia = math::Vector3D(
        Ixz > minI ? 1.0f / Ixz : 0.0f,
        Iy > minI ? 1.0f / Iy : 0.0f,
        Ixz > minI ? 1.0f / Ixz : 0.0f
    );
}

math::Vector3D RigidBody3D::applyInvInertiaWorld(const math::Vector3D& v) const {
    if (m_invMass == 0.0f) return math::Vector3D(0, 0, 0);
    // Transform to body space, apply diagonal inverse inertia, transform back
    math::Quaternion invRot = m_orientation.conjugate();
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

    // ── Velocity-Verlet Integration ─────────────────────────
    // Position step first (2nd-order accurate): x' = x + v*dt + 0.5*a*dt²
    math::Vector3D linearAccel = m_forceAccum * m_invMass;
    position = PhysicsMath::verletPositionStep(position, velocity, linearAccel, dt);

    // Angular: apply torque
    angularVelocity += applyInvInertiaWorld(m_torqueAccum) * dt;

    // ── Gyroscopic torque correction (implicit midpoint) ──────
    // Only apply for asymmetric inertia (spheres have zero gyroscopic torque).
    float angVelSq = angularVelocity.dot(angularVelocity);
    if (angVelSq > 1e-6f) {
        float invIx = m_invInertia.x, invIy = m_invInertia.y, invIz = m_invInertia.z;
        bool isAsymmetric = (std::abs(invIx - invIy) > 1e-6f * (invIx + invIy + 1e-10f)) ||
                            (std::abs(invIy - invIz) > 1e-6f * (invIy + invIz + 1e-10f));
        if (isAsymmetric) {
            math::Quaternion invRot = m_orientation.conjugate();
            math::Vector3D wLocal = invRot.rotateVector(angularVelocity);
            math::Vector3D Iw(
                invIx > 0 ? wLocal.x / invIx : 0,
                invIy > 0 ? wLocal.y / invIy : 0,
                invIz > 0 ? wLocal.z / invIz : 0
            );
            math::Vector3D gyroLocal = wLocal.cross(Iw);
            math::Vector3D gyroWorld = m_orientation.rotateVector(gyroLocal);
            angularVelocity -= applyInvInertiaWorld(gyroWorld) * (dt * 0.5f);
        }
    }

    // ── Damping (frame-rate independent) ─────────────────────
    float linDamp = 1.0f / (1.0f + dt * m_linearDamping * 60.0f);
    float angDamp = 1.0f / (1.0f + dt * m_angularDamping * 60.0f);
    velocity = velocity * linDamp;
    angularVelocity = angularVelocity * angDamp;

    // Velocity update (Verlet second half-step)
    velocity += linearAccel * dt;

    // ── Quaternion Integration (exponential map, 2nd order) ──
    angVelSq = angularVelocity.dot(angularVelocity);
    if (angVelSq > 1e-12f) {
        float angSpeed = std::sqrt(angVelSq);
        float halfAngle = angSpeed * dt * 0.5f;
        float sinHalf, cosHalf;
        if (halfAngle < 0.01f) {
            // 4th-order Taylor expansion for small angles
            float ha2 = halfAngle * halfAngle;
            sinHalf = halfAngle * (1.0f - ha2 * (1.0f / 6.0f - ha2 / 120.0f));
            cosHalf = 1.0f - ha2 * (0.5f - ha2 / 24.0f);
        } else {
            sinHalf = std::sin(halfAngle);
            cosHalf = std::cos(halfAngle);
        }
        float s = sinHalf / angSpeed;
        math::Quaternion deltaQ(
            angularVelocity.x * s, angularVelocity.y * s,
            angularVelocity.z * s, cosHalf
        );
        m_orientation = deltaQ * m_orientation;
    }

    // ── Quaternion renormalization (fast first-order correction) ──
    {
        float qSqr = m_orientation.x * m_orientation.x +
                      m_orientation.y * m_orientation.y +
                      m_orientation.z * m_orientation.z +
                      m_orientation.w * m_orientation.w;
        float err = qSqr - 1.0f;
        if (std::abs(err) > 1e-4f) {
            m_orientation = m_orientation.normalized();
        } else if (std::abs(err) > 1e-8f) {
            float correction = 1.0f - err * 0.5f;
            m_orientation = math::Quaternion(
                m_orientation.x * correction, m_orientation.y * correction,
                m_orientation.z * correction, m_orientation.w * correction
            );
        }
    }

    // ── Velocity clamping (safety limits) ────────────────────
    constexpr float maxLin = 500.0f, maxLinSq = maxLin * maxLin;
    float linSq = velocity.dot(velocity);
    if (linSq > maxLinSq) velocity = velocity * (maxLin / std::sqrt(linSq));

    constexpr float maxAng = 100.0f, maxAngSq = maxAng * maxAng;
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
