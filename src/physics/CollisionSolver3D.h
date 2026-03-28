#pragma once

#include "RigidBody3D.h"
#include "Collider3D.h"
#include "math/Vector3D.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace engine {
namespace physics {

/// Contact3D — Full contact manifold point for the solver
struct Contact3D {
    int bodyA = -1;
    int bodyB = -1;
    math::Vector3D normal;        // From A to B
    math::Vector3D contactPoint;
    math::Vector3D localPointA;   // Local to bodyA
    math::Vector3D localPointB;   // Local to bodyB
    float penetration = 0.0f;

    // Solver state (warm startable)
    float normalImpulse   = 0.0f;
    float tangentImpulse1 = 0.0f;
    float tangentImpulse2 = 0.0f;

    // Precomputed per-step
    math::Vector3D rA, rB;        // Contact arms (cached)
    math::Vector3D tangent1, tangent2;
    float normalMass = 0.0f;
    float tangentMass1 = 0.0f;
    float tangentMass2 = 0.0f;
    float restitution = 0.0f;
    float friction = 0.0f;
    float bias = 0.0f;

    // Hash for warm-start matching (contact point quantized)
    uint32_t contactHash = 0;
};

/// computeContactHash — spatially quantize contact point for fast matching
inline uint32_t computeContactHash(int a, int b, const math::Vector3D& p) {
    // Quantize position to 1cm grid
    int32_t qx = static_cast<int32_t>(std::floor(p.x * 100.0f));
    int32_t qy = static_cast<int32_t>(std::floor(p.y * 100.0f));
    int32_t qz = static_cast<int32_t>(std::floor(p.z * 100.0f));
    uint32_t h = static_cast<uint32_t>(a) * 73856093u;
    h ^= static_cast<uint32_t>(b) * 19349663u;
    h ^= static_cast<uint32_t>(qx) * 83492791u;
    h ^= static_cast<uint32_t>(qy) * 53471161u;
    h ^= static_cast<uint32_t>(qz) * 27644437u;
    return h;
}

/// CollisionSolver3D — Sequential Impulse solver with Coulomb friction.
///
/// Features:
///   - Normal impulse (separation + restitution)
///   - Combined Coulomb cone friction (|t1² + t2²| ≤ μ²N²)
///   - Baumgarte stabilization
///   - Contact-hash warm starting
///
class CollisionSolver3D {
public:
    int iterations = 10;              // Solver iterations per step
    float baumgarte = 0.2f;           // Penetration correction factor [0..1]
    float slop = 0.005f;              // Allowed penetration before correction (m)
    float restitutionThreshold = 1.0f; // Min closing velocity for restitution

    /// Pre-compute contact data before solving
    void preStep(std::vector<Contact3D>& contacts, std::vector<RigidBody3D>& bodies, float dt);

    /// Solve contacts iteratively
    void solve(std::vector<Contact3D>& contacts, std::vector<RigidBody3D>& bodies);

private:
    float computeEffectiveMass(const RigidBody3D& A, const RigidBody3D& B,
        const math::Vector3D& rA, const math::Vector3D& rB, const math::Vector3D& dir);

    float computeRelativeVelocity(const RigidBody3D& A, const RigidBody3D& B,
        const math::Vector3D& rA, const math::Vector3D& rB, const math::Vector3D& dir);
};

} // namespace physics
} // namespace engine
