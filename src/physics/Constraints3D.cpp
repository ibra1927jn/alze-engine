// Constraints3D.cpp — Implementación de DistanceConstraint, BallSocketConstraint,
// HingeConstraint y ConstraintSolver3D
#include "Constraints3D.h"
#include <algorithm>

namespace engine {
namespace physics {

// ── DistanceConstraint ────────────────────────────────────────────────
void DistanceConstraint::preStep(std::vector<RigidBody3D>& bodies, float dt) {
    RigidBody3D& A = bodies[bodyA];
    RigidBody3D& B = bodies[bodyB];
    math::Vector3D wA = A.position + A.getOrientation().rotateVector(localAnchorA);
    math::Vector3D wB = B.position + B.getOrientation().rotateVector(localAnchorB);
    math::Vector3D delta = wB - wA;
    float dist = delta.magnitude();
    m_n = (dist > 1e-6f) ? delta * (1.0f / dist) : math::Vector3D(0, 1, 0);
    if (dist <= 1e-6f) dist = 0.0f;
    math::Vector3D rA = wA - A.position;
    math::Vector3D rB = wB - B.position;
    math::Vector3D rAxN = rA.cross(m_n);
    math::Vector3D rBxN = rB.cross(m_n);
    float invMassSum = A.getInvMass() + B.getInvMass();
    float angA = A.applyInvInertiaWorld(rAxN).dot(rAxN);
    float angB = B.applyInvInertiaWorld(rBxN).dot(rBxN);
    float totalInvMass = invMassSum + angA + angB;
    m_effectiveMass = totalInvMass > 1e-8f ? 1.0f / totalInvMass : 0.0f;
    float error = dist - restLength;
    float invDt = 1.0f / dt;
    m_bias = -0.2f * invDt * error * stiffness;
    if (m_impulse != 0.0f) {
        math::Vector3D P = m_n * m_impulse;
        A.applyImpulseAtPoint(-P, wA);
        B.applyImpulseAtPoint(P, wB);
    }
}

void DistanceConstraint::solve(std::vector<RigidBody3D>& bodies) {
    if (!enabled) return;
    RigidBody3D& A = bodies[bodyA];
    RigidBody3D& B = bodies[bodyB];
    math::Vector3D wA = A.position + A.getOrientation().rotateVector(localAnchorA);
    math::Vector3D wB = B.position + B.getOrientation().rotateVector(localAnchorB);
    math::Vector3D rA = wA - A.position;
    math::Vector3D rB = wB - B.position;
    math::Vector3D vA = A.velocity + A.angularVelocity.cross(rA);
    math::Vector3D vB = B.velocity + B.angularVelocity.cross(rB);
    float vn = (vB - vA).dot(m_n);
    float lambda = m_effectiveMass * (-(vn + m_bias));
    m_impulse += lambda;
    math::Vector3D P = m_n * lambda;
    A.applyImpulseAtPoint(-P, wA);
    B.applyImpulseAtPoint(P, wB);
}

// ── BallSocketConstraint ──────────────────────────────────────────────
void BallSocketConstraint::preStep(std::vector<RigidBody3D>& bodies, float dt) {
    RigidBody3D& A = bodies[bodyA];
    RigidBody3D& B = bodies[bodyB];
    math::Vector3D wA = A.position + A.getOrientation().rotateVector(localAnchorA);
    math::Vector3D wB = B.position + B.getOrientation().rotateVector(localAnchorB);
    math::Vector3D error = wB - wA;
    float invDt = 1.0f / dt;
    math::Vector3D rA = wA - A.position;
    math::Vector3D rB = wB - B.position;
    const math::Vector3D axes[3] = { {1,0,0}, {0,1,0}, {0,0,1} };
    for (int i = 0; i < 3; i++) {
        math::Vector3D rAxN = rA.cross(axes[i]);
        math::Vector3D rBxN = rB.cross(axes[i]);
        float invMassSum = A.getInvMass() + B.getInvMass();
        float angA = A.applyInvInertiaWorld(rAxN).dot(rAxN);
        float angB = B.applyInvInertiaWorld(rBxN).dot(rBxN);
        float total = invMassSum + angA + angB;
        m_effectiveMass[i] = total > 1e-8f ? 1.0f / total : 0.0f;
        float errAxis = (i == 0) ? error.x : (i == 1) ? error.y : error.z;
        m_bias[i] = -0.3f * invDt * errAxis;
    }
    math::Vector3D P(m_impulse[0], m_impulse[1], m_impulse[2]);
    if (P.dot(P) > 1e-10f) {
        A.applyImpulseAtPoint(-P, wA);
        B.applyImpulseAtPoint(P, wB);
    }
}

void BallSocketConstraint::solve(std::vector<RigidBody3D>& bodies) {
    if (!enabled) return;
    RigidBody3D& A = bodies[bodyA];
    RigidBody3D& B = bodies[bodyB];
    math::Vector3D wA = A.position + A.getOrientation().rotateVector(localAnchorA);
    math::Vector3D wB = B.position + B.getOrientation().rotateVector(localAnchorB);
    math::Vector3D rA = wA - A.position;
    math::Vector3D rB = wB - B.position;
    math::Vector3D vA = A.velocity + A.angularVelocity.cross(rA);
    math::Vector3D vB = B.velocity + B.angularVelocity.cross(rB);
    math::Vector3D relV = vB - vA;
    const math::Vector3D axes[3] = { {1,0,0}, {0,1,0}, {0,0,1} };
    for (int i = 0; i < 3; i++) {
        float vn = relV.dot(axes[i]);
        float lambda = m_effectiveMass[i] * (-(vn + m_bias[i]));
        m_impulse[i] += lambda;
        math::Vector3D P = axes[i] * lambda;
        A.applyImpulseAtPoint(-P, wA);
        B.applyImpulseAtPoint(P, wB);
    }
}

// ── HingeConstraint ───────────────────────────────────────────────────
void HingeConstraint::preStep(std::vector<RigidBody3D>& bodies, float dt) {
    m_posConstraint.bodyA = bodyA;
    m_posConstraint.bodyB = bodyB;
    m_posConstraint.localAnchorA = localAnchorA;
    m_posConstraint.localAnchorB = localAnchorB;
    m_posConstraint.preStep(bodies, dt);

    RigidBody3D& A = bodies[bodyA];
    RigidBody3D& B = bodies[bodyB];
    math::Vector3D worldAxisA = A.getOrientation().rotateVector(localAxisA).normalized();
    math::Vector3D worldAxisB = B.getOrientation().rotateVector(localAxisB).normalized();

    math::Vector3D u, v;
    if (std::abs(worldAxisA.x) >= 0.57735f)
        u = math::Vector3D(worldAxisA.y, -worldAxisA.x, 0.0f).normalized();
    else
        u = math::Vector3D(0.0f, worldAxisA.z, -worldAxisA.y).normalized();
    v = worldAxisA.cross(u);

    float errU = worldAxisB.dot(u);
    float errV = worldAxisB.dot(v);
    float invDt = 1.0f / dt;
    for (int i = 0; i < 2; i++) {
        math::Vector3D axis = (i == 0) ? u : v;
        float err = (i == 0) ? errU : errV;
        math::Vector3D angA = A.applyInvInertiaWorld(axis);
        math::Vector3D angB = B.applyInvInertiaWorld(axis);
        float total = angA.dot(axis) + angB.dot(axis);
        m_angEffMass[i] = total > 1e-8f ? 1.0f / total : 0.0f;
        m_angBias[i] = -0.3f * invDt * err;
    }
    {
        math::Vector3D angA = A.applyInvInertiaWorld(worldAxisA);
        math::Vector3D angB = B.applyInvInertiaWorld(worldAxisA);
        float total = angA.dot(worldAxisA) + angB.dot(worldAxisA);
        m_motorEffMass = total > 1e-8f ? 1.0f / total : 0.0f;
    }
    if (hasLimits) {
        math::Vector3D refA = A.getOrientation().rotateVector(
            math::Vector3D(1,0,0).cross(localAxisA).sqrMagnitude() > 0.01f
                ? math::Vector3D(1,0,0).cross(localAxisA).normalized()
                : math::Vector3D(0,0,1).cross(localAxisA).normalized());
        math::Vector3D refB = B.getOrientation().rotateVector(
            math::Vector3D(1,0,0).cross(localAxisB).sqrMagnitude() > 0.01f
                ? math::Vector3D(1,0,0).cross(localAxisB).normalized()
                : math::Vector3D(0,0,1).cross(localAxisB).normalized());
        float sinAngle = refA.cross(refB).dot(worldAxisA);
        float cosAngle = refA.dot(refB);
        m_currentAngle = std::atan2(sinAngle, cosAngle);
        m_atLowerLimit = m_currentAngle <= lowerLimit;
        m_atUpperLimit = m_currentAngle >= upperLimit;
    }
    for (int i = 0; i < 2; i++) {
        if (std::abs(m_angImpulse[i]) > 1e-10f) {
            math::Vector3D axis = (i == 0) ? u : v;
            math::Vector3D angP = axis * m_angImpulse[i];
            A.angularVelocity -= A.applyInvInertiaWorld(angP);
            B.angularVelocity += B.applyInvInertiaWorld(angP);
        }
    }
    if (std::abs(m_motorImpulse) > 1e-10f) {
        math::Vector3D angP = worldAxisA * m_motorImpulse;
        A.angularVelocity -= A.applyInvInertiaWorld(angP);
        B.angularVelocity += B.applyInvInertiaWorld(angP);
    }
}

void HingeConstraint::solve(std::vector<RigidBody3D>& bodies) {
    if (!enabled) return;
    m_posConstraint.solve(bodies);
    RigidBody3D& A = bodies[bodyA];
    RigidBody3D& B = bodies[bodyB];
    math::Vector3D worldAxisA = A.getOrientation().rotateVector(localAxisA).normalized();

    math::Vector3D u, v;
    if (std::abs(worldAxisA.x) >= 0.57735f)
        u = math::Vector3D(worldAxisA.y, -worldAxisA.x, 0.0f).normalized();
    else
        u = math::Vector3D(0.0f, worldAxisA.z, -worldAxisA.y).normalized();
    v = worldAxisA.cross(u);

    math::Vector3D relW = B.angularVelocity - A.angularVelocity;
    for (int i = 0; i < 2; i++) {
        math::Vector3D axis = (i == 0) ? u : v;
        float wn = relW.dot(axis);
        float lambda = m_angEffMass[i] * (-(wn + m_angBias[i]));
        m_angImpulse[i] += lambda;
        math::Vector3D angP = axis * lambda;
        A.angularVelocity -= A.applyInvInertiaWorld(angP);
        B.angularVelocity += B.applyInvInertiaWorld(angP);
    }
    if (enableMotor) {
        relW = B.angularVelocity - A.angularVelocity;
        float wHinge = relW.dot(worldAxisA);
        float lambda = m_motorEffMass * (motorSpeed - wHinge);
        float maxImp = maxMotorTorque;
        float newImpulse = std::max(-maxImp, std::min(m_motorImpulse + lambda, maxImp));
        lambda = newImpulse - m_motorImpulse;
        m_motorImpulse = newImpulse;
        math::Vector3D angP = worldAxisA * lambda;
        A.angularVelocity -= A.applyInvInertiaWorld(angP);
        B.angularVelocity += B.applyInvInertiaWorld(angP);
    }
    if (hasLimits) {
        relW = B.angularVelocity - A.angularVelocity;
        float wHinge = relW.dot(worldAxisA);
        if (m_atLowerLimit) {
            float lambda = m_motorEffMass * (-wHinge);
            float newImpulse = std::max(m_limitImpulse + lambda, 0.0f);
            lambda = newImpulse - m_limitImpulse;
            m_limitImpulse = newImpulse;
            math::Vector3D angP = worldAxisA * lambda;
            A.angularVelocity -= A.applyInvInertiaWorld(angP);
            B.angularVelocity += B.applyInvInertiaWorld(angP);
        } else if (m_atUpperLimit) {
            float lambda = m_motorEffMass * (-wHinge);
            float newImpulse = std::min(m_limitImpulse + lambda, 0.0f);
            lambda = newImpulse - m_limitImpulse;
            m_limitImpulse = newImpulse;
            math::Vector3D angP = worldAxisA * lambda;
            A.angularVelocity -= A.applyInvInertiaWorld(angP);
            B.angularVelocity += B.applyInvInertiaWorld(angP);
        }
    }
}

// ── ConstraintSolver3D ────────────────────────────────────────────────
int ConstraintSolver3D::addDistance(int bodyA, int bodyB,
    const math::Vector3D& anchorA, const math::Vector3D& anchorB,
    float restLength, float stiffness)
{
    auto c = std::make_unique<DistanceConstraint>();
    c->bodyA = bodyA; c->bodyB = bodyB;
    c->localAnchorA = anchorA; c->localAnchorB = anchorB;
    c->restLength = restLength; c->stiffness = stiffness;
    m_constraints.push_back(std::move(c));
    return static_cast<int>(m_constraints.size()) - 1;
}

int ConstraintSolver3D::addBallSocket(int bodyA, int bodyB,
    const math::Vector3D& anchorA, const math::Vector3D& anchorB)
{
    auto c = std::make_unique<BallSocketConstraint>();
    c->bodyA = bodyA; c->bodyB = bodyB;
    c->localAnchorA = anchorA; c->localAnchorB = anchorB;
    m_constraints.push_back(std::move(c));
    return static_cast<int>(m_constraints.size()) - 1;
}

int ConstraintSolver3D::addHinge(int bodyA, int bodyB,
    const math::Vector3D& anchorA, const math::Vector3D& anchorB,
    const math::Vector3D& axisA, const math::Vector3D& axisB)
{
    auto c = std::make_unique<HingeConstraint>();
    c->bodyA = bodyA; c->bodyB = bodyB;
    c->localAnchorA = anchorA; c->localAnchorB = anchorB;
    c->localAxisA = axisA; c->localAxisB = axisB;
    m_constraints.push_back(std::move(c));
    return static_cast<int>(m_constraints.size()) - 1;
}

void ConstraintSolver3D::preStep(std::vector<RigidBody3D>& bodies, float dt) {
    for (auto& c : m_constraints)
        if (c->enabled) c->preStep(bodies, dt);
}

void ConstraintSolver3D::solve(std::vector<RigidBody3D>& bodies) {
    for (int iter = 0; iter < iterations; iter++)
        for (auto& c : m_constraints)
            if (c->enabled) c->solve(bodies);
}

} // namespace physics
} // namespace engine
