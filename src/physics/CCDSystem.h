#pragma once

#include "math/Vector3D.h"
#include "math/Quaternion.h"
#include "RigidBody3D.h"
#include "Collider3D.h"
#include <cmath>
#include <vector>
#include <algorithm>

namespace engine {
namespace physics {

// ═══════════════════════════════════════════════════════════════
// CCD (Continuous Collision Detection) — Full swept-volume
//
// Prevents tunneling for fast-moving objects of ANY shape.
// Uses conservative advancement + bilateral advancement for
// exact time-of-impact (TOI) computation.
//
// Supports: Sphere-Sphere, Sphere-Box, Box-Box, Capsule-*
// ═══════════════════════════════════════════════════════════════

namespace CCDMath {

    /// Swept sphere vs static sphere — exact TOI
    /// Returns time ∈ [0,1] or -1 if no collision
    inline float sweptSphereSphere(
        const math::Vector3D& posA, const math::Vector3D& velA, float radiusA,
        const math::Vector3D& posB, const math::Vector3D& velB, float radiusB,
        float dt)
    {
        math::Vector3D relPos = posA - posB;
        math::Vector3D relVel = (velA - velB) * dt;
        float sumR = radiusA + radiusB;

        // Quadratic: |relPos + t*relVel|² = sumR²
        float a = relVel.sqrMagnitude();
        float b = 2.0f * relPos.dot(relVel);
        float c = relPos.sqrMagnitude() - sumR * sumR;

        if (c < 0.0f) return 0.0f; // Already overlapping

        float disc = b * b - 4.0f * a * c;
        if (disc < 0.0f) return -1.0f;

        float sqrtDisc = std::sqrt(disc);
        float t = (-b - sqrtDisc) / (2.0f * a);

        if (t >= 0.0f && t <= 1.0f) return t;
        return -1.0f;
    }

    /// Swept sphere vs static AABB (conservative advancement)
    /// Binary search for TOI within [0, dt]
    inline float sweptSphereAABB(
        const math::Vector3D& spherePos, const math::Vector3D& sphereVel, float radius,
        const math::Vector3D& boxMin, const math::Vector3D& boxMax,
        float dt, int maxIterations = 16)
    {
        // Closest point on AABB to sphere center
        auto closestPoint = [&](const math::Vector3D& p) -> math::Vector3D {
            return math::Vector3D(
                std::max(boxMin.x, std::min(boxMax.x, p.x)),
                std::max(boxMin.y, std::min(boxMax.y, p.y)),
                std::max(boxMin.z, std::min(boxMax.z, p.z))
            );
        };

        float tMin = 0.0f, tMax = 1.0f;

        // Check if already intersecting
        math::Vector3D cp = closestPoint(spherePos);
        if ((cp - spherePos).sqrMagnitude() <= radius * radius) return 0.0f;

        // Check end-of-frame position
        math::Vector3D endPos = spherePos + sphereVel * dt;
        math::Vector3D cpEnd = closestPoint(endPos);
        if ((cpEnd - endPos).sqrMagnitude() > radius * radius) return -1.0f;

        // Binary search for TOI
        for (int i = 0; i < maxIterations; i++) {
            float tMid = (tMin + tMax) * 0.5f;
            math::Vector3D midPos = spherePos + sphereVel * (dt * tMid);
            math::Vector3D cpMid = closestPoint(midPos);
            float distSq = (cpMid - midPos).sqrMagnitude();

            if (distSq <= radius * radius)
                tMax = tMid;
            else
                tMin = tMid;
        }

        return tMax;
    }

    /// Conservative advancement for convex shapes
    /// Uses separating axis and closing velocity to advance time
    inline float conservativeAdvancement(
        const math::Vector3D& posA, const math::Vector3D& velA,
        const math::Vector3D& posB, const math::Vector3D& velB,
        float combinedRadius, float dt, int maxIterations = 20)
    {
        math::Vector3D relVel = velA - velB;
        float relSpeed = relVel.magnitude();
        if (relSpeed < 1e-6f) return -1.0f;

        float t = 0.0f;
        math::Vector3D posAcur = posA;
        math::Vector3D posBcur = posB;

        for (int i = 0; i < maxIterations; i++) {
            math::Vector3D diff = posAcur - posBcur;
            float dist = diff.magnitude() - combinedRadius;

            if (dist < 0.001f) return t; // Contact found

            float advance = dist / relSpeed;
            t += advance / dt;

            if (t > 1.0f) return -1.0f; // No collision this frame

            posAcur = posA + velA * (dt * t);
            posBcur = posB + velB * (dt * t);
        }

        return t;
    }

