// CollisionSolver3D.cpp
#include "CollisionSolver3D.h"
#include "PhysicsMaterial.h"
#include "PhysicsMath.h"
#include <algorithm>

namespace engine {
namespace physics {

void CollisionSolver3D::preStep(std::vector<Contact3D>& contacts,
                                 std::vector<RigidBody3D>& bodies, float dt)
{
    float invDt = dt > 0.0f ? 1.0f / dt : 0.0f;
    for (auto& c : contacts) {
        RigidBody3D& A = bodies[c.bodyA];
        RigidBody3D& B = bodies[c.bodyB];

        // ── Material-based restitution ────────────────────────
        c.restitution = PhysicsMaterial::combineRestitution(
            A.material.restitution, B.material.restitution);

        // ── Stribeck friction: compute effective μ ───────────
        // Pre-compute sliding speed for Stribeck curve evaluation
        c.rA = c.contactPoint - A.position;
        c.rB = c.contactPoint - B.position;
        math::Vector3D velA = A.velocity + A.angularVelocity.cross(c.rA);
        math::Vector3D velB = B.velocity + B.angularVelocity.cross(c.rB);
        math::Vector3D relVel = velB - velA;
        math::Vector3D tangentVel = relVel - c.normal * relVel.dot(c.normal);
        float slidingSpeed = std::sqrt(tangentVel.dot(tangentVel));

        // Stribeck curve: smooth static → kinetic transition
        float muA = A.material.stribeckFriction(slidingSpeed);
        float muB = B.material.stribeckFriction(slidingSpeed);
        c.friction = PhysicsMaterial::combineFriction(muA, muB);

        // Tangent basis computation
        if (std::abs(c.normal.x) >= 0.57735f)
            c.tangent1 = math::Vector3D(c.normal.y, -c.normal.x, 0.0f).normalized();
        else
            c.tangent1 = math::Vector3D(0.0f, c.normal.z, -c.normal.y).normalized();
        c.tangent2 = c.normal.cross(c.tangent1);

        // Effective masses
        c.normalMass   = computeEffectiveMass(A, B, c.rA, c.rB, c.normal);
        c.tangentMass1 = computeEffectiveMass(A, B, c.rA, c.rB, c.tangent1);
        c.tangentMass2 = computeEffectiveMass(A, B, c.rA, c.rB, c.tangent2);

        // Baumgarte bias
        c.bias = 0.0f;
        if (c.penetration > slop)
            c.bias = -baumgarte * invDt * (c.penetration - slop);
        float closingVel = computeRelativeVelocity(A, B, c.rA, c.rB, c.normal);
        if (-closingVel > restitutionThreshold)
            c.bias += c.restitution * closingVel;
        else
            c.restitution = 0.0f;

        // Warm-start
        if (c.normalImpulse != 0.0f || c.tangentImpulse1 != 0.0f || c.tangentImpulse2 != 0.0f) {
            math::Vector3D P = c.normal * c.normalImpulse +
                               c.tangent1 * c.tangentImpulse1 +
                               c.tangent2 * c.tangentImpulse2;
            A.applyImpulseAtPoint(P * -1.0f, c.contactPoint);
            B.applyImpulseAtPoint(P, c.contactPoint);
        }
    }
}

void CollisionSolver3D::solve(std::vector<Contact3D>& contacts,
                               std::vector<RigidBody3D>& bodies)
{
    for (int iter = 0; iter < iterations; iter++) {
        for (auto& c : contacts) {
            RigidBody3D& A = bodies[c.bodyA];
            RigidBody3D& B = bodies[c.bodyB];
            const math::Vector3D& rA = c.rA;
            const math::Vector3D& rB = c.rB;

            // ── Normal impulse ────────────────────────────────
            float vn = computeRelativeVelocity(A, B, rA, rB, c.normal);
            float lambda = c.normalMass * (-(vn + c.bias));
            float newImpulse = std::max(c.normalImpulse + lambda, 0.0f);
            lambda = newImpulse - c.normalImpulse;
            c.normalImpulse = newImpulse;
            math::Vector3D impulse = c.normal * lambda;
            A.applyImpulseAtPoint(impulse * -1.0f, c.contactPoint);
            B.applyImpulseAtPoint(impulse, c.contactPoint);

            // ── Tangent impulse (Coulomb cone) ────────────────
            float vt1 = computeRelativeVelocity(A, B, rA, rB, c.tangent1);
            float vt2 = computeRelativeVelocity(A, B, rA, rB, c.tangent2);
            float lt1 = c.tangentMass1 * (-vt1);
            float lt2 = c.tangentMass2 * (-vt2);
            float newT1 = c.tangentImpulse1 + lt1;
            float newT2 = c.tangentImpulse2 + lt2;
            float maxFriction = c.friction * c.normalImpulse;
            float tangentMag  = std::sqrt(newT1 * newT1 + newT2 * newT2);
            if (tangentMag > maxFriction && tangentMag > 1e-8f) {
                float scale = maxFriction / tangentMag;
                newT1 *= scale; newT2 *= scale;
            }
            lt1 = newT1 - c.tangentImpulse1;
            lt2 = newT2 - c.tangentImpulse2;
            c.tangentImpulse1 = newT1;
            c.tangentImpulse2 = newT2;
            math::Vector3D fImpulse = c.tangent1 * lt1 + c.tangent2 * lt2;
            A.applyImpulseAtPoint(fImpulse * -1.0f, c.contactPoint);
            B.applyImpulseAtPoint(fImpulse, c.contactPoint);

            // ── Rolling Friction Torque ───────────────────────
            // τ_roll = μr × N × r  (opposes angular velocity)
            float muRoll = PhysicsMaterial::combineFriction(
                A.material.rollingFriction, B.material.rollingFriction);
            if (muRoll > 0.0f && c.normalImpulse > 0.0f) {
                float radiusA = (A.shape == RigidBody3D::Shape::SPHERE) ? A.sphereRadius : 0.0f;
                float radiusB = (B.shape == RigidBody3D::Shape::SPHERE) ? B.sphereRadius : 0.0f;
                if (radiusA > 0.0f && A.isDynamic()) {
                    float rollTorque = PhysicsMath::rollingResistanceTorque(
                        muRoll, c.normalImpulse, radiusA);
                    float angSpeed = std::sqrt(A.angularVelocity.dot(A.angularVelocity));
                    if (angSpeed > 1e-6f) {
                        math::Vector3D braking = A.angularVelocity * (-rollTorque / angSpeed);
                        A.angularVelocity += braking;
                    }
                }
                if (radiusB > 0.0f && B.isDynamic()) {
                    float rollTorque = PhysicsMath::rollingResistanceTorque(
                        muRoll, c.normalImpulse, radiusB);
                    float angSpeed = std::sqrt(B.angularVelocity.dot(B.angularVelocity));
                    if (angSpeed > 1e-6f) {
                        math::Vector3D braking = B.angularVelocity * (-rollTorque / angSpeed);
                        B.angularVelocity += braking;
                    }
                }
            }
        }
    }
}

float CollisionSolver3D::computeEffectiveMass(const RigidBody3D& A, const RigidBody3D& B,
    const math::Vector3D& rA, const math::Vector3D& rB, const math::Vector3D& dir)
{
    float invMassSum = A.getInvMass() + B.getInvMass();
    math::Vector3D raCrossDir = rA.cross(dir);
    math::Vector3D rbCrossDir = rB.cross(dir);
    math::Vector3D angA = A.applyInvInertiaWorld(raCrossDir);
    math::Vector3D angB = B.applyInvInertiaWorld(rbCrossDir);
    float effectiveMass = invMassSum + angA.dot(raCrossDir) + angB.dot(rbCrossDir);
    return effectiveMass > 0.0f ? 1.0f / effectiveMass : 0.0f;
}

float CollisionSolver3D::computeRelativeVelocity(const RigidBody3D& A, const RigidBody3D& B,
    const math::Vector3D& rA, const math::Vector3D& rB, const math::Vector3D& dir)
{
    math::Vector3D velA = A.velocity + A.angularVelocity.cross(rA);
    math::Vector3D velB = B.velocity + B.angularVelocity.cross(rB);
    return (velB - velA).dot(dir);
}

} // namespace physics
} // namespace engine
