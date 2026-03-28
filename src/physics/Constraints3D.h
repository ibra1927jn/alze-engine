#pragma once

#include "RigidBody3D.h"
#include "math/Vector3D.h"
#include <vector>
#include <memory>
#include <cmath>

namespace engine {
namespace physics {

// ════════════════════════════════════════════════════════════════
// Constraint System — Sequential Impulse joints
//
// All constraints share the same solve loop.
// Each constraint type defines its own preStep() and solve().
// ════════════════════════════════════════════════════════════════

/// Base constraint interface
struct Constraint3D {
    enum class Type { DISTANCE, BALL_SOCKET, HINGE };

    Type type;
    int bodyA = -1;
    int bodyB = -1;
    bool enabled = true;

    virtual ~Constraint3D() = default;

    /// Pre-compute data for the constraint
    virtual void preStep(std::vector<RigidBody3D>& bodies, float dt) = 0;

    /// Apply corrective impulses to satisfy the constraint
    virtual void solve(std::vector<RigidBody3D>& bodies) = 0;
};

// ════════════════════════════════════════════════════════════════
// Distance Constraint — keeps two bodies at a fixed distance
//
// Use case: rope, chain links, suspension springs
//
// Maintains |pB - pA| = restLength  (or within softness tolerance)
// ════════════════════════════════════════════════════════════════

struct DistanceConstraint : public Constraint3D {
    math::Vector3D localAnchorA;  // Anchor in body A local space
    math::Vector3D localAnchorB;  // Anchor in body B local space
    float restLength = 1.0f;
    float stiffness = 1.0f;       // [0..1] — 1 = rigid, <1 = soft spring
    float damping = 0.0f;         // Velocity damping

    // Solver state
    math::Vector3D m_n;           // Constraint axis (normalized)
    float m_effectiveMass = 0.0f;
    float m_bias = 0.0f;
    float m_impulse = 0.0f;       // Accumulated impulse

    DistanceConstraint() { type = Type::DISTANCE; }

    void preStep(std::vector<RigidBody3D>& bodies, float dt) override;
    void solve(std::vector<RigidBody3D>& bodies) override;
};

// ════════════════════════════════════════════════════════════════
// Ball-Socket Joint — constrains positions to share a point
//
// Use case: ragdoll joints, pendulums, ball joints
//
// Maintains worldA(anchorA) == worldB(anchorB)
// ════════════════════════════════════════════════════════════════

struct BallSocketConstraint : public Constraint3D {
    math::Vector3D localAnchorA;
    math::Vector3D localAnchorB;

    // Solver state (3 DOF constraint → solve 3 axes)
    float m_effectiveMass[3] = {};
    float m_bias[3] = {};
    float m_impulse[3] = {};

    BallSocketConstraint() { type = Type::BALL_SOCKET; }

    void preStep(std::vector<RigidBody3D>& bodies, float dt) override;
    void solve(std::vector<RigidBody3D>& bodies) override;
};

// ════════════════════════════════════════════════════════════════
// Hinge Constraint — ball-socket + angular constraint around 1 axis
//
// Use case: doors, wheels, axles, elbows
//
// Constrains: position (3 DOF) + allows rotation only around hinge axis
// ════════════════════════════════════════════════════════════════

struct HingeConstraint : public Constraint3D {
    math::Vector3D localAnchorA;
    math::Vector3D localAnchorB;
    math::Vector3D localAxisA = math::Vector3D(0, 1, 0);
    math::Vector3D localAxisB = math::Vector3D(0, 1, 0);

    // Angle limits (radians)
    bool hasLimits = false;
    float lowerLimit = -3.14159f;
    float upperLimit = 3.14159f;

    // Motor
    bool enableMotor = false;
    float motorSpeed = 0.0f;          // Target angular velocity (rad/s)
    float maxMotorTorque = 100.0f;    // Max impulse the motor can apply

    // Solver state
    BallSocketConstraint m_posConstraint;  // Reuse position constraint

    float m_angEffMass[2] = {};
    float m_angBias[2] = {};
    float m_angImpulse[2] = {};

    // Motor + limits solver state
    float m_motorEffMass = 0.0f;
    float m_motorImpulse = 0.0f;
    float m_limitImpulse = 0.0f;
    float m_currentAngle = 0.0f;
    bool m_atLowerLimit = false;
    bool m_atUpperLimit = false;

    HingeConstraint() { type = Type::HINGE; }

    void preStep(std::vector<RigidBody3D>& bodies, float dt) override;
    void solve(std::vector<RigidBody3D>& bodies) override;
};

// ════════════════════════════════════════════════════════════════
// Constraint Manager — owns and solves all constraints
// ════════════════════════════════════════════════════════════════

class ConstraintSolver3D {
public:
    int iterations = 8;

    int addDistance(int bodyA, int bodyB,
                   const math::Vector3D& anchorA, const math::Vector3D& anchorB,
                   float restLength, float stiffness = 1.0f);
    int addBallSocket(int bodyA, int bodyB,
                      const math::Vector3D& anchorA, const math::Vector3D& anchorB);
    int addHinge(int bodyA, int bodyB,
                 const math::Vector3D& anchorA, const math::Vector3D& anchorB,
                 const math::Vector3D& axisA = math::Vector3D(0,1,0),
                 const math::Vector3D& axisB = math::Vector3D(0,1,0));
    void preStep(std::vector<RigidBody3D>& bodies, float dt);
    void solve(std::vector<RigidBody3D>& bodies);
    int getCount() const { return static_cast<int>(m_constraints.size()); }
    Constraint3D* getConstraint(int index) { return m_constraints[index].get(); }

private:
    std::vector<std::unique_ptr<Constraint3D>> m_constraints;
};

} // namespace physics
} // namespace engine