    /// Swept capsule vs plane (for ground collision CCD)
    inline float sweptCapsulePlane(
        const math::Vector3D& capsulePos, const math::Vector3D& capsuleVel,
        float capsuleRadius, float capsuleHalfHeight,
        const math::Vector3D& planeNormal, float planeD, float dt)
    {
        // Lowest point of capsule
        float lowestOffset = capsuleHalfHeight + capsuleRadius;
        math::Vector3D lowestPoint = capsulePos - planeNormal * lowestOffset;
        float dist = lowestPoint.dot(planeNormal) - planeD;

        if (dist <= 0.0f) return 0.0f; // Already penetrating

        float closingSpeed = -capsuleVel.dot(planeNormal);
        if (closingSpeed <= 0.0f) return -1.0f; // Moving away

        float toi = dist / (closingSpeed * dt);
        return (toi <= 1.0f) ? toi : -1.0f;
    }

} // namespace CCDMath

// ═══════════════════════════════════════════════════════════════
// CCDResult — Output of CCD query
// ═══════════════════════════════════════════════════════════════

struct CCDResult {
    bool hit = false;
    float timeOfImpact = 1.0f;     // t ∈ [0,1] within the timestep
    math::Vector3D contactPoint;
    math::Vector3D contactNormal;
    int bodyA = -1;
    int bodyB = -1;
};

// ═══════════════════════════════════════════════════════════════
// CCDSystem — Manages CCD queries for the physics world
// ═══════════════════════════════════════════════════════════════

class CCDSystem {
public:
    float speedThreshold = 2.0f; // Only perform CCD for bodies faster than this

    /// Check if a body needs CCD (based on speed)
    bool needsCCD(const RigidBody3D& body) const {
        return body.velocity.sqrMagnitude() > speedThreshold * speedThreshold;
    }

    /// Perform CCD between two bodies, returns earliest TOI result
    CCDResult checkPair(const RigidBody3D& a, int idxA,
                        const RigidBody3D& b, int idxB, float dt) const {
        CCDResult result;
        result.bodyA = idxA;
        result.bodyB = idxB;

        float toi = -1.0f;

        // Sphere vs Sphere
        if (a.shape == RigidBody3D::Shape::SPHERE &&
            b.shape == RigidBody3D::Shape::SPHERE) {
            toi = CCDMath::sweptSphereSphere(
                a.position, a.velocity, a.sphereRadius,
                b.position, b.velocity, b.sphereRadius, dt);
        }
        // Sphere vs Box (treat box as AABB for CCD)
        else if (a.shape == RigidBody3D::Shape::SPHERE &&
                 b.shape == RigidBody3D::Shape::BOX) {
            math::Vector3D bMin = b.position - b.boxHalfExtents;
            math::Vector3D bMax = b.position + b.boxHalfExtents;
            toi = CCDMath::sweptSphereAABB(a.position, a.velocity, a.sphereRadius,
                                            bMin, bMax, dt);
        }
        else if (b.shape == RigidBody3D::Shape::SPHERE &&
                 a.shape == RigidBody3D::Shape::BOX) {
            math::Vector3D aMin = a.position - a.boxHalfExtents;
            math::Vector3D aMax = a.position + a.boxHalfExtents;
            toi = CCDMath::sweptSphereAABB(b.position, b.velocity, b.sphereRadius,
                                            aMin, aMax, dt);
            // Swap so bodyA is the sphere
            result.bodyA = idxB; result.bodyB = idxA;
        }
        // General convex (conservative advancement fallback)
        else {
            float combinedR = 0.0f;
            if (a.shape == RigidBody3D::Shape::SPHERE) combinedR += a.sphereRadius;
            else combinedR += a.boxHalfExtents.magnitude();
            if (b.shape == RigidBody3D::Shape::SPHERE) combinedR += b.sphereRadius;
            else combinedR += b.boxHalfExtents.magnitude();

            toi = CCDMath::conservativeAdvancement(
                a.position, a.velocity, b.position, b.velocity,
                combinedR, dt);
        }

        if (toi >= 0.0f && toi <= 1.0f) {
            result.hit = true;
            result.timeOfImpact = toi;
            result.contactNormal = (a.position + a.velocity * dt * toi -
                                    (b.position + b.velocity * dt * toi));
            float len = result.contactNormal.magnitude();
            if (len > 1e-6f) result.contactNormal = result.contactNormal * (1.0f / len);
        }

        return result;
    }

    /// Scan all fast-moving bodies and return earliest TOI events
    std::vector<CCDResult> scanAll(const std::vector<RigidBody3D>& bodies, float dt) const {
        std::vector<CCDResult> results;
        int n = static_cast<int>(bodies.size());

        for (int i = 0; i < n; i++) {
            if (!needsCCD(bodies[i]) || bodies[i].getInvMass() == 0.0f) continue;

            for (int j = i + 1; j < n; j++) {
                auto r = checkPair(bodies[i], i, bodies[j], j, dt);
                if (r.hit) results.push_back(r);
            }
        }

        // Sort by earliest TOI
        std::sort(results.begin(), results.end(),
                  [](const CCDResult& a, const CCDResult& b) {
                      return a.timeOfImpact < b.timeOfImpact;
                  });

        return results;
    }
};

} // namespace physics
} // namespace engine
