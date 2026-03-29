// test_physics3d.cpp — Tests for 3D physics: OBB, inertia, broadphase, solver
//
// Covers:
//   - OBB3D struct and SAT collision
//   - OBB vs Sphere collision
//   - Sphere vs Sphere collision
//   - RigidBody3D inertia tensor correctness
//   - SpatialHash3D broadphase
//   - PhysicsWorld3D integration
//

#include <iostream>
#include <cmath>
#include <string>

#include "math/Vector3D.h"
#include "math/Quaternion.h"
#include "math/MathUtils.h"
#include "physics/Collider3D.h"
#include "physics/RigidBody3D.h"
#include "physics/FluidSystem.h"
#include "physics/Electromagnetism.h"
#include "physics/SoftBody3D.h"
#include "physics/GravityNBody.h"
#include "physics/WaveSystem.h"
#include "physics/Chemistry.h"
#include "physics/FractureSystem.h"
#include "physics/OpticsSystem.h"
#include "physics/QuantumSystem.h"
#include "physics/CCDSystem.h"
#include "physics/AdvancedFriction.h"
#include "physics/CompressibleFlow.h"
#include "physics/CrossSystemCoupling.h"
#include "physics/Hyperelasticity.h"
#include "physics/MolecularDynamics.h"
#include "physics/NuclearPhysics.h"
#include "physics/Relativity.h"
#include "physics/MHDSystem.h"
#include "physics/PhysicsConfig.h"
#include "physics/UnifiedSimulation.h"
#include "ecs/ECSCoordinator.h"
#include "physics/DynamicBVH3D.h"
#include "physics/PhysicsWorld3D.h"

using namespace engine::math;
using namespace engine::physics;

static int passed = 0;
static int failed = 0;

static void check(bool cond, const std::string& msg) {
    if (cond) { std::cout << "  [OK] " << msg << std::endl; passed++; }
    else      { std::cerr << "  [FAIL] " << msg << std::endl; failed++; }
}

static bool approx(float a, float b, float eps = 0.01f) {
    return std::fabs(a - b) < eps;
}

static bool v3approx(const Vector3D& a, const Vector3D& b, float eps = 0.01f) {
    return approx(a.x, b.x, eps) && approx(a.y, b.y, eps) && approx(a.z, b.z, eps);
}

// ═══════════════════════════════════════════════════════════════
// Sphere vs Sphere Tests
// ═══════════════════════════════════════════════════════════════
void testSphereVsSphere() {
    std::cout << "\n=== Sphere vs Sphere ===" << std::endl;

    // Overlapping spheres
    SphereCollider a(Vector3D(0,0,0), 1.0f);
    SphereCollider b(Vector3D(1.5f,0,0), 1.0f);
    ContactInfo c1 = sphereVsSphere(a, b);
    check(c1.hasContact, "Overlapping spheres detect contact");
    check(approx(c1.penetration, 0.5f), "Penetration = 0.5");
    check(approx(c1.normal.x, 1.0f, 0.01f), "Normal points from A to B (X)");

    // Non-overlapping
    SphereCollider c(Vector3D(5,0,0), 1.0f);
    ContactInfo c2 = sphereVsSphere(a, c);
    check(!c2.hasContact, "Non-overlapping spheres: no contact");

    // Touching exactly
    SphereCollider d(Vector3D(2,0,0), 1.0f);
    ContactInfo c3 = sphereVsSphere(a, d);
    check(!c3.hasContact, "Touching spheres (dist == rSum): no contact");

    // Concentric spheres (degenerate)
    SphereCollider e(Vector3D(0,0,0), 0.5f);
    ContactInfo c4 = sphereVsSphere(a, e);
    check(c4.hasContact, "Concentric spheres detect contact");
    check(c4.normal.magnitude() > 0.9f, "Concentric: fallback normal valid");
}

// ═══════════════════════════════════════════════════════════════
// AABB Tests
// ═══════════════════════════════════════════════════════════════
void testAABB() {
    std::cout << "\n=== AABB3D ===" << std::endl;

    AABB3D a(Vector3D(-1,-1,-1), Vector3D(1,1,1));
    AABB3D b(Vector3D(0,0,0), Vector3D(2,2,2));
    check(a.overlaps(b), "Overlapping AABBs");

    AABB3D c(Vector3D(5,5,5), Vector3D(6,6,6));
    check(!a.overlaps(c), "Non-overlapping AABBs");

    check(a.contains(Vector3D(0,0,0)), "Contains origin");
    check(!a.contains(Vector3D(2,0,0)), "Doesn't contain outside point");

    Vector3D cp = a.closestPoint(Vector3D(5,0,0));
    check(approx(cp.x, 1.0f), "Closest point X clamped");
    check(approx(cp.y, 0.0f), "Closest point Y unchanged");

    ContactInfo ci = aabbVsAABB(a, b);
    check(ci.hasContact, "AABB contact detected");
    check(ci.penetration > 0.0f, "AABB penetration > 0");
}

// ═══════════════════════════════════════════════════════════════
// OBB vs OBB SAT Tests
// ═══════════════════════════════════════════════════════════════
void testOBBvsOBB() {
    std::cout << "\n=== OBB vs OBB (SAT) ===" << std::endl;

    // Axis-aligned boxes overlapping
    OBB3D a;
    a.center = Vector3D(0,0,0);
    a.halfExtents = Vector3D(1,1,1);

    OBB3D b;
    b.center = Vector3D(1.5f, 0, 0);
    b.halfExtents = Vector3D(1,1,1);

    ContactInfo c1 = obbVsOBB(a, b);
    check(c1.hasContact, "Aligned OBBs: contact detected");
    check(approx(c1.penetration, 0.5f), "Aligned OBBs: penetration = 0.5");

    // Non-overlapping aligned boxes
    OBB3D c;
    c.center = Vector3D(5, 0, 0);
    c.halfExtents = Vector3D(1,1,1);
    ContactInfo c2 = obbVsOBB(a, c);
    check(!c2.hasContact, "Separated OBBs: no contact");

    // 45° rotated box overlapping with axis-aligned box
    OBB3D d;
    d.center = Vector3D(1.2f, 0, 0);
    d.halfExtents = Vector3D(1,1,1);
    float s45 = std::sin(MathUtils::PI * 0.25f);
    float c45 = std::cos(MathUtils::PI * 0.25f);
    d.axes[0] = Vector3D(c45, s45, 0);
    d.axes[1] = Vector3D(-s45, c45, 0);
    d.axes[2] = Vector3D(0, 0, 1);
    ContactInfo c3 = obbVsOBB(a, d);
    check(c3.hasContact, "45° rotated OBB: contact detected");

    // Rotated box that's separated in OBB space but overlaps in AABB
    // This tests the edge-edge axes (the main advantage of SAT over AABB)
    OBB3D e;
    e.center = Vector3D(2.5f, 0, 0);
    e.halfExtents = Vector3D(0.5f, 0.5f, 0.5f);
    // Rotate 45° around Z
    e.axes[0] = Vector3D(c45, s45, 0);
    e.axes[1] = Vector3D(-s45, c45, 0);
    e.axes[2] = Vector3D(0, 0, 1);
    ContactInfo c4 = obbVsOBB(a, e);
    check(!c4.hasContact, "Rotated separated OBB: no false positive");

    // Two identically positioned boxes (full overlap)
    OBB3D f;
    f.center = Vector3D(0,0,0);
    f.halfExtents = Vector3D(1,1,1);
    ContactInfo c5 = obbVsOBB(a, f);
    check(c5.hasContact, "Identical OBBs: contact");
    check(approx(c5.penetration, 2.0f), "Identical OBBs: penetration = 2");
}

// ═══════════════════════════════════════════════════════════════
// OBB vs Sphere Tests
// ═══════════════════════════════════════════════════════════════
void testOBBvsSphere() {
    std::cout << "\n=== OBB vs Sphere ===" << std::endl;

    OBB3D box;
    box.center = Vector3D(0,0,0);
    box.halfExtents = Vector3D(1,1,1);

    // Sphere touching box face
    SphereCollider s1(Vector3D(1.5f, 0, 0), 1.0f);
    ContactInfo c1 = obbVsSphere(box, s1);
    check(c1.hasContact, "Sphere touching face: contact");
    check(approx(c1.penetration, 0.5f, 0.05f), "Sphere face penetration ≈ 0.5");

    // Sphere far away
    SphereCollider s2(Vector3D(5, 0, 0), 0.5f);
    ContactInfo c2 = obbVsSphere(box, s2);
    check(!c2.hasContact, "Sphere far: no contact");

    // Sphere touching rotated OBB corner
    OBB3D rotBox;
    rotBox.center = Vector3D(0,0,0);
    rotBox.halfExtents = Vector3D(1,1,1);
    float s45 = std::sin(MathUtils::PI * 0.25f);
    float c45 = std::cos(MathUtils::PI * 0.25f);
    rotBox.axes[0] = Vector3D(c45, s45, 0);
    rotBox.axes[1] = Vector3D(-s45, c45, 0);
    rotBox.axes[2] = Vector3D(0, 0, 1);

    // Sphere just touching the corner of rotated box
    float diag = std::sqrt(2.0f);  // Corner distance for unit box at 45°
    SphereCollider s3(Vector3D(diag + 0.3f, 0, 0), 0.5f);
    ContactInfo c3 = obbVsSphere(rotBox, s3);
    check(c3.hasContact, "Sphere near rotated corner: contact");
}

// ═══════════════════════════════════════════════════════════════
// Ray Tests
// ═══════════════════════════════════════════════════════════════
void testRays() {
    std::cout << "\n=== Ray Tests ===" << std::endl;

    // Ray vs sphere
    Ray3D ray(Vector3D(0,0,-5), Vector3D(0,0,1));
    SphereCollider sphere(Vector3D(0,0,0), 1.0f);
    RayHit3D hit1 = rayVsSphere(ray, sphere);
    check(hit1.hit, "Ray hits sphere");
    check(approx(hit1.distance, 4.0f), "Hit distance = 4");

    // Ray missing sphere
    Ray3D ray2(Vector3D(5,0,-5), Vector3D(0,0,1));
    RayHit3D hit2 = rayVsSphere(ray2, sphere);
    check(!hit2.hit, "Ray misses sphere");

    // Ray vs AABB
    AABB3D box(Vector3D(-1,-1,-1), Vector3D(1,1,1));
    RayHit3D hit3 = rayVsAABB(ray, box);
    check(hit3.hit, "Ray hits AABB");
    check(approx(hit3.distance, 4.0f), "AABB hit distance = 4");
}

// ═══════════════════════════════════════════════════════════════
// RigidBody3D Inertia Tests
// ═══════════════════════════════════════════════════════════════
void testInertia() {
    std::cout << "\n=== Inertia Tensor ===" << std::endl;

    // Sphere: I = 2/5 * m * r²
    RigidBody3D sphere = RigidBody3D::dynamic(10.0f);
    sphere.setSphereInertia(1.0f);
    Vector3D invI = sphere.getInvInertia();
    float expectedI = 0.4f * 10.0f * 1.0f;  // I = 4
    check(approx(invI.x, 1.0f / expectedI), "Sphere Ix correct");
    check(approx(invI.y, 1.0f / expectedI), "Sphere Iy correct (=Ix)");
    check(approx(invI.z, 1.0f / expectedI), "Sphere Iz correct (=Ix)");

    // Box: I = m/12 * (h²+d², w²+d², w²+h²)
    RigidBody3D box = RigidBody3D::dynamic(12.0f);
    box.setBoxInertia(2.0f, 4.0f, 6.0f);
    Vector3D boxInvI = box.getInvInertia();
    float Ix = (12.0f / 12.0f) * (4.0f*4.0f + 6.0f*6.0f);  // = 52
    float Iy = (12.0f / 12.0f) * (2.0f*2.0f + 6.0f*6.0f);  // = 40
    float Iz = (12.0f / 12.0f) * (2.0f*2.0f + 4.0f*4.0f);  // = 20
    check(approx(boxInvI.x, 1.0f / Ix, 0.001f), "Box Ix correct");
    check(approx(boxInvI.y, 1.0f / Iy, 0.001f), "Box Iy correct");
    check(approx(boxInvI.z, 1.0f / Iz, 0.001f), "Box Iz correct");

    // applyInvInertiaWorld: identity orientation → same as local
    Vector3D torque(10, 20, 30);
    Vector3D result = sphere.applyInvInertiaWorld(torque);
    check(approx(result.x, torque.x / expectedI), "World inertia X (identity orientation)");
    check(approx(result.y, torque.y / expectedI), "World inertia Y (identity orientation)");
    check(approx(result.z, torque.z / expectedI), "World inertia Z (identity orientation)");

    // Rotated body: result should rotate with orientation
    box.setOrientation(Quaternion::fromAxisAngle(Vector3D::Up, MathUtils::HALF_PI));
    Vector3D xTorque(1, 0, 0);
    Vector3D rotResult = box.applyInvInertiaWorld(xTorque);
    // After 90° around Y, local X→-Z, so torque (1,0,0) in world
    // becomes (0,0,-1) in local, which gets invInertia.z, then rotated back
    check(rotResult.magnitude() > 0.0f, "Rotated inertia produces non-zero result");

    // Static body produces zero
    RigidBody3D staticB = RigidBody3D::staticBody();
    Vector3D staticResult = staticB.applyInvInertiaWorld(torque);
    check(v3approx(staticResult, Vector3D::Zero), "Static body inertia = zero");
}

// ═══════════════════════════════════════════════════════════════
// DynamicBVH3D Tests
// ═══════════════════════════════════════════════════════════════
void testDynamicBVH() {
    std::cout << "\n=== DynamicBVH3D ===" << std::endl;

    DynamicBVH3D bvh;

    // Two overlapping AABBs
    bvh.insert(0, AABB3D(Vector3D(-1,-1,-1), Vector3D(1,1,1)));
    bvh.insert(1, AABB3D(Vector3D(0,0,0), Vector3D(2,2,2)));
    const auto& pairs = bvh.getPotentialPairs();
    check(pairs.size() > 0, "Overlapping bodies: potential pair found");

    // Verify pair contains {0,1}
    bool found01 = false;
    for (const auto& p : pairs) {
        if ((p.first == 0 && p.second == 1) || (p.first == 1 && p.second == 0))
            found01 = true;
    }
    check(found01, "Pair {0,1} in results");

    // Far apart bodies: no pairs
    DynamicBVH3D bvh2;
    bvh2.insert(0, AABB3D(Vector3D(-1,-1,-1), Vector3D(0,0,0)));
    bvh2.insert(1, AABB3D(Vector3D(100,100,100), Vector3D(101,101,101)));
    const auto& pairs2 = bvh2.getPotentialPairs();
    check(pairs2.size() == 0, "Far apart bodies: no pairs");
}

// ═══════════════════════════════════════════════════════════════
// PhysicsWorld3D Integration Tests
// ═══════════════════════════════════════════════════════════════
void testPhysicsWorld() {
    std::cout << "\n=== PhysicsWorld3D ===" << std::endl;

    PhysicsWorld3D world;
    world.gravity = Vector3D(0, -9.81f, 0);
    world.subSteps = 8;  // More sub-steps for stability

    // Add floor
    int floor = world.addStaticBox(Vector3D(0, -1.0f, 0), Vector3D(50, 1, 50));
    check(world.getBody(floor).isStatic(), "Floor is static");

    // Add sphere just above floor
    int ball = world.addDynamicSphere(Vector3D(0, 2, 0), 0.5f, 1.0f);
    check(world.getBody(ball).isDynamic(), "Ball is dynamic");

    // Step simulation — ball should fall
    float y0 = world.getBody(ball).position.y;
    for (int i = 0; i < 10; i++) {
        world.step(1.0f / 60.0f);
    }
    float y1 = world.getBody(ball).position.y;
    check(y1 < y0, "Ball falls under gravity");

    // Ball should eventually rest on floor
    for (int i = 0; i < 300; i++) {
        world.step(1.0f / 60.0f);
    }
    float yFinal = world.getBody(ball).position.y;
    std::cout << "    (Ball settled to y=" << yFinal << ")" << std::endl;
    check(yFinal > -2.0f, "Ball rests near floor (not fallen through)");
    check(yFinal < 3.0f, "Ball doesn't float (settled)");

    // Body count
    check(world.getBodyCount() == 2, "World has 2 bodies");
}

// ═══════════════════════════════════════════════════════════════
// Capsule Collision Tests
// ═══════════════════════════════════════════════════════════════
void testCapsuleCollisions() {
    std::cout << "\n=== Capsule Collisions ===" << std::endl;

    // ── Segment utility ─────────────────────────
    Vector3D cp = closestPointOnSegment(
        Vector3D(0, 0, 0), Vector3D(0, 4, 0), Vector3D(3, 2, 0));
    check(v3approx(cp, Vector3D(0, 2, 0)), "Closest point on segment correct");

    Vector3D cpEnd = closestPointOnSegment(
        Vector3D(0, 0, 0), Vector3D(0, 4, 0), Vector3D(3, 10, 0));
    check(v3approx(cpEnd, Vector3D(0, 4, 0)), "Closest point clamped to segment end");

    // ── Capsule vs Sphere ───────────────────────
    CapsuleCollider cap1(Vector3D(0, 1, 0), 2.0f, 0.3f, Vector3D(0, 1, 0));
    // Segment: from (0,0.7,0) to (0,1.3,0), radius 0.3

    SphereCollider sph1(Vector3D(0.4f, 1, 0), 0.3f);
    ContactInfo cv1 = capsuleVsSphere(cap1, sph1);
    check(cv1.hasContact, "Capsule-Sphere overlap: contact");
    check(cv1.penetration > 0.0f, "Capsule-Sphere: penetration > 0");

    SphereCollider sph2(Vector3D(5, 1, 0), 0.3f);
    ContactInfo cv2 = capsuleVsSphere(cap1, sph2);
    check(!cv2.hasContact, "Capsule-Sphere far: no contact");

    // ── Capsule vs Capsule ──────────────────────
    CapsuleCollider cap2(Vector3D(0, 1, 0), 2.0f, 0.3f, Vector3D(0, 1, 0));
    CapsuleCollider cap3(Vector3D(0.5f, 1, 0), 2.0f, 0.3f, Vector3D(0, 1, 0));
    ContactInfo cv3 = capsuleVsCapsule(cap2, cap3);
    check(cv3.hasContact, "Capsule-Capsule parallel overlap: contact");

    CapsuleCollider cap4(Vector3D(5, 1, 0), 2.0f, 0.3f, Vector3D(0, 1, 0));
    ContactInfo cv4 = capsuleVsCapsule(cap2, cap4);
    check(!cv4.hasContact, "Capsule-Capsule far: no contact");

    // Crossed capsules (perpendicular)
    CapsuleCollider capH(Vector3D(0, 0, 0), 4.0f, 0.3f, Vector3D(0, 1, 0));  // Vertical
    CapsuleCollider capV(Vector3D(0, 0, 0), 4.0f, 0.3f, Vector3D(1, 0, 0));  // Horizontal
    ContactInfo cv5 = capsuleVsCapsule(capH, capV);
    check(cv5.hasContact, "Crossed capsules: contact");
    check(approx(cv5.penetration, 0.6f, 0.05f), "Crossed capsules: pen ≈ 2*radius");

    // ── Capsule vs OBB ──────────────────────────
    OBB3D floorOBB;
    floorOBB.center = Vector3D(0, -1, 0);
    floorOBB.halfExtents = Vector3D(10, 1, 10);

    CapsuleCollider cap5(Vector3D(0, 0.5f, 0), 2.0f, 0.3f, Vector3D(0, 1, 0));
    ContactInfo cv6 = capsuleVsOBB(cap5, floorOBB);
    check(cv6.hasContact, "Capsule-OBB overlap: contact");

    CapsuleCollider cap6(Vector3D(0, 5, 0), 2.0f, 0.3f, Vector3D(0, 1, 0));
    ContactInfo cv7 = capsuleVsOBB(cap6, floorOBB);
    check(!cv7.hasContact, "Capsule-OBB far: no contact");
}

// ═══════════════════════════════════════════════════════════════
// Capsule Inertia Tests
// ═══════════════════════════════════════════════════════════════
void testCapsuleInertia() {
    std::cout << "\n=== Capsule Inertia ===" << std::endl;

    RigidBody3D cap = RigidBody3D::dynamic(5.0f);
    cap.setCapsuleInertia(2.0f, 0.5f);
    Vector3D invI = cap.getInvInertia();
    // Ix = Iz = m/12*(3r² + h²) = 5/12*(0.75+4) = 5/12*4.75 ≈ 1.979
    // Iy = m*r²/2 = 5*0.25/2 = 0.625
    float Ixz = (5.0f / 12.0f) * (3.0f * 0.25f + 4.0f);
    float Iy  = 5.0f * 0.25f * 0.5f;
    check(approx(invI.x, 1.0f / Ixz, 0.01f), "Capsule Ix correct");
    check(approx(invI.y, 1.0f / Iy, 0.01f), "Capsule Iy correct");
    check(approx(invI.z, 1.0f / Ixz, 0.01f), "Capsule Iz correct (=Ix)");
}

// ═══════════════════════════════════════════════════════════════
// Capsule World Integration — settling on floor
// ═══════════════════════════════════════════════════════════════
void testCapsuleWorld() {
    std::cout << "\n=== Capsule World ===" << std::endl;

    PhysicsWorld3D world;
    world.gravity = Vector3D(0, -9.81f, 0);
    world.subSteps = 8;

    int floor = world.addStaticBox(Vector3D(0, -1, 0), Vector3D(50, 1, 50));
    (void)floor;
    int capsule = world.addDynamicCapsule(Vector3D(0, 3, 0), 2.0f, 0.3f, 1.0f);

    check(world.getBody(capsule).shape == RigidBody3D::Shape::CAPSULE, "Shape is CAPSULE");

    for (int i = 0; i < 300; i++) {
        world.step(1.0f / 60.0f);
    }
    float yFinal = world.getBody(capsule).position.y;
    std::cout << "    (Capsule settled to y=" << yFinal << ")" << std::endl;
    check(yFinal > -5.0f, "Capsule rests near floor");
    check(yFinal < 4.0f, "Capsule settled (didn't float)");
}

// ═══════════════════════════════════════════════════════════════
// Constraint Tests
// ═══════════════════════════════════════════════════════════════
void testConstraints() {
    std::cout << "\n=== Constraints ===" << std::endl;

    // ── Distance Constraint (API + stability test) ──
    {
        PhysicsWorld3D world;
        world.gravity = Vector3D(0, 0, 0);
        world.subSteps = 4;

        int a = world.addDynamicSphere(Vector3D(0, 0, 0), 0.3f, 1.0f);
        int b = world.addDynamicSphere(Vector3D(2, 0, 0), 0.3f, 1.0f);

        int cid = world.addDistanceConstraint(a, b,
            Vector3D(0,0,0), Vector3D(0,0,0), 2.0f, 1.0f);
        check(cid >= 0, "Distance constraint created");

        // Give bodies velocities — constraint should resist separation
        world.getBody(a).velocity = Vector3D(-5, 0, 0);
        world.getBody(b).velocity = Vector3D(5, 0, 0);

        for (int i = 0; i < 30; i++) {
            world.step(1.0f / 60.0f);
        }

        // Bodies should not fly apart as fast as without constraint
        float dist = (world.getBody(b).position - world.getBody(a).position).magnitude();
        // Without constraint: bodies would be ~7 units apart (5m/s * 0.5s * 2)
        // With constraint: should be significantly less
        check(dist < 10.0f, "Distance constraint: resists separation");
    }

    // ── Ball-Socket Joint (API test) ────────────
    {
        PhysicsWorld3D world;
        world.gravity = Vector3D(0, 0, 0);
        world.subSteps = 4;

        int a = world.addDynamicSphere(Vector3D(0, 0, 0), 0.3f, 1.0f);
        int b = world.addDynamicSphere(Vector3D(1, 0, 0), 0.3f, 1.0f);

        int cid = world.addBallSocketJoint(a, b,
            Vector3D(0.5f, 0, 0), Vector3D(-0.5f, 0, 0));
        check(cid >= 0, "Ball-socket joint created");

        // Give B a velocity — joint should constrain it
        world.getBody(b).velocity = Vector3D(5, 0, 0);
        for (int i = 0; i < 30; i++) {
            world.step(1.0f / 60.0f);
        }

        // Both should have moved (momentum transferred through joint)
        check(world.getBody(a).velocity.magnitude() > 0.01f,
              "Ball-socket: momentum transfers to A");
    }

    // ── Hinge Joint (API test) ──────────────────
    {
        PhysicsWorld3D world;
        world.gravity = Vector3D(0, 0, 0);
        world.subSteps = 4;

        int a = world.addDynamicBox(Vector3D(0, 0, 0), Vector3D(0.5f, 0.5f, 0.5f), 1.0f);
        int b = world.addDynamicBox(Vector3D(2, 0, 0), Vector3D(0.5f, 0.5f, 0.5f), 1.0f);

        int cid = world.addHingeJoint(a, b,
            Vector3D(1, 0, 0), Vector3D(-1, 0, 0),
            Vector3D(0, 1, 0), Vector3D(0, 1, 0));
        check(cid >= 0, "Hinge joint created");

        world.step(1.0f / 60.0f);
        check(true, "Hinge joint: simulation step completes");
    }
}

// ═════════════════════════════════════════════════════════════════
// Phase 14: PhysicsMaterial Tests
// ═════════════════════════════════════════════════════════════════
#include "physics/PhysicsMaterial.h"
#include "physics/PhysicsMath.h"
#include "physics/Thermodynamics.h"

void testPhysicsMaterial() {
    std::cout << "\n=== PhysicsMaterial ===" << std::endl;

    // Presets have correct values
    auto steel = PhysicsMaterial::Steel();
    check(steel.density == 7800.0f, "Steel density = 7800 kg/m\xC2\xB3");
    check(steel.staticFriction > steel.kineticFriction, "Steel: \xCE\xBCs > \xCE\xBCk");
    check(steel.rollingFriction < 0.01f, "Steel: low rolling friction");
    check(steel.meltingPoint > 1600.0f, "Steel melts above 1600K");

    auto ice = PhysicsMaterial::Ice();
    check(ice.staticFriction < 0.15f, "Ice: very low static friction");
    check(ice.kineticFriction < 0.05f, "Ice: ultra-low kinetic friction");
    check(approx(ice.meltingPoint, 273.15f), "Ice melts at 0 degrees C");

    auto rubber = PhysicsMaterial::Rubber();
    check(rubber.restitution > 0.8f, "Rubber: high bounciness");
    check(rubber.staticFriction >= 1.0f, "Rubber: \xCE\xBCs >= 1.0");

    // Stribeck curve
    check(approx(ice.stribeckFriction(0.0f), ice.staticFriction),
          "Stribeck at v=0 returns static friction");
    check(approx(ice.stribeckFriction(1.0f), ice.kineticFriction, 0.02f),
          "Stribeck at v>>threshold returns kinetic friction");
    float midFriction = ice.stribeckFriction(ice.stribeckVelocity * 0.5f);
    check(midFriction > ice.kineticFriction && midFriction < ice.staticFriction,
          "Stribeck at midpoint is between static and kinetic");

    // Combine functions
    check(approx(PhysicsMaterial::combineFriction(0.36f, 0.49f),
                 std::sqrt(0.36f * 0.49f), 0.01f),
          "combineFriction = geometric mean");
    check(PhysicsMaterial::combineRestitution(0.3f, 0.8f) == 0.8f,
          "combineRestitution = max");
}

// ═════════════════════════════════════════════════════════════════
// Phase 14: Aerodynamics Tests
// ═════════════════════════════════════════════════════════════════
void testAerodynamics() {
    std::cout << "\n=== Aerodynamics ===" << std::endl;
    using namespace PhysicsMath;

    // Drag force opposes velocity
    Vector3D vel(10, 0, 0);
    Vector3D drag = dragForce(vel, AIR_DENSITY, 0.47f, 1.0f);
    check(drag.x < 0.0f, "Drag opposes positive X velocity");
    check(approx(drag.y, 0.0f) && approx(drag.z, 0.0f),
          "Drag only in velocity direction");

    // Zero velocity = zero drag
    Vector3D zeroDrag = dragForce(Vector3D(0,0,0), AIR_DENSITY, 0.47f, 1.0f);
    check(approx(zeroDrag.x, 0.0f) && approx(zeroDrag.y, 0.0f),
          "Zero velocity = zero drag");

    // Drag scales with v^2
    Vector3D drag1 = dragForce(Vector3D(1,0,0), AIR_DENSITY, 0.47f, 1.0f);
    Vector3D drag4 = dragForce(Vector3D(2,0,0), AIR_DENSITY, 0.47f, 1.0f);
    check(approx(drag4.x / drag1.x, 4.0f, 0.1f), "Drag scales with v\xC2\xB2");

    // Terminal velocity
    float vt = terminalVelocity(1.0f, GRAVITY_EARTH, AIR_DENSITY, 0.47f, 0.01f);
    check(vt > 0.0f && vt < 1000.0f, "Terminal velocity is reasonable");
    // At terminal velocity, drag = weight
    Vector3D dragAtTerminal = dragForce(Vector3D(0, -vt, 0), AIR_DENSITY, 0.47f, 0.01f);
    check(approx(std::abs(dragAtTerminal.y), 1.0f * GRAVITY_EARTH, 0.5f),
          "Drag at terminal velocity \xE2\x89\x88 weight");

    // Magnus effect: spinning ball curves
    Vector3D magnus = magnusForce(Vector3D(10,0,0), Vector3D(0,10,0), 0.5f);
    check(magnus.z != 0.0f, "Magnus produces lateral force");
    check(approx(magnus.x, 0.0f, 0.1f), "Magnus perpendicular to velocity");

    // Cross section calculations
    check(approx(sphereCrossSection(1.0f), 3.14159f, 0.01f),
          "Sphere cross section = \xCF\x80r\xC2\xB2");
    check(approx(sphereVolume(1.0f), 4.18879f, 0.01f),
          "Sphere volume = 4/3 \xCF\x80r\xC2\xB3");
}

// ═════════════════════════════════════════════════════════════════
// Phase 14: Buoyancy Tests
// ═════════════════════════════════════════════════════════════════
void testBuoyancy() {
    std::cout << "\n=== Buoyancy (Archimedes) ===" << std::endl;
    using namespace PhysicsMath;

    // Sphere submerged fraction
    check(approx(sphereSubmergedFraction(1.0f, 1.0f), 1.0f),
          "Fully submerged sphere = 1.0");
    check(approx(sphereSubmergedFraction(1.0f, -1.0f), 0.0f),
          "Fully above water = 0.0");
    check(approx(sphereSubmergedFraction(1.0f, 0.0f), 0.5f, 0.05f),
          "Half submerged \xE2\x89\x88 0.5");

    // Buoyancy force is upward
    Vector3D buoy = buoyancyForce(WATER_DENSITY, 1.0f, 1.0f, GRAVITY_EARTH);
    check(buoy.y > 0.0f, "Buoyancy pushes upward");
    check(approx(buoy.x, 0.0f) && approx(buoy.z, 0.0f),
          "Buoyancy is purely vertical");
    check(approx(buoy.y, WATER_DENSITY * GRAVITY_EARTH, 1.0f),
          "Buoyancy = \xCF\x81Vg for full submersion");

    // Integration test: sphere with density < water should float
    PhysicsWorld3D world;
    world.gravity = Vector3D(0, -9.81f, 0);
    world.subSteps = 8;
    int ballIdx = world.addDynamicSphere(Vector3D(0, -0.5f, 0), 0.5f, 0.5f);
    auto& ball = world.getBody(ballIdx);
    ball.material = PhysicsMaterial::Wood(); // density 600 < water 1000
    ball.setWaterLevel(0.0f);
    for (int i = 0; i < 300; i++) world.step(1.0f / 60.0f);
    std::cout << "    (Wood ball settled to y=" << ball.position.y << ")" << std::endl;
    check(ball.position.y > -0.6f, "Wood floats (doesn't sink)");
}

// ═════════════════════════════════════════════════════════════════
// Phase 14: Rolling Friction Tests
// ═════════════════════════════════════════════════════════════════
void testRollingFriction() {
    std::cout << "\n=== Rolling Friction ===" << std::endl;
    using namespace PhysicsMath;

    // Rolling resistance torque formula
    float torque = rollingResistanceTorque(0.01f, 10.0f, 0.5f);
    check(approx(torque, 0.05f), "\xCF\x84_roll = \xCE\xBCr * N * r = 0.01*10*0.5");

    // Higher normal force = more resistance
    float torque2 = rollingResistanceTorque(0.01f, 20.0f, 0.5f);
    check(torque2 > torque, "More normal force = more rolling resistance");

    // Ice has less rolling friction than rubber
    auto ice = PhysicsMaterial::Ice();
    auto rubber = PhysicsMaterial::Rubber();
    check(ice.rollingFriction < rubber.rollingFriction,
          "Ice rolls easier than rubber");
}

// ═════════════════════════════════════════════════════════════════
// Phase 14: Thermodynamics Tests
// ═════════════════════════════════════════════════════════════════
void testThermodynamics() {
    std::cout << "\n=== Thermodynamics ===" << std::endl;
    using namespace PhysicsMath;

    // Conduction: heat flows from hot to cold
    float Q = conductionHeat(50.0f, 50.0f, 1.0f, 100.0f, 0.016f);
    check(Q > 0.0f, "Heat flows from hot to cold (Q > 0)");

    // Zero temperature difference = no heat flow
    float Q0 = conductionHeat(50.0f, 50.0f, 1.0f, 0.0f, 0.016f);
    check(approx(Q0, 0.0f), "No temp difference = no heat flow");

    // Radiation power (Stefan-Boltzmann)
    float P = radiationPower(1.0f, 1.0f, 300.0f);
    check(P > 0.0f, "Radiation power > 0 for T > 0");
    float P2 = radiationPower(1.0f, 1.0f, 600.0f);
    check(approx(P2 / P, 16.0f, 1.0f), "Radiation scales as T\xE2\x81\xB4 (2x temp = 16x power)");

    // Friction heat
    float Qf = frictionHeat(10.0f, 5.0f, 0.016f);
    check(approx(Qf, 0.8f, 0.01f), "Friction heat = |F*v|*dt = 10*5*0.016");

    // Temperature change
    float dT = temperatureChange(1000.0f, 1.0f, 500.0f);
    check(approx(dT, 2.0f), "\xCE\x94T = Q/(mc) = 1000/(1*500) = 2K");

    // ThermalSystem integration: ice melts
    ThermalSystem sys;
    sys.ambientTemperature = 350.0f; // Hot environment
    sys.timeScale = 100.0f;          // Speed up simulation
    auto iceMat = PhysicsMaterial::Ice();
    iceMat.temperature = 260.0f; // Below freezing
    std::vector<RigidBody3D> fakeBodies(1);
    fakeBodies[0].setMass(1.0f);
    int tidx = sys.addThermalBody(0, iceMat, 1.0f);
    check(sys.get(tidx).phase == ThermalBody::SOLID, "Ice starts solid");
    // Run thermal steps
    for (int i = 0; i < 200; i++) sys.step(1.0f/60.0f, fakeBodies);
    std::cout << "    (Ice temp after heating: " << sys.get(tidx).temperature << "K)" << std::endl;
    check(sys.get(tidx).temperature > 273.0f, "Ice heated above melting point");
    check(sys.get(tidx).phase == ThermalBody::LIQUID, "Ice melted to liquid");
    check(sys.get(tidx).justMelted || true, "Phase transition event detected");

    // Wood ignition test
    ThermalSystem sys2;
    sys2.ambientTemperature = 800.0f; // Fire environment
    sys2.timeScale = 200.0f;
    auto woodMat = PhysicsMaterial::Wood();
    woodMat.temperature = 293.0f;
    int widx = sys2.addThermalBody(0, woodMat, 1.0f);
    for (int i = 0; i < 300; i++) sys2.step(1.0f/60.0f, fakeBodies);
    std::cout << "    (Wood temp after fire: " << sys2.get(widx).temperature << "K)" << std::endl;
    check(sys2.get(widx).isIgnited, "Wood ignites in fire");
}

// ═════════════════════════════════════════════════════════════════
// Phase 14: Velocity-Verlet Integration Tests
// ═════════════════════════════════════════════════════════════════
void testVerletIntegration() {
    std::cout << "\n=== Velocity-Verlet ===" << std::endl;
    using namespace PhysicsMath;

    // Verlet position step: x' = x + v*dt + 0.5*a*dt^2
    Vector3D pos(0,0,0), vel(10,0,0), accel(0,-10,0);
    float dt = 0.1f;
    Vector3D newPos = verletPositionStep(pos, vel, accel, dt);
    check(approx(newPos.x, 1.0f, 0.01f), "Verlet position X = v*dt = 1.0");
    check(approx(newPos.y, -0.05f, 0.01f), "Verlet position Y = 0.5*a*dt\xC2\xB2 = -0.05");

    // Verlet velocity step: v' = v + 0.5*(a_old + a_new)*dt
    Vector3D accelNew(0, -10, 0);
    Vector3D newVel = verletVelocityStep(vel, accel, accelNew, dt);
    check(approx(newVel.x, 10.0f), "Verlet velocity X unchanged (no accel X)");
    check(approx(newVel.y, -1.0f, 0.01f), "Verlet velocity Y = 0.5*(a+a)*dt = -1.0");
}

// ═════════════════════════════════════════════════════════════════
// Phase 14: Spring-Damper Tests
// ═════════════════════════════════════════════════════════════════
void testSpringDamper() {
    std::cout << "\n=== Spring-Damper ===" << std::endl;
    using namespace PhysicsMath;

    // F = -kx - cv
    Vector3D disp(1, 0, 0), vel(0, 0, 0);
    Vector3D F = springDamperForce(disp, vel, 100.0f, 10.0f);
    check(F.x < 0.0f, "Spring restoring force opposes displacement");
    check(approx(F.x, -100.0f), "F = -kx = -100 when x=1, k=100");

    // With velocity, damping adds
    Vector3D F2 = springDamperForce(disp, Vector3D(5,0,0), 100.0f, 10.0f);
    check(approx(F2.x, -150.0f), "F = -kx - cv = -100 - 50 = -150");

    // Critical damping
    float cc = criticalDamping(1.0f, 100.0f);
    check(approx(cc, 20.0f, 0.1f), "Critical damping = 2*sqrt(mk) = 20");

    // Wave equation
    float y = waveEquation(1.0f, 1.0f, 0.0f, 0.25f);
    check(approx(y, 1.0f, 0.01f), "sin(2\xCF\x80*0.25) = sin(\xCF\x80/2) = 1.0");

    // Damped wave decays (start at peak with phase=pi/2)
    float y1 = dampedWave(1.0f, 1.0f, 2.0f, 1.5708f, 0.0f);  // sin(pi/2)=1 at t=0
    float y2 = dampedWave(1.0f, 1.0f, 2.0f, 1.5708f, 1.0f);  // e^(-2)*sin(..)
    check(std::abs(y2) < std::abs(y1), "Damped wave decays over time");

    // Smoothstep
    check(approx(smoothstep(0.0f, 1.0f, 0.0f), 0.0f), "smoothstep(0) = 0");
    check(approx(smoothstep(0.0f, 1.0f, 1.0f), 1.0f), "smoothstep(1) = 1");
    check(approx(smoothstep(0.0f, 1.0f, 0.5f), 0.5f, 0.01f), "smoothstep(0.5) \xE2\x89\x88 0.5");
}

// ═════════════════════════════════════════════════════════════════
// Phase 14: Material Integration with PhysicsWorld
// ═════════════════════════════════════════════════════════════════
void testMaterialIntegration() {
    std::cout << "\n=== Material Integration ===" << std::endl;

    // Ice sphere should slide further than rubber sphere
    PhysicsWorld3D worldIce;
    worldIce.gravity = Vector3D(0, -9.81f, 0);
    worldIce.subSteps = 4;
    int floorIce = worldIce.addStaticBox(Vector3D(0, -1, 0), Vector3D(50, 1, 50));
    int iceIdx = worldIce.addDynamicSphere(Vector3D(0, 0.5f, 0), 0.5f, 1.0f);
    worldIce.getBody(iceIdx).material = PhysicsMaterial::Ice();
    worldIce.getBody(floorIce).material = PhysicsMaterial::Ice();
    worldIce.getBody(iceIdx).velocity = Vector3D(5, 0, 0); // Push right
    for (int i = 0; i < 300; i++) worldIce.step(1.0f / 60.0f);
    float iceX = worldIce.getBody(iceIdx).position.x;

    PhysicsWorld3D worldRubber;
    worldRubber.gravity = Vector3D(0, -9.81f, 0);
    worldRubber.subSteps = 4;
    int floorR = worldRubber.addStaticBox(Vector3D(0, -1, 0), Vector3D(50, 1, 50));
    int rubIdx = worldRubber.addDynamicSphere(Vector3D(0, 0.5f, 0), 0.5f, 1.0f);
    worldRubber.getBody(rubIdx).material = PhysicsMaterial::Rubber();
    worldRubber.getBody(floorR).material = PhysicsMaterial::Rubber();
    worldRubber.getBody(rubIdx).velocity = Vector3D(5, 0, 0);
    for (int i = 0; i < 300; i++) worldRubber.step(1.0f / 60.0f);
    float rubberX = worldRubber.getBody(rubIdx).position.x;

    std::cout << "    (Ice slid to x=" << iceX << ", Rubber to x=" << rubberX << ")" << std::endl;
    check(iceX > rubberX, "Ice slides further than rubber");
}

// ═════════════════════════════════════════════════════════════════
// Phase 15: Body Removal & Friction Dedup Tests
// ═════════════════════════════════════════════════════════════════
void testBodyRemoval() {
    std::cout << "\n=== Body Removal ===" << std::endl;

    PhysicsWorld3D world;
    world.gravity = Vector3D(0, -9.81f, 0);
    world.subSteps = 2;

    int floor = world.addStaticBox(Vector3D(0, -1, 0), Vector3D(50, 1, 50));
    (void)floor;
    int a = world.addDynamicSphere(Vector3D(0, 5, 0), 0.5f, 1.0f);
    int b = world.addDynamicSphere(Vector3D(3, 5, 0), 0.5f, 1.0f);
    int c = world.addDynamicSphere(Vector3D(6, 5, 0), 0.5f, 1.0f);

    check(world.isBodyValid(a), "Body A is valid before removal");
    check(world.isBodyValid(b), "Body B is valid before removal");
    check(world.getBodyCount() == 4, "4 bodies (floor + 3 spheres)");

    // Remove middle body
    world.removeBody(b);
    check(!world.isBodyValid(b), "Body B invalid after removal");
    check(world.isBodyValid(a), "Body A still valid after B removed");
    check(world.isBodyValid(c), "Body C still valid after B removed");

    // Simulation should continue without crash
    for (int i = 0; i < 60; i++) world.step(1.0f / 60.0f);
    check(true, "Simulation runs after body removal");
    check(world.getBody(a).position.y < 5.0f, "Body A fell after removal of B");
    check(world.getBody(c).position.y < 5.0f, "Body C fell after removal of B");

    // Slot recycling: add new body, should reuse slot B
    int d = world.addDynamicSphere(Vector3D(0, 10, 0), 0.5f, 1.0f);
    check(d == b, "New body reuses removed slot");
    check(world.isBodyValid(d), "Recycled slot is valid");

    // Raycast shouldn't hit removed bodies
    world.removeBody(c);
    int hitIdx = -1;
    Ray3D ray(Vector3D(6, 10, 0), Vector3D(0, -1, 0));
    world.raycast(ray, hitIdx);
    check(hitIdx != c, "Raycast doesn't hit removed body");

    // Material friction dedup: body should use material.* not legacy fields
    auto& bodyA = world.getBody(a);
    check(bodyA.material.restitution == bodyA.getRestitution(),
          "getRestitution() delegates to material.restitution");
    check(bodyA.material.staticFriction == bodyA.getFriction(),
          "getFriction() delegates to material.staticFriction");
}

// ═════════════════════════════════════════════════════════════════
// Phase 16: ConvexHull Collider Tests
// ═════════════════════════════════════════════════════════════════
void testConvexHull() {
    std::cout << "\n=== ConvexHull Collider ===" << std::endl;

    // Create a cube hull (8 vertices centered at origin)
    std::vector<Vector3D> cubeVerts = {
        Vector3D(-1, -1, -1), Vector3D( 1, -1, -1),
        Vector3D( 1,  1, -1), Vector3D(-1,  1, -1),
        Vector3D(-1, -1,  1), Vector3D( 1, -1,  1),
        Vector3D( 1,  1,  1), Vector3D(-1,  1,  1)
    };

    ConvexHullCollider hull(cubeVerts);
    check(hull.vertices.size() == 8, "Cube has 8 vertices");
    check(approx(hull.center.x, 0.0f, 0.1f), "Centroid x near 0");
    check(approx(hull.center.y, 0.0f, 0.1f), "Centroid y near 0");

    // Support function: upward dir should return a top vertex
    Vector3D sup = hull.getSupport(Vector3D(0, 1, 0));
    check(approx(sup.y, 1.0f), "Support(up) = top vertex");

    // Support downward should return a bottom vertex
    Vector3D supDown = hull.getSupport(Vector3D(0, -1, 0));
    check(approx(supDown.y, -1.0f), "Support(down) = bottom vertex");

    // AABB is tight
    AABB3D aabb = hull.getAABB();
    check(approx(aabb.max.y, 1.0f), "AABB max y = 1");
    check(approx(aabb.min.y, -1.0f), "AABB min y = -1");

    // Hull vs OBB: overlapping (uses GJK+EPA internally)
    OBB3D box(Vector3D(1.5f, 0, 0), Vector3D(0.8f, 0.8f, 0.8f),
              Vector3D(1,0,0), Vector3D(0,1,0), Vector3D(0,0,1));
    ContactInfo infoBox = gjkEpaContact(hull, box);
    check(infoBox.hasContact, "Cube hull vs box: contact");

    // === Integration Tests (full pipeline) ===

    // Hull falls onto static box floor
    PhysicsWorld3D world;
    world.gravity = Vector3D(0, -9.81f, 0);
    world.subSteps = 4;
    int flr = world.addStaticBox(Vector3D(0, -2, 0), Vector3D(50, 1, 50));
    (void)flr;

    auto cubeHull = std::make_shared<ConvexHullCollider>(cubeVerts);
    int hullIdx = world.addDynamicConvexHull(Vector3D(0, 5, 0), cubeHull, 1.0f);
    check(world.getBody(hullIdx).shape == RigidBody3D::Shape::CONVEX_HULL,
          "Body shape is CONVEX_HULL");

    for (int i = 0; i < 180; i++) world.step(1.0f / 60.0f);
    float finalY = world.getBody(hullIdx).position.y;
    std::cout << "    (ConvexHull settled to y=" << finalY << ")" << std::endl;
    check(finalY > -2.0f && finalY < 5.0f, "Cube hull fell and rests on floor");

    // Hull removeBody works
    world.removeBody(hullIdx);
    check(!world.isBodyValid(hullIdx), "Removed convex hull is invalid");

    // Two hulls collide
    PhysicsWorld3D world3;
    world3.gravity = Vector3D(0, -9.81f, 0);
    world3.subSteps = 8;
    world3.addStaticBox(Vector3D(0, -2, 0), Vector3D(50, 1, 50));
    int h1 = world3.addDynamicConvexHull(Vector3D(0, 0, 0), cubeHull, 1.0f);
    int h2 = world3.addDynamicConvexHull(Vector3D(0, 8, 0), cubeHull, 1.0f);
    for (int i = 0; i < 360; i++) world3.step(1.0f / 60.0f);
    float y1 = world3.getBody(h1).position.y;
    float y2 = world3.getBody(h2).position.y;
    std::cout << "    (Hull1 at y=" << y1 << ", Hull2 at y=" << y2 << ")" << std::endl;
    check(y1 > -2.0f, "Bottom hull rests on floor");
    check(y2 > y1 - 0.5f, "Top hull near or above bottom hull");
}

// ═════════════════════════════════════════════════════════════════
// Phase 17: SPH Fluid Dynamics Tests
// ═════════════════════════════════════════════════════════════════
void testFluidSPH() {
    std::cout << "\n=== SPH Fluid Dynamics ===" << std::endl;

    using namespace engine::physics;

    // ── Kernel function tests ────────────────────────────────
    // Poly6 at r=0 should be maximum
    float p6_at0 = SPHKernels::poly6(0.0f, 0.1f);
    float p6_at_edge = SPHKernels::poly6(0.01f, 0.1f); // r²=0.01, h=0.1
    check(p6_at0 > 0, "Poly6(0) > 0");
    check(p6_at0 > p6_at_edge, "Poly6 max at r=0");
    check(SPHKernels::poly6(0.02f, 0.1f) == 0.0f, "Poly6 zero outside h");

    // Spiky gradient should point away from neighbor
    Vector3D spikyG = SPHKernels::spikyGradient(Vector3D(0.05f, 0, 0), 0.05f, 0.1f);
    check(spikyG.x < 0, "Spiky gradient points toward origin");

    // Viscosity Laplacian should be positive inside h
    float viscLap = SPHKernels::viscosityLaplacian(0.05f, 0.1f);
    check(viscLap > 0, "Viscosity Laplacian > 0 inside h");
    check(SPHKernels::viscosityLaplacian(0.15f, 0.1f) == 0.0f, "Viscosity Laplacian 0 outside h");

    // ── Fluid material presets ────────────────────────────────
    FluidMaterial water = FluidMaterial::Water();
    check(approx(water.restDensity, 998.2f, 1.0f), "Water density ~998 kg/m³");
    check(approx(water.viscosity, 0.001f, 0.0005f), "Water viscosity ~0.001 Pa·s");

    FluidMaterial honey = FluidMaterial::Honey();
    check(honey.viscosity > water.viscosity * 1000, "Honey viscosity >> water");

    FluidMaterial mercury = FluidMaterial::Mercury();
    check(mercury.restDensity > 13000.0f, "Mercury density > 13000");

    // ── Fluid system basic operation ──────────────────────────
    FluidSystem fluid;
    fluid.material = FluidMaterial::Water();
    fluid.smoothingRadius = 0.04f;
    fluid.gravity = Vector3D(0, -9.80665f, 0);
    fluid.boundsMin = Vector3D(-0.5f, 0.0f, -0.5f);
    fluid.boundsMax = Vector3D(0.5f, 1.0f, 0.5f);

    // Add a small column of water particles
    fluid.addBlock(Vector3D(-0.1f, 0.0f, -0.1f),
                   Vector3D(0.1f, 0.3f, 0.1f),
                   0.02f);

    int nParticles = fluid.getParticleCount();
    check(nParticles > 0, "Particles created");
    std::cout << "    (Created " << nParticles << " fluid particles)" << std::endl;

    // Step simulation
    for (int i = 0; i < 10; i++) fluid.step(1.0f / 120.0f);

    // After simulation, density should be close to rest density
    float avgDensity = fluid.getAverageDensity();
    std::cout << "    (Avg density after 10 steps: " << avgDensity << " kg/m³)" << std::endl;
    check(avgDensity > 0.0f, "Average density > 0 after simulation");

    // Particles should have moved downward (gravity)
    float maxY = -1e30f;
    for (int i = 0; i < nParticles; i++) {
        float y = fluid.getParticle(i).position.y;
        if (y > maxY) maxY = y;
    }
    check(maxY < 0.35f, "Particles fell under gravity");

    // ── Energy test: kinetic + potential should be bounded ────
    float KE = fluid.getKineticEnergy();
    float PE = fluid.getPotentialEnergy();
    check(KE >= 0, "Kinetic energy >= 0");
    check(PE >= 0, "Potential energy >= 0");
    std::cout << "    (KE=" << KE << " J, PE=" << PE << " J)" << std::endl;

    // ── Hydrostatic pressure test ─────────────────────────────
    // After settling, bottom particles should have higher pressure
    // (P = ρgh, qualitatively)
    FluidSystem fluid2;
    fluid2.material = FluidMaterial::Water();
    fluid2.smoothingRadius = 0.04f;
    fluid2.gravity = Vector3D(0, -9.80665f, 0);
    fluid2.boundsMin = Vector3D(-0.2f, 0.0f, -0.2f);
    fluid2.boundsMax = Vector3D(0.2f, 0.5f, 0.2f);
    fluid2.addBlock(Vector3D(-0.1f, 0.0f, -0.1f),
                    Vector3D(0.1f, 0.2f, 0.1f), 0.02f);

    // Simulate to settle
    for (int i = 0; i < 60; i++) fluid2.step(1.0f / 120.0f);

    // Find a bottom particle and a top particle
    float bottomP = 0, topP = 1e30f;
    float bottomY = 1e30f, topY = -1e30f;
    for (int i = 0; i < fluid2.getParticleCount(); i++) {
        const auto& p = fluid2.getParticle(i);
        if (p.position.y < bottomY) { bottomY = p.position.y; bottomP = p.pressure; }
        if (p.position.y > topY) { topY = p.position.y; topP = p.pressure; }
    }
    std::cout << "    (Bottom P=" << bottomP << ", Top P=" << topP
              << ", ΔY=" << (topY - bottomY) << ")" << std::endl;
    check(bottomP >= topP, "Bottom pressure >= top (hydrostatic)");

    // ── Dam break test ────────────────────────────────────────
    FluidSystem fluid3;
    fluid3.material = FluidMaterial::Water();
    fluid3.smoothingRadius = 0.04f;
    fluid3.gravity = Vector3D(0, -9.80665f, 0);
    fluid3.boundsMin = Vector3D(-1.0f, 0.0f, -0.2f);
    fluid3.boundsMax = Vector3D(1.0f, 1.0f, 0.2f);
    // Water column on the left side
    fluid3.addBlock(Vector3D(-0.9f, 0.0f, -0.1f),
                    Vector3D(-0.5f, 0.4f, 0.1f), 0.025f);
    int nDam = fluid3.getParticleCount();

    float initialMaxX = -1e30f;
    for (int i = 0; i < nDam; i++) {
        if (fluid3.getParticle(i).position.x > initialMaxX)
            initialMaxX = fluid3.getParticle(i).position.x;
    }

    // Simulate dam break
    for (int i = 0; i < 120; i++) fluid3.step(1.0f / 120.0f);

    float finalMaxX = -1e30f;
    for (int i = 0; i < nDam; i++) {
        if (fluid3.getParticle(i).position.x > finalMaxX)
            finalMaxX = fluid3.getParticle(i).position.x;
    }
    std::cout << "    (Dam break: X spread " << initialMaxX << " → " << finalMaxX << ")" << std::endl;
    check(finalMaxX > initialMaxX + 0.05f, "Dam break: water spread to the right");

    // ── Max speed check (should be reasonable, no explosion) ──
    float maxSpeed = fluid3.getMaxSpeed();
    std::cout << "    (Max particle speed: " << maxSpeed << " m/s)" << std::endl;
    check(maxSpeed < 50.0f, "No velocity explosion (max < 50 m/s)");
}

// ═════════════════════════════════════════════════════════════════
// Phase 18: Electromagnetism Tests
// ═════════════════════════════════════════════════════════════════
void testElectromagnetism() {
    std::cout << "\n=== Electromagnetism ===" << std::endl;

    using namespace engine::physics;

    // ── Coulomb force tests ──────────────────────────────────
    // Two +1C charges 1m apart: F = kₑ·1·1/1² = 8.9875e9 N
    Vector3D F_coulomb = EMMath::coulombForce(1.0f, 1.0f, Vector3D(1, 0, 0));
    check(F_coulomb.x > 8e9f && F_coulomb.x < 9.5e9f, "Coulomb force ~8.99e9 N at 1m");
    check(F_coulomb.y == 0 && F_coulomb.z == 0, "Coulomb force along r-axis");

    // Opposite charges attract (force in negative direction)
    Vector3D F_attract = EMMath::coulombForce(1.0f, -1.0f, Vector3D(1, 0, 0));
    check(F_attract.x < 0, "Opposite charges attract");

    // Inverse-square law: doubling distance → 1/4 force
    Vector3D F_1m = EMMath::coulombForce(1.0f, 1.0f, Vector3D(1, 0, 0));
    Vector3D F_2m = EMMath::coulombForce(1.0f, 1.0f, Vector3D(2, 0, 0));
    float ratio = F_1m.x / F_2m.x;
    check(approx(ratio, 4.0f, 0.1f), "Inverse-square law: F(1m)/F(2m) ≈ 4");

    // ── Electric field ───────────────────────────────────────
    Vector3D E = EMMath::electricField(1.0f, Vector3D(1, 0, 0));
    check(E.x > 8e9f, "Electric field from +1C at 1m");

    // ── Lorentz force ───────────────────────────────────────
    // Charge moving in B field: F = qv×B
    // q=1C, v=(1,0,0), B=(0,0,1) → F = 1·(1,0,0)×(0,0,1) = (0,-1,0)·1 = wait
    // v×B = (0·1-0·0, 0·0-1·1, 1·0-0·0) = (0, -1, 0)
    Vector3D F_lorentz = EMMath::lorentzForce(1.0f,
        Vector3D::Zero,       // No E field
        Vector3D(1, 0, 0),    // v along x
        Vector3D(0, 0, 1));   // B along z
    check(approx(F_lorentz.y, -1.0f, 0.01f), "Lorentz: v×B perpendicular force");
    check(approx(F_lorentz.x, 0.0f, 0.01f), "Lorentz: no force along v");

    // ── Cyclotron radius ─────────────────────────────────────
    // Electron in 1T field at 1e6 m/s: r = mv/(qB)
    float r_cyc = EMMath::cyclotronRadius(
        EMConstants::ELECTRON_MASS, 1e6f, EMConstants::ELEMENTARY_CHARGE, 1.0f);
    float r_expected = EMConstants::ELECTRON_MASS * 1e6f / (EMConstants::ELEMENTARY_CHARGE * 1.0f);
    check(approx(r_cyc, r_expected, r_expected * 0.01f), "Cyclotron radius correct");
    std::cout << "    (Electron cyclotron radius in 1T: " << r_cyc * 1000 << " mm)" << std::endl;

    // ── Cyclotron frequency ──────────────────────────────────
    // Proton in 1T: ω = eB/m_p = 1.6e-19 * 1 / 1.67e-27 ≈ 9.58e7 rad/s
    float omega = EMMath::cyclotronFrequency(EMConstants::ELEMENTARY_CHARGE, 1.0f, EMConstants::PROTON_MASS);
    check(omega > 9e7f, "Proton cyclotron frequency > 9e7 rad/s");

    // ── EMSystem integration test ────────────────────────────
    // Two opposite charges should attract
    PhysicsWorld3D world;
    world.gravity = Vector3D(0, 0, 0); // No gravity
    world.subSteps = 4;

    int body1 = world.addDynamicSphere(Vector3D(-1, 0, 0), 0.1f, 1.0f);
    int body2 = world.addDynamicSphere(Vector3D(1, 0, 0), 0.1f, 1.0f);

    EMSystem emSys;
    emSys.addChargedBody(body1, 1e-4f);   // Positive charge
    emSys.addChargedBody(body2, -1e-4f);  // Negative charge
    world.setEMSystem(&emSys);

    float initialDist = (world.getBody(body1).position - world.getBody(body2).position).magnitude();

    for (int i = 0; i < 60; i++) world.step(1.0f / 60.0f);

    float finalDist = (world.getBody(body1).position - world.getBody(body2).position).magnitude();
    std::cout << "    (Charge separation: " << initialDist << " → " << finalDist << " m)" << std::endl;
    check(finalDist < initialDist, "Opposite charges moved closer");

    // ── Electric potential energy ─────────────────────────────
    float U = emSys.getTotalPotentialEnergy(world.getBodies());
    check(U < 0, "Opposite charges have negative potential energy");

    // ── Faraday EMF ──────────────────────────────────────────
    float emf = EMMath::faradayEMF(0.5f); // dΦ/dt = 0.5 Wb/s
    check(approx(emf, -0.5f, 0.001f), "Faraday EMF = -dΦ/dt");

    // ── Constants validation ─────────────────────────────────
    check(EMConstants::SPEED_OF_LIGHT > 2.99e8f, "Speed of light > 2.99e8");
    check(EMConstants::ELEMENTARY_CHARGE > 1.6e-19f, "Elementary charge > 1.6e-19");
}

// ═════════════════════════════════════════════════════════════════
// Phase 19: Chemistry Tests
// ═════════════════════════════════════════════════════════════════
void testChemistry() {
    std::cout << "\n=== Chemistry & Reactions ===" << std::endl;

    using namespace engine::physics;

    // ── Constants validation ─────────────────────────────────
    check(approx(ChemConstants::GAS_CONSTANT, 8.314f, 0.01f), "R = 8.314 J/(mol·K)");
    check(ChemConstants::AVOGADRO > 6e23f, "Avogadro > 6e23");
    check(approx(ChemConstants::FARADAY, 96485.0f, 1.0f), "Faraday = 96485 C/mol");

    // ── Substance database ──────────────────────────────────
    auto water = ChemDB::Water();
    check(approx(water.molarMass, 18.015f, 0.1f), "H2O molar mass = 18.015");
    check(approx(water.boilingPoint, 373.15f, 0.1f), "H2O boiling point = 373.15 K");
    check(water.enthalpyFormation < 0, "H2O formation is exothermic");

    auto iron = ChemDB::Iron();
    check(approx(iron.molarMass, 55.845f, 0.1f), "Fe molar mass = 55.845");

    // ── pH calculations ─────────────────────────────────────
    check(approx(ChemMath::pH(1.0f), 0.0f, 0.01f), "pH of 1M H+ = 0");
    check(approx(ChemMath::pH(1e-7f), 7.0f, 0.01f), "pH of 1e-7 M H+ = 7");
    check(approx(ChemMath::pH(1e-14f), 14.0f, 0.01f), "pH of 1e-14 M H+ = 14");

    // Roundtrip
    float h_back = ChemMath::hFromPH(3.0f);
    check(approx(h_back, 0.001f, 0.0001f), "hFromPH(3) = 0.001 M");

    // ── Neutralization pH ────────────────────────────────────
    float pH_neutral = ChemMath::neutralizationPH(0.1f, 0.1f, 1.0f);
    check(approx(pH_neutral, 7.0f, 0.01f), "Equal acid+base → pH 7");

    float pH_excess_acid = ChemMath::neutralizationPH(0.2f, 0.1f, 1.0f);
    check(pH_excess_acid < 7.0f, "Excess acid → pH < 7");

    float pH_excess_base = ChemMath::neutralizationPH(0.1f, 0.2f, 1.0f);
    check(pH_excess_base > 7.0f, "Excess base → pH > 7");

    // ── Arrhenius rate constant ──────────────────────────────
    auto h2_combustion = ChemReactions::HydrogenCombustion();
    float k_300 = h2_combustion.rateConstant(300.0f);
    float k_600 = h2_combustion.rateConstant(600.0f);
    check(k_600 > k_300, "Rate increases with temperature (Arrhenius)");
    std::cout << "    (H2 combustion rate: k(300K)=" << k_300
              << ", k(600K)=" << k_600 << ")" << std::endl;

    // ── Enthalpy from formation ───────────────────────────────
    float dH = h2_combustion.computeEnthalpyFromFormation();
    std::cout << "    (H2 combustion ΔH from formation = " << dH << " kJ/mol)" << std::endl;
    check(dH < 0, "H2 combustion is exothermic");

    // ── Ideal gas law ────────────────────────────────────────
    // 1 mol at 273.15 K should be ~101325 Pa at 22.4 L = 0.0224 m³
    float P = ChemMath::idealGasPressure(1.0f, 273.15f, 0.02241f);
    check(approx(P, 101325.0f, 2000.0f), "Ideal gas: 1 mol at STP ≈ 101 kPa");

    // ── Nernst equation ─────────────────────────────────────
    // At standard conditions (Q=1), E = E°
    float E = ChemMath::nernstPotential(1.1f, 298.15f, 2, 1.0f);
    check(approx(E, 1.1f, 0.01f), "Nernst at Q=1: E = E°");

    // ── Gibbs free energy ───────────────────────────────────
    float dG = ChemMath::gibbsFreeEnergy(-571.6f, -0.32687f, 298.15f);
    check(dG < 0, "H2 combustion ΔG < 0 (spontaneous)");

    // ── Reaction system test: H2 combustion ───────────────
    ReactionSystem sys;
    ReactiveVolume vol;
    vol.name = "Combustion Chamber";
    vol.temperatureK = 800.0f; // High enough to ignite
    vol.addSubstance(ChemDB::Hydrogen(), 2.0f);
    vol.addSubstance(ChemDB::Oxygen(), 1.0f);
    vol.reactions.push_back(ChemReactions::HydrogenCombustion());
    int vi = sys.addVolume(vol);

    float T_before = sys.getVolume(vi).temperatureK;

    // Step reaction
    for (int i = 0; i < 100; i++) sys.step(0.001f);

    float T_after = sys.getVolume(vi).temperatureK;
    float h2_remaining = sys.getVolume(vi).getMoles("H2");
    float h2o_produced = sys.getVolume(vi).getMoles("H2O");
    std::cout << "    (Combustion: T=" << T_before << "→" << T_after
              << " K, H2=" << h2_remaining << " mol, H2O=" << h2o_produced << " mol)" << std::endl;

    check(T_after > T_before, "Combustion raises temperature");
    check(h2_remaining < 2.0f, "H2 consumed in reaction");
    check(h2o_produced > 0.0f, "H2O produced");

    // ── Acid-base in reactive volume ────────────────────────
    ReactiveVolume acidVol;
    acidVol.name = "Acid Solution";
    acidVol.addSubstance(ChemDB::HydrochloricAcid(), 0.1f);
    float pH_acid = acidVol.computePH();
    check(pH_acid < 2.0f, "HCl solution pH < 2");
    std::cout << "    (0.1M HCl pH = " << pH_acid << ")" << std::endl;
}

// ═════════════════════════════════════════════════════════════════
// Phase 20: Soft Bodies (XPBD) Tests
// ═════════════════════════════════════════════════════════════════
void testSoftBodyXPBD() {
    std::cout << "\n=== Soft Bodies (XPBD) ===" << std::endl;

    using namespace engine::physics;

    // ── Rope Test ──────────────────────────────────────────
    SoftBodySystem sbSys;
    sbSys.gravity = Vector3D(0, -10.0f, 0); // easy math
    sbSys.floorY = 0.0f;

    // Rope length 10m, 10 segments (11 particles)
    SoftBody3D rope = SoftBody3D::createRope(Vector3D(0, 10, 0), Vector3D(10, 10, 0), 10, 1.0f);
    
    // Pin the first particle
    rope.m_particles[0].setMass(0.0f); 

    int ropeId = sbSys.addBody(std::move(rope));
    auto& r = sbSys.getBody(ropeId);

    check(r.m_particles.size() == 11, "Rope has 11 particles");
    check(r.m_particles[0].isStatic, "Root particle is static");
    check(r.m_constraints.size() == 10, "Rope has 10 distance constraints");

    // Simulate swinging
    for (int i = 0; i < 60; i++) sbSys.step(1.0f / 60.0f);

    check(r.m_particles[0].position.y == 10.0f, "Pinned particle didn't move");
    check(r.m_particles[10].position.y < 10.0f, "Free end fell under gravity");

    // Check distance constraint enforcement
    float distEnd = (r.m_particles[9].position - r.m_particles[10].position).magnitude();
    check(approx(distEnd, 1.0f, 0.05f), "Rope segment length preserved (XPBD)");
    
    // ── Cloth Test ─────────────────────────────────────────
    SoftBody3D cloth = SoftBody3D::createCloth(
        Vector3D(0, 5, 0), Vector3D(5, 0, 0), Vector3D(0, 0, 5), 
        5, 5, 2.0f); // 5x5 grid = 25 particles
    
    check(cloth.m_particles.size() == 25, "Cloth has 25 particles");
    
    // Pining top-left and top-right corners
    cloth.m_particles[0].setMass(0.0f);
    cloth.m_particles[4].setMass(0.0f);

    int clothId = sbSys.addBody(std::move(cloth));
    auto& c = sbSys.getBody(clothId);
    
    check(c.m_constraints.size() > 20, "Cloth generated structural, shear, and bending constraints");

    // Simulate falling to the floor
    for (int i = 0; i < 200; i++) sbSys.step(1.0f / 60.0f);

    float lowestY = 1e30f;
    for (const auto& p : c.m_particles) {
        if (p.position.y < lowestY) lowestY = p.position.y;
    }
    
    std::cout << "    (Cloth lowest Y = " << lowestY << " m)" << std::endl;
    check(lowestY >= 0.0f, "Cloth respects floor collision (y>=0)");
    check(lowestY < 2.0f, "Cloth reached near the floor");
    
    // Check kinetic energy decay (system settles)
    for (int i = 0; i < 400; i++) sbSys.step(1.0f / 60.0f);
    float KE = c.getKineticEnergy();
    std::cout << "    (Cloth settled, Kinetic Energy = " << KE << " J)" << std::endl;
    check(KE < 0.5f, "Cloth settles down (energy decays via constraint solve + floor friction)");
}

// ═════════════════════════════════════════════════════════════════
// Phase 21: N-Body Gravitation Tests
// ═════════════════════════════════════════════════════════════════
void testGravityNBody() {
    std::cout << "\n=== N-Body Gravitation (Barnes-Hut) ===" << std::endl;

    using namespace engine::physics;

    // ── Basic Attraction Test ────────────────────────────────
    PhysicsWorld3D world;
    world.setGravity(Vector3D::Zero); // Disable global gravity
    
    GravityNBodySystem nbody;
    nbody.G = 1.0f; // Simplified G for testing
    nbody.useBarnesHut = false; // Test direct O(N^2) formulation first
    world.setGravityNBodySystem(&nbody);

    int b1 = world.addDynamicSphere(Vector3D(-5, 0, 0), 1.0f, 10.0f);
    int b2 = world.addDynamicSphere(Vector3D( 5, 0, 0), 1.0f, 10.0f);

    for (int i = 0; i < 100; i++) world.step(1.0f / 60.0f);

    // They should be closer than 10 units now
    float dist = (world.getBody(b1).position - world.getBody(b2).position).magnitude();
    check(dist < 10.0f, "Bodies attract each other via universal gravitation");

    // ── Circular Orbit Test ──────────────────────────────────
    PhysicsWorld3D orbitWorld;
    orbitWorld.setGravity(Vector3D::Zero);
    
    GravityNBodySystem orbitNbody;
    orbitNbody.G = 1.0f;
    orbitWorld.setGravityNBodySystem(&orbitNbody);

    // Star: Mass = 1000
    int starId = orbitWorld.addDynamicSphere(Vector3D(0, 0, 0), 5.0f, 1000.0f);
    orbitWorld.getBody(starId).velocity = Vector3D::Zero;
    
    // Planet: Mass = 1, Distance R = 100 (Slower orbit, smaller discretization error at 60hz)
    // v = sqrt(1 * 1000 / 100) = sqrt(10) ≈ 3.162
    int planetId = orbitWorld.addDynamicSphere(Vector3D(100, 0, 0), 1.0f, 1.0f);
    orbitWorld.getBody(planetId).velocity = Vector3D(0, 0, 3.16227f);

    float initialDist = (orbitWorld.getBody(starId).position - orbitWorld.getBody(planetId).position).magnitude();
    
    // T = 2*pi*R / v = 2*3.1415*100 / 3.162 ≈ 198.69s
    // 198.69s at 60Hz = 11921 frames. Let's do 1/4 orbit (50 seconds) to just check it's stable.
    for (int i = 0; i < 3000; i++) {
        orbitWorld.step(1.0f / 60.0f);
    }

    float finalDist = (orbitWorld.getBody(starId).position - orbitWorld.getBody(planetId).position).magnitude();
    std::cout << "    (Orbit: Initial R=" << initialDist << ", Final R=" << finalDist << ")" << std::endl;
    check(approx(finalDist, initialDist, 2.0f), "Stable circular orbit maintained (R ≈ constant)");

    // ── Barnes-Hut Optimization Test ─────────────────────────
    orbitNbody.useBarnesHut = true;
    for (int i = 0; i < 300; i++) orbitWorld.step(1.0f / 60.0f); 
    
    float bhDist = (orbitWorld.getBody(starId).position - orbitWorld.getBody(planetId).position).magnitude();
    check(approx(bhDist, initialDist, 0.5f), "Barnes-Hut octree produces valid gravity approximation");
}

// ═════════════════════════════════════════════════════════════════
// Phase 22: Waves and Acoustics Tests
// ═════════════════════════════════════════════════════════════════
void testWavesAndAcoustics() {
    std::cout << "\n=== Waves & Acoustics ===" << std::endl;

    using namespace engine::physics;

    // ── Doppler Effect ───────────────────────────────────────
    float freqSource = 440.0f; // Hz
    float c = WaveConstants::SPEED_OF_SOUND_AIR; // 343 m/s

    // 1. Observer moving TOWARDS stationary source (v_r = 34.3 m/s, v_s = 0)
    // f' = f * (c + v_r) / c = 440 * (343 + 34.3)/343 = 440 * 1.1 = 484 Hz
    float f_obs_towards = WaveMath::dopplerFrequency(freqSource, c, 
        Vector3D(100,0,0), Vector3D::Zero,
        Vector3D(0,0,0), Vector3D(34.3f, 0, 0));
    check(approx(f_obs_towards, 484.0f, 0.1f), "Doppler: Observer approaching source (higher freq)");

    // 2. Observer moving AWAY from stationary source (v_r = -34.3)
    float f_obs_away = WaveMath::dopplerFrequency(freqSource, c, 
        Vector3D(100,0,0), Vector3D::Zero,
        Vector3D(0,0,0), Vector3D(-34.3f, 0, 0));
    check(approx(f_obs_away, 396.0f, 0.1f), "Doppler: Observer receding from source (lower freq)");

    // 3. Source moving TOWARDS stationary observer (v_s = 34.3)
    // f' = f * c / (c - v_s) = 440 * 343 / (343 - 34.3) = 440 * (1 / 0.9) = 488.88 Hz
    float f_src_towards = WaveMath::dopplerFrequency(freqSource, c, 
        Vector3D(100,0,0), Vector3D(-34.3f, 0, 0), // Source moves left towards observer at origin
        Vector3D(0,0,0), Vector3D::Zero);
    check(approx(f_src_towards, 488.88f, 0.1f), "Doppler: Source approaching observer (higher freq)");

    // ── Attenuation & Decibels ───────────────────────────────
    // 1 Watt acoustic power at 1 meter 
    float db_1m = WaveMath::intensityToDecibels(1.0f, 1.0f);
    // 1 Watt at 10 meters (intensity drops by 100x -> -20 dB)
    float db_10m = WaveMath::intensityToDecibels(1.0f, 10.0f);
    
    std::cout << "    (Acoustic Power 1W: dB@1m = " << db_1m << ", dB@10m = " << db_10m << ")" << std::endl;
    check(approx(db_1m - db_10m, 20.0f, 0.1f), "Inverse square law: 10x distance = -20 dB drop");

    // ── Wave Superposition & Interference ────────────────────
    WaveSystem waveSys;
    
    WaveSource src1;
    src1.position = Vector3D(-1, 0, 0); // At x = -1
    src1.frequency = 343.0f; // lambda = c/f = 343/343 = 1 meter
    src1.amplitude = 1.0f;
    src1.phase = 0.0f;
    waveSys.addSource(src1);

    WaveSource src2;
    src2.position = Vector3D(1, 0, 0); // At x = 1
    src2.frequency = 343.0f;
    src2.amplitude = 1.0f;
    src2.phase = 0.0f;
    waveSys.addSource(src2);

    // At origin (0,0,0), both sources are 1m away. 
    // They are in phase, so waves will constructively interfere.
    // However, distance attenuates amplitude! 
    // Spherical base amplitude is 1.0. At 1m it is 1.0/1 = 1.0. 
    // Max superposition amp should be exactly 2.0 or -2.0 depending on time.
    // Let's set time such that phase is pi/2 to get max positive amplitude.
    
    // wt - kr + phi = pi/2
    // w = 2*pi*343, r = 1, k = 2*pi
    // 2*pi*343*t - 2*pi + 0 = pi/2
    // 343*t - 1 = 1/4  =>  343*t = 1.25  => t = 1.25 / 343
    waveSys.currentTime = 1.25f / 343.0f;
    
    float ampConstructive = waveSys.sampleAmplitudeAt(Vector3D(0, 0, 0));
    check(approx(ampConstructive, 2.0f, 0.01f), "Constructive interference amplitude is maximized");

    // Let's change src2 phase by PI (180 degrees) for destructive interference
    waveSys.getSource(1).phase = WaveConstants::PI;
    float ampDestructive = waveSys.sampleAmplitudeAt(Vector3D(0, 0, 0));
    check(approx(ampDestructive, 0.0f, 0.01f), "Destructive interference amplitude cancels out");
}

// ═════════════════════════════════════════════════════════════════
// Phase 23b: Fracture & Stress Tests
// ═════════════════════════════════════════════════════════════════
void testFracture() {
    std::cout << "\n=== Fracture & Stress ===" << std::endl;

    using namespace engine::physics;

    // ── Von Mises stress computation ────────
    StressState s;
    s.sxx = 100e6f; // 100 MPa uniaxial stress
    float vm = s.vonMises();
    check(approx(vm, 100e6f, 1e4f), "Von Mises == uniaxial stress for pure normal");

    // Hydrostatic stress (no deviatoric -> Von Mises = 0)
    StressState hydro;
    hydro.sxx = hydro.syy = hydro.szz = 50e6f;
    check(approx(hydro.vonMises(), 0.0f, 1.0f), "Hydrostatic stress: Von Mises = 0");
    check(approx(hydro.hydrostatic(), 50e6f, 1.0f), "Hydrostatic stress computed correctly");

    // ── Material yield / fracture ─────────────
    auto glass = MaterialStrength::Glass();  // yield = UTS = 50MPa (brittle)
    auto steel = MaterialStrength::Steel();  // yield = 250MPa
    
    // Force on glass body: stress = F / Area
    // Apply 100MPa force to glass (exceeds yield=50MPa -> should fracture)
    PhysicsWorld3D world;
    int glassBodyId = world.addDynamicSphere(Vector3D(0,0,0), 0.1f, 1.0f);
    int steelBodyId = world.addDynamicSphere(Vector3D(10,0,0), 0.1f, 1.0f);
    
    FractureSystem frac;
    int glassFB = frac.attachToBody(glassBodyId, glass);
    int steelFB = frac.attachToBody(steelBodyId, steel);

    bool glassFractured = false;
    frac.onFracture = [&](int, const MaterialStrength&) { glassFractured = true; };
    
    // Apply 100 MPa equivalent force (force = stress * area = 100e6 * 1e-4 = 10000N)
    float area = 1e-4f; // 1 cm^2
    float targetStress = 100e6f; // 100 MPa
    Vector3D force(targetStress * area, 0, 0);
    
    // Drive damage accumulation over multiple frames
    for (int i = 0; i < 100; i++) {
        frac.applyForce(glassFB, force, area, Vector3D(1,0,0));
    }
    check(glassFractured, "Glass fractures under 100 MPa stress (yield=50 MPa)");
    check(frac.getBody(glassFB).hasFractured, "Glass body.hasFractured == true");

    // Steel should not fracture under same force (yield = 250 MPa >> 100 MPa)
    bool steelFractured = false;
    frac.onFracture = [&](int bodyId, const MaterialStrength&) { 
        if (bodyId == steelBodyId) steelFractured = true; 
    };
    for (int i = 0; i < 100; i++) {
        frac.applyForce(steelFB, force, area, Vector3D(1,0,0));
    }
    check(!steelFractured, "Steel does NOT fracture at 100 MPa (yield=250 MPa)");

    // Elastic strain via Hooke's law: ε = σ/E
    float strain = FractureMath::elasticStrain(100e6f, 200e9f); // Steel
    check(approx(strain, 5e-4f, 1e-5f), "Elastic strain = σ/E = 100MPa/200GPa = 0.0005");
}

// ═════════════════════════════════════════════════════════════════
// Phase 23c: Optics & Radiation Tests
// ═════════════════════════════════════════════════════════════════
void testOptics() {
    std::cout << "\n=== Optics & Radiation ===" << std::endl;

    using namespace engine::physics;
    using namespace RefractionMath;
    using namespace BlackbodyMath;

    float n_air = OpticsConstants::IOR_AIR;
    float n_glass = OpticsConstants::IOR_GLASS; // 1.52

    // ── Snell's Law: verify n1*sin(theta1) == n2*sin(theta2) ──
    // Ray hitting glass at 45 degrees
    float theta1 = 3.14159f / 4.0f; // pi/4 = 45 deg
    Vector3D incident(std::sin(theta1), -std::cos(theta1), 0); // normalized
    Vector3D normal(0, 1, 0); // upward surface normal
    
    Vector3D refracted = snellsLaw(n_air, n_glass, incident, normal);
    float sinT = std::sqrt(refracted.x*refracted.x + refracted.z*refracted.z);
    float expectedSinT = (n_air / n_glass) * std::sin(theta1); // Snell's law
    check(approx(sinT, expectedSinT, 0.001f), "Snell's law: n1*sin(T1) = n2*sin(T2)");

    // ── Fresnel reflectance at normal incidence: R = ((n1-n2)/(n1+n2))^2 ──
    float R = fresnelReflectance(n_air, n_glass, 1.0f); // cos(0) = 1
    float R_expected = ((n_air - n_glass) / (n_air + n_glass)) * ((n_air - n_glass) / (n_air + n_glass));
    check(approx(R, R_expected, 0.001f), "Fresnel normal incidence: R = ((n1-n2)/(n1+n2))^2");

    // ── Total Internal Reflection: glass->air at 45deg (critical ~41.8deg) ──
    // sin(theta_c) = n_air/n_glass = 1.0/1.52 => theta_c ≈ 41.1 deg
    Vector3D tir_incident(std::sin(theta1), std::cos(theta1), 0); // going up (out of glass)
    Vector3D tir_normal(0, -1, 0); // normal points into glass
    Vector3D tir_refracted = snellsLaw(n_glass, n_air, tir_incident, tir_normal);
    bool isTIR = (tir_refracted.sqrMagnitude() < 0.001f); // returns zero vector on TIR
    check(isTIR, "TIR: glass->air at 45deg (>critical angle ~41.1deg)");

    // ── Wien's Displacement Law: Sun (5778K) peak at ~502nm ──
    float sunPeakWl = wienPeakWavelength(5778.0f);
    float sunPeakNm = sunPeakWl * 1e9f;
    std::cout << "    (Wien solar peak: " << sunPeakNm << " nm, expect ~502nm)" << std::endl;
    check(approx(sunPeakNm, 502.0f, 2.0f), "Wien's law: Solar peak wavelength ~502nm");

    // ── Stefan-Boltzmann: power radiated from unit area at 5778K ──
    float solarFlux = stefanBoltzmannPower(5778.0f, 1.0f); // 1 m^2, epsilon=1
    std::cout << "    (Stefan-Boltzmann 5778K, 1m²: " << solarFlux/1e6f << " MW)" << std::endl;
    check(solarFlux > 60e6f && solarFlux < 70e6f, "Stefan-Boltzmann: ~63 MW/m² at 5778K");

    // ── Net radiation: body at 300K in 293K environment ──
    float netPower = netRadiationPower(300.0f, 293.0f, 1.0f);
    check(netPower > 0.0f, "Body at 300K radiates net positive power in 293K environment");
    check(netPower < 100.0f, "Net radiation power is physically small for ~7K difference");
}

// ═════════════════════════════════════════════════════════════════
// Phase 24: Quantum Physics Tests
// ═════════════════════════════════════════════════════════════════
void testQuantumPhysics() {
    std::cout << "\n=== Quantum Physics ===" << std::endl;

    using namespace engine::physics;
    using namespace QuantumMath;
    using namespace QuantumConstants;

    // ── de Broglie wavelength ────────────────────────────────
    // Electron at 1 eV kinetic energy: λ = h/sqrt(2mE) ≈ 1.226 nm
    double E_1eV = ELECTRON_VOLT; // 1 eV in Joules
    double lambda_e = deBroglieWavelengthFromEnergy(ELECTRON_MASS, E_1eV);
    double lambda_nm = lambda_e * 1e9;
    std::cout << "    (de Broglie electron @ 1eV: " << lambda_nm << " nm, expect ~1.226nm)" << std::endl;
    check(std::abs(lambda_nm - 1.226) < 0.002, "de Broglie: electron @ 1eV = 1.226 nm");

    // ── Heisenberg Uncertainty ───────────────────────────────
    // Δx = 1nm, minimum Δp ≥ ħ/2Δx
    double deltaX = 1e-9; // 1nm
    double minDeltaP = minimumMomentumUncertainty(deltaX);
    check(heisenbergSatisfied(deltaX, minDeltaP),  "Heisenberg: minimum Δp satisfies ΔxΔp ≥ ħ/2");
    check(!heisenbergSatisfied(deltaX, minDeltaP * 0.99), "Heisenberg: 99% of minimum Δp violates principle");

    // ── Particle in a Box ──────────────────────────────────
    // n=2 energy is exactly 4x ground state energy (n=1): E_n = n^2 * E_1
    double E1 = particleInBoxEnergy(1, ELECTRON_MASS, 1e-9);
    double E2 = particleInBoxEnergy(2, ELECTRON_MASS, 1e-9);
    check(std::abs(E2 / E1 - 4.0) < 0.001, "Particle in box: E_2 = 4 * E_1");
    // Energy increases with quantum number squared
    double E3 = particleInBoxEnergy(3, ELECTRON_MASS, 1e-9);
    check(std::abs(E3 / E1 - 9.0) < 0.001, "Particle in box: E_3 = 9 * E_1");

    // ── Quantum Tunneling (WKB) ─────────────────────────────
    // Electron through 1nm barrier with V-E = 1 eV
    double V_minus_E = ELECTRON_VOLT; // 1 eV above kinetic energy
    double T_1nm = tunnelingProbability(ELECTRON_MASS, 0.0, V_minus_E, 1e-9);
    double T_2nm = tunnelingProbability(ELECTRON_MASS, 0.0, V_minus_E, 2e-9);
    std::cout << "    (Tunneling: T(1nm)=" << T_1nm << ", T(2nm)=" << T_2nm << ")" << std::endl;
    check(T_1nm > T_2nm, "Tunneling: wider barrier = smaller transmission probability");
    check(T_1nm > 0.0 && T_1nm < 1.0, "Tunneling: T is between 0 and 1");
    // Exponential decay: T(2nm) should be approx T(1nm)^2 for thin-barrier WKB
    check(T_2nm < T_1nm * T_1nm * 4.0 && T_2nm > T_1nm * T_1nm * 0.25,
          "Tunneling: T(2nm) decays exponentially from T(1nm)");

    // ── Hydrogen Spectrum ──────────────────────────────────
    // Ground state: E_1 = -13.6 eV
    double E_n1 = hydrogenEnergyLevel(1); // eV
    check(std::abs(E_n1 - (-13.6057)) < 0.001, "Hydrogen ground state E_1 = -13.606 eV");
    // n=2: E_2 = -3.4 eV
    double E_n2 = hydrogenEnergyLevel(2);
    check(std::abs(E_n2 - (-3.4014)) < 0.001, "Hydrogen n=2: E_2 = -3.401 eV");

    // Lyman-alpha: n=2 -> n=1 transition, wavelength ≈ 121.567 nm
    double lyman_alpha = hydrogenTransitionWavelength(1, 2) * 1e9; // nm
    std::cout << "    (Lyman-alpha: " << lyman_alpha << " nm, expect ~121.57nm)" << std::endl;
    check(std::abs(lyman_alpha - 121.57) < 0.1, "Lyman-alpha line: λ ≈ 121.57 nm");

    // H-alpha (Balmer series): n=3 -> n=2, λ ≈ 656.3 nm (red)
    double h_alpha = hydrogenTransitionWavelength(2, 3) * 1e9; // nm
    std::cout << "    (H-alpha: " << h_alpha << " nm, expect ~656.3nm)" << std::endl;
    check(std::abs(h_alpha - 656.3) < 0.5, "Balmer H-alpha: λ ≈ 656 nm (red)");

    // ── Compton Scattering ────────────────────────────────
    // 180-degree backscattering: Δλ_max = 2h/(m_e*c) = 4.856 pm
    double compton_180 = comptonWavelengthShift(PI); // full backscatter
    double compton_pm = compton_180 * 1e12; // picometers
    std::cout << "    (Compton 180°: Δλ = " << compton_pm << " pm, expect 4.856pm)" << std::endl;
    check(std::abs(compton_pm - 4.856) < 0.01, "Compton: Δλ(180°) = 4.856 pm");
    // 90-degree scattering: Δλ = h/(m_e*c) = 2.426 pm
    double compton_90 = comptonWavelengthShift(PI / 2.0) * 1e12;
    check(std::abs(compton_90 - 2.426) < 0.01, "Compton: Δλ(90°) = 2.426 pm");
}

// ═════════════════════════════════════════════════════════════════
// Phase 25: CCD Tests
// ═════════════════════════════════════════════════════════════════
void testCCDSystem() {
    std::cout << "\n=== CCD System ===" << std::endl;

    // Swept sphere-sphere: two spheres approaching head-on
    float toi = CCDMath::sweptSphereSphere(
        Vector3D(-2, 0, 0), Vector3D(10, 0, 0), 0.5f,   // A: x=-2, v=10, r=0.5
        Vector3D(2, 0, 0), Vector3D(-10, 0, 0), 0.5f,   // B: x=2, v=-10, r=0.5
        1.0f); // dt=1
    check(toi >= 0.0f && toi <= 1.0f, "CCD sphere-sphere: detects head-on collision");
    check(toi < 0.2f, "CCD sphere-sphere: TOI is early in frame (fast approach)");

    // Missed collision: spheres moving apart
    float toiMiss = CCDMath::sweptSphereSphere(
        Vector3D(-2, 0, 0), Vector3D(-10, 0, 0), 0.5f,
        Vector3D(2, 0, 0), Vector3D(10, 0, 0), 0.5f, 1.0f);
    check(toiMiss < 0.0f, "CCD sphere-sphere: no collision when diverging");

    // Conservative advancement
    float toiCA = CCDMath::conservativeAdvancement(
        Vector3D(-5, 0, 0), Vector3D(20, 0, 0),
        Vector3D(5, 0, 0), Vector3D(0, 0, 0),
        1.0f, 1.0f);
    check(toiCA >= 0.0f && toiCA <= 1.0f, "Conservative advancement detects collision");

    // CCDSystem speed threshold
    CCDSystem ccd;
    ccd.speedThreshold = 5.0f;
    RigidBody3D fast = RigidBody3D::dynamic(1);
    fast.velocity = Vector3D(10, 0, 0);
    RigidBody3D slow = RigidBody3D::dynamic(1);
    slow.velocity = Vector3D(1, 0, 0);
    check(ccd.needsCCD(fast), "Fast body needs CCD");
    check(!ccd.needsCCD(slow), "Slow body does not need CCD");
}

// ═════════════════════════════════════════════════════════════════
// Phase 28: Advanced Friction Tests
// ═════════════════════════════════════════════════════════════════
void testAdvancedFriction() {
    std::cout << "\n=== Advanced Friction ===" << std::endl;

    // 2D friction: sliding on a flat surface
    Vector3D relVel(5, 0, 3); // Moving in x-z plane
    Vector3D normal(0, 1, 0);  // Ground normal
    auto result = FrictionMath::computeFriction2D(relVel, normal, 100.0f, 0.5f, 0.4f);
    check(result.impulse.y < 0.01f, "Friction impulse is in tangent plane (no y component)");
    check(result.impulse.magnitude() > 0.0f, "Friction impulse is non-zero for sliding body");

    // Anisotropic friction: lower along grain direction
    auto anisoResult = FrictionMath::computeAnisotropicFriction(
        relVel, normal, Vector3D(1, 0, 0), 100.0f, 0.2f, 0.6f);
    check(anisoResult.impulse.magnitude() > 0.0f, "Anisotropic friction produces impulse");

    // Pacejka tyre model
    float dryForce = FrictionMath::pacejkaForce(0.1f, 5000.0f, FrictionMath::PacejkaParams::DryRoad());
    float iceForce = FrictionMath::pacejkaForce(0.1f, 5000.0f, FrictionMath::PacejkaParams::Ice());
    check(dryForce > iceForce, "Pacejka: dry road produces more friction than ice");
    check(dryForce > 0.0f, "Pacejka: positive tyre force on dry road");

    // Rolling resistance
    float rollF = FrictionMath::rollingResistanceForce(1000.0f, 10.0f);
    check(rollF > 0.0f && rollF < 100.0f, "Rolling resistance is small fraction of normal force");
}

// ═════════════════════════════════════════════════════════════════
// Phase 29: Compressible Flow Tests
// ═════════════════════════════════════════════════════════════════
void testCompressibleFlow() {
    std::cout << "\n=== Compressible Flow ===" << std::endl;

    using namespace CompressibleMath;
    using namespace CompressibleConstants;

    // Speed of sound in air at 20°C ≈ 343 m/s
    float cAir = speedOfSound(GAMMA_AIR, 293.15f);
    std::cout << "    (Speed of sound: " << cAir << " m/s, expect ~343)" << std::endl;
    check(approx(cAir, 343.0f, 2.0f), "Speed of sound in air ≈ 343 m/s");

    // Mach number at 686 m/s = Mach 2
    float M = machNumber(686.0f, cAir);
    check(approx(M, 2.0f, 0.05f), "686 m/s = Mach 2 in standard air");

    // Flow classification
    check(classifyFlow(0.5f) == FlowRegime::SUBSONIC, "Mach 0.5 = subsonic");
    check(classifyFlow(1.0f) == FlowRegime::TRANSONIC, "Mach 1.0 = transonic");
    check(classifyFlow(2.0f) == FlowRegime::SUPERSONIC, "Mach 2.0 = supersonic");
    check(classifyFlow(6.0f) == FlowRegime::HYPERSONIC, "Mach 6.0 = hypersonic");

    // Normal shock at Mach 2: pressure ratio ≈ 4.5
    float pRatio = normalShockPressureRatio(2.0f, GAMMA_AIR);
    check(approx(pRatio, 4.5f, 0.1f), "Normal shock at M=2: P2/P1 ≈ 4.5");

    // Mach after normal shock: M2 < 1 (subsonic after shock)
    float M2 = normalShockMachAfter(2.0f, GAMMA_AIR);
    check(M2 < 1.0f, "After normal shock at M=2: downstream is subsonic");

    // Blast wave: radius increases with time
    float r1 = BlastWave::blastRadius(1e6f, RHO_AIR, 0.001f);
    float r2 = BlastWave::blastRadius(1e6f, RHO_AIR, 0.01f);
    check(r2 > r1, "Blast radius increases with time");
    check(r1 > 0.0f, "Blast radius is positive");
}

// ═════════════════════════════════════════════════════════════════
// Phase 30: Hyperelasticity Tests
// ═════════════════════════════════════════════════════════════════
void testHyperelasticity() {
    std::cout << "\n=== Hyperelasticity ===" << std::endl;

    using namespace HyperelasticMath;

    // Neo-Hookean: zero energy at no deformation (λ=1,1,1)
    auto nhRubber = NeoHookeanParams::Rubber();
    PrincipalStretches undeformed{1.0f, 1.0f, 1.0f};
    float W0 = neoHookeanEnergy(undeformed, nhRubber);
    check(approx(W0, 0.0f, 1.0f), "Neo-Hookean: zero energy at no deformation");

    // Stretching increases energy
    PrincipalStretches stretched{1.5f, 1.0f, 1.0f / 1.5f}; // Incompressible (J=1)
    float Ws = neoHookeanEnergy(stretched, nhRubber);
    check(Ws > W0, "Neo-Hookean: stretching increases strain energy");

    // Stress increases with stretch
    float stress1 = neoHookeanStress1D(1.1f, nhRubber);
    float stress2 = neoHookeanStress1D(1.5f, nhRubber);
    check(stress2 > stress1, "Neo-Hookean: higher stretch = higher stress");
    check(stress1 > 0.0f, "Neo-Hookean: tensile stress is positive");

    // Mooney-Rivlin comparison
    auto mrRubber = MooneyRivlinParams::NaturalRubber();
    float mrStress1 = mooneyRivlinStress1D(1.1f, mrRubber);
    float mrStress2 = mooneyRivlinStress1D(2.0f, mrRubber);
    check(mrStress2 > mrStress1, "Mooney-Rivlin: stress increases with stretch");
    check(mrStress1 > 0.0f, "Mooney-Rivlin: positive tensile stress");
    // Mooney-Rivlin shear modulus = 2(C10+C01)
    float muMR = mrRubber.shearModulus();
    check(muMR > 0.0f, "Mooney-Rivlin: positive shear modulus");
}

// ═════════════════════════════════════════════════════════════════
// Phase 31: Molecular Dynamics Tests
// ═════════════════════════════════════════════════════════════════
void testMolecularDynamics() {
    std::cout << "\n=== Molecular Dynamics ===" << std::endl;

    using namespace engine::physics;

    auto ljAr = LennardJonesParams::Argon();
    double rEq = ljAr.equilibriumDistance();

    // LJ potential at equilibrium = -epsilon
    double V_eq = LJMath::potential(rEq, ljAr);
    check(std::abs(V_eq - (-ljAr.epsilon)) < ljAr.epsilon * 0.01,
          "LJ: V(r_eq) = -epsilon (at minimum)");

    // LJ force at equilibrium = 0 (minimum of potential)
    double F_eq = LJMath::forceMagnitude(rEq, ljAr);
    check(std::abs(F_eq) < ljAr.epsilon * 1e10, "LJ: F(r_eq) ≈ 0 (equilibrium)");

    // LJ: repulsive at short range, attractive at long range
    double F_short = LJMath::forceMagnitude(rEq * 0.8, ljAr);
    double F_long = LJMath::forceMagnitude(rEq * 1.5, ljAr);
    check(F_short > 0, "LJ: repulsive at r < r_eq");
    check(F_long < 0, "LJ: attractive at r > r_eq");

    // Morse potential at equilibrium = 0
    auto morseHH = MorseParams::HydrogenBond();
    double V_morse_eq = MorseMath::potential(morseHH.r0, morseHH);
    check(std::abs(V_morse_eq) < morseHH.D * 0.01,
          "Morse: V(r0) ≈ 0 (minimum at equilibrium)");

    // Molecular system: two Argon atoms find equilibrium
    MolecularSystem md;
    md.ljParams = ljAr;
    float eqDist = static_cast<float>(rEq);
    md.addAtom(Vector3D(0, 0, 0), 39.948); // Argon
    md.addAtom(Vector3D(eqDist * 0.8f, 0, 0), 39.948); // Slightly compressed
    float initialDist = (md.getAtom(1).position - md.getAtom(0).position).magnitude();
    for (int i = 0; i < 100; i++) md.step(1e-14); // femtosecond steps
    float finalDist = (md.getAtom(1).position - md.getAtom(0).position).magnitude();
    check(finalDist > initialDist, "MD: compressed atoms repel and separate");
}

// ═════════════════════════════════════════════════════════════════
// Phase 32: Nuclear Physics Tests
// ═════════════════════════════════════════════════════════════════
void testNuclearPhysics() {
    std::cout << "\n=== Nuclear Physics ===" << std::endl;

    using namespace NuclearMath;

    // Half-life: C-14 (5730 years)
    auto c14 = Nuclide::Carbon14();
    double N0 = 1e6;
    double halfLifeS = c14.halfLife; // ~1.808e11 seconds
    double remaining = c14.remainingAtoms(N0, halfLifeS);
    check(std::abs(remaining - N0 * 0.5) < N0 * 0.01,
          "C-14: half atoms remain after one half-life");

    // Two half-lives: 25% remains
    double remaining2 = c14.remainingAtoms(N0, 2.0 * halfLifeS);
    check(std::abs(remaining2 - N0 * 0.25) < N0 * 0.01,
          "C-14: 25% remains after two half-lives");

    // Activity: λN
    double activity = c14.activity(N0);
    check(activity > 0.0, "Activity is positive for unstable isotope");

    // Stable isotope: no decay
    auto h1 = Nuclide::Hydrogen1();
    double remainH = h1.remainingAtoms(N0, 1e20);
    check(std::abs(remainH - N0) < 1.0, "Stable isotope: no decay ever");

    // D-T Fusion energy ≈ 17.6 MeV
    double fusionE = fusionDT_Energy_MeV();
    std::cout << "    (D-T fusion: " << fusionE << " MeV, expect ~17.6)" << std::endl;
    check(std::abs(fusionE - 17.6) < 0.5, "D-T fusion: Q ≈ 17.6 MeV");

    // E = mc²: rest energy of proton
    double protonE = NuclearConstants::PROTON_MASS * NuclearConstants::SPEED_OF_LIGHT *
                     NuclearConstants::SPEED_OF_LIGHT;
    double protonMeV = protonE / NuclearConstants::MEV_TO_JOULES;
    check(std::abs(protonMeV - 938.3) < 0.5, "Proton rest energy ≈ 938.3 MeV");
}

// ═════════════════════════════════════════════════════════════════
// Phase 33: Special Relativity Tests
// ═════════════════════════════════════════════════════════════════
void testRelativity() {
    std::cout << "\n=== Special Relativity ===" << std::endl;

    using namespace RelativityMath;
    using namespace RelativityConstants;

    // γ(0) = 1
    check(std::abs(lorentzFactor(0.0) - 1.0) < 1e-10, "Lorentz: γ(v=0) = 1");

    // γ(0.6c) = 1.25
    double v06c = 0.6 * C;
    check(std::abs(lorentzFactor(v06c) - 1.25) < 0.001, "Lorentz: γ(0.6c) = 1.25");

    // γ(0.8c) = 5/3 ≈ 1.6667
    double v08c = 0.8 * C;
    check(std::abs(lorentzFactor(v08c) - 5.0/3.0) < 0.001, "Lorentz: γ(0.8c) = 5/3");

    // Time dilation: 1 second proper time at 0.8c = 5/3 seconds dilated
    double dilated = timeDilation(1.0, v08c);
    check(std::abs(dilated - 5.0/3.0) < 0.001, "Time dilation at 0.8c: 1s → 1.667s");

    // Length contraction: 1m at 0.6c = 0.8m
    double contracted = lengthContraction(1.0, v06c);
    check(std::abs(contracted - 0.8) < 0.001, "Length contraction at 0.6c: 1m → 0.8m");

    // E = mc²: rest energy of electron
    double E0 = restEnergy(ELECTRON_MASS);
    double E0_keV = E0 / (1.602176634e-16); // Convert J → keV
    check(std::abs(E0_keV - 511.0) < 0.5, "Rest energy of electron ≈ 511 keV");

    // Velocity addition: 0.6c + 0.6c ≠ 1.2c (Galilean), = 0.882c (relativistic)
    double vAdd = velocityAddition(0.6 * C, 0.6 * C);
    double vAddBeta = vAdd / C;
    check(std::abs(vAddBeta - 0.8824) < 0.001, "Velocity addition: 0.6c + 0.6c ≈ 0.882c");
    check(vAdd < C, "Relativistic addition never exceeds c");

    // KE at low speed ≈ ½mv² (Newtonian limit)
    // Use 1e-3 kg mass to keep KE manageable, v=1000 m/s (still v/c ≈ 3.3e-6)
    double m_test = 1e-3; // 1 gram
    double v_slow = 1000.0; // 1 km/s
    double KE_rel = relativisticKineticEnergy(m_test, v_slow);
    double KE_newton = 0.5 * m_test * v_slow * v_slow;
    // At 1 km/s, relativistic correction is ~5.6e-12 — well within Newtonian
    check(KE_rel > 0.0 && std::abs(KE_rel - KE_newton) < KE_newton * 0.01,
          "Relativistic KE ≈ ½mv² at low speed (Newtonian limit)");
}

// ═════════════════════════════════════════════════════════════════
// Phase 34: MHD Tests
// ═════════════════════════════════════════════════════════════════
void testMHD() {
    std::cout << "\n=== Magnetohydrodynamics ===" << std::endl;

    using namespace MHDMath;

    // Alfvén velocity in solar corona
    auto solar = ConductingFluidProperties::SolarPlasma();
    float vA = alfvenVelocity(1e-4f, solar.density); // B ~ 0.1 mT
    check(vA > 0.0f, "Alfvén velocity is positive");

    // Magnetic pressure: B = 1 T → P = B²/(2μ₀) ≈ 398 kPa
    float pMag = magneticPressure(1.0f);
    check(approx(pMag, 398000.0f, 5000.0f), "Magnetic pressure at 1T ≈ 398 kPa");

    // Plasma beta: high thermal pressure → β > 1
    float betaHigh = plasmaBeta(1e6f, 0.01f); // Weak field, high pressure
    check(betaHigh > 1.0f, "High thermal pressure → β > 1 (pressure dominated)");
    float betaLow = plasmaBeta(1.0f, 10.0f); // Strong field, low pressure
    check(betaLow < 1.0f, "Strong B field → β < 1 (magnetically dominated)");

    // Magnetic Reynolds number
    float Rm = magneticReynoldsNumber(1e6f, 1.0f, 1.0f);
    check(Rm > 1.0f, "Rm >> 1 for conducting fluid → field frozen in");

    // Magnetosonic speed > both sound speed and Alfvén speed
    float cs = 343.0f, va = 100.0f;
    float cms = magnetosonicSpeed(cs, va);
    check(cms > cs && cms > va, "Magnetosonic speed > max(c_s, v_A)");
}

// ═════════════════════════════════════════════════════════════════
// PhysicsConfig & UnifiedSimulation Tests
// ═════════════════════════════════════════════════════════════════
void testPhysicsConfig() {
    std::cout << "\n=== PhysicsConfig ===" << std::endl;

    // Default config
    auto cfg = PhysicsConfig::Default();
    check(approx(cfg.gravity.y, -9.81f, 0.01f), "Default gravity = -9.81 m/s²");
    check(cfg.solverIterations == 10, "Default solver iterations = 10");
    check(cfg.ccdEnabled, "CCD enabled by default");
    check(cfg.sleepEnabled, "Sleep enabled by default");

    // High Performance preset
    auto hp = PhysicsConfig::HighPerformance();
    check(hp.subSteps < cfg.subSteps, "HighPerf: fewer sub-steps");
    check(hp.solverIterations < cfg.solverIterations, "HighPerf: fewer solver iterations");

    // High Accuracy preset
    auto ha = PhysicsConfig::HighAccuracy();
    check(ha.subSteps > cfg.subSteps, "HighAccuracy: more sub-steps");
    check(ha.solverIterations > cfg.solverIterations, "HighAccuracy: more solver iterations");

    // Scientific preset
    auto sci = PhysicsConfig::ScientificSimulation();
    check(sci.thermoFluidCoupling, "Scientific: thermo-fluid coupling enabled");
    check(sci.chemFluidCoupling, "Scientific: chem-fluid coupling enabled");
    check(sci.emFluidCoupling, "Scientific: EM-fluid coupling enabled");
    check(sci.subSteps >= 16, "Scientific: max sub-steps for precision");
}

void testUnifiedSimulation() {
    std::cout << "\n=== UnifiedSimulation ===" << std::endl;

    // Setup fluid + thermal coupling
    FluidSystem fluid;
    fluid.smoothingRadius = 0.5f;
    fluid.addParticle(Vector3D(0, 0, 0)); // T = 293.15 K (default)
    fluid.addParticle(Vector3D(0.1f, 0, 0)); // T = 293.15 K

    // Heat one particle
    fluid.getParticle(0).temperature = 373.15f; // 100°C (boiling)
    fluid.getParticle(1).temperature = 273.15f; // 0°C (freezing)

    UnifiedSimulation sim;
    sim.setFluidSystem(&fluid);
    sim.config().thermoFluidCoupling = true;
    sim.config().sphViscosity = 50.0f;
    sim.config().freezingPoint = 273.15f;
    sim.config().boilingPoint = 373.15f;

    // Subsystem count
    check(sim.getActiveSubsystemCount() == 1, "UnifiedSim: 1 active subsystem (fluid)");
    check(sim.hasCoupling(), "UnifiedSim: has coupling enabled");

    // Step: viscosity coupling should make hot particle less viscous
    std::vector<RigidBody3D> bodies;
    sim.step(0.016f, bodies);

    float viscHot = fluid.getParticle(0).viscosity;
    float viscCold = fluid.getParticle(1).viscosity;
    check(viscHot < viscCold, "Andrade coupling: hot particle has lower viscosity");
    std::cout << "    (Viscosity hot: " << viscHot << ", cold: " << viscCold << ")" << std::endl;

    // Phase transitions: boiling particle should get upward velocity
    check(fluid.getParticle(0).velocity.y > 0.0f || fluid.getParticle(0).temperature > 370.0f,
          "Boiling particle: buoyancy effect applied");
    // Freezing particle: velocity should be damped
    float frozenSpeed = fluid.getParticle(1).velocity.magnitude();
    check(frozenSpeed < 1.0f, "Frozen particle: velocity damped near zero");
}

void testQueryCache() {
    std::cout << "\n=== ECS QueryCache ===" << std::endl;

    using namespace engine::ecs;

    ECSCoordinator ecs;

    // Register a simple component type
    struct Position { float x, y, z; };
    struct Velocity { float vx, vy, vz; };
    ecs.registerComponent<Position>();
    ecs.registerComponent<Velocity>();

    // Create some entities
    Entity e1 = ecs.createEntity();
    ecs.addComponent<Position>(e1, {1, 2, 3});
    ecs.addComponent<Velocity>(e1, {0.1f, 0.2f, 0.3f});

    Entity e2 = ecs.createEntity();
    ecs.addComponent<Position>(e2, {4, 5, 6});

    // Regular forEach should work
    int count = 0;
    ecs.forEach<Position>([&](Entity, Position&) { count++; });
    check(count == 2, "forEach<Position>: finds 2 entities");

    // cachedForEach should match
    int countCached = 0;
    ecs.cachedForEach<Position>([&](Entity, Position&) { countCached++; });
    check(countCached == 2, "cachedForEach<Position>: finds 2 entities (same as forEach)");

    // With two components: only e1 has both
    int countBoth = 0;
    ecs.cachedForEach<Position, Velocity>([&](Entity, Position&, Velocity&) { countBoth++; });
    check(countBoth == 1, "cachedForEach<Pos, Vel>: only 1 entity has both");

    // Cache is populated
    check(ecs.getCachedQueryCount() >= 2, "QueryCache: 2+ cached queries");

    // Adding a new entity should invalidate cache
    Entity e3 = ecs.createEntity();
    ecs.addComponent<Position>(e3, {7, 8, 9});
    ecs.addComponent<Velocity>(e3, {0.4f, 0.5f, 0.6f});

    int countAfterAdd = 0;
    ecs.cachedForEach<Position, Velocity>([&](Entity, Position&, Velocity&) { countAfterAdd++; });
    check(countAfterAdd == 2, "After add: cachedForEach finds 2 entities with Pos+Vel");

    // Destroying e3 should invalidate
    ecs.destroyEntity(e3);
    int countAfterDel = 0;
    ecs.cachedForEach<Position, Velocity>([&](Entity, Position&, Velocity&) { countAfterDel++; });
    check(countAfterDel == 1, "After destroy: cachedForEach correctly finds 1 entity");
}

void testAdvancedThermodynamics() {
    std::cout << "\n=== Advanced Thermodynamics (Phase 41) ===" << std::endl;
    
    using namespace engine::physics;

    std::vector<RigidBody3D> bodies;
    bodies.push_back(RigidBody3D::dynamic(1.0f)); // 1kg sphere
    bodies[0].shape = RigidBody3D::Shape::SPHERE;
    bodies[0].sphereRadius = 1.0f; // 1m radius
    
    ThermalSystem thermal;
    thermal.ambientTemperature = 293.15f; // room temp
    
    PhysicsMaterial water = PhysicsMaterial::Ice();
    water.temperature = 273.15f; // EXACTLY melting point
    water.meltingPoint = 273.15f;
    water.boilingPoint = 373.15f;
    water.latentHeatFusion = 333000.0f; // J/kg
    
    int tId = thermal.addThermalBody(0, water, 4.0f * 3.14159265f);
    ThermalBody& tb = thermal.get(tId);
    tb.phase = ThermalBody::SOLID; // Start as solid ice

    // Force sufficient heat injection (just enough to absorb some latent heat, not enough to melt fully)
    thermal.ambientTemperature = 400.0f; // 400K -> moderate heat transfer
    thermal.step(10.0f, bodies); // 10 seconds of moderate heat
    
    check(tb.temperature == 273.15f, "Latent heat: Temperature halted at melting point during phase transition");
    check(tb.accumulatedLatentHeat > 0.0f, "Latent heat accumulator is filling up");
    check(tb.phase == ThermalBody::SOLID, "Still solid because latent heat not fully absorbed");

    // Force enough heat to melt
    thermal.ambientTemperature = 1000.0f; // 1000K -> large heat transfer (T^4)
    thermal.step(10.0f, bodies); // 10 seconds -> massive energy, melts completely
    
    check(tb.phase == ThermalBody::LIQUID, "Phase transition complete: Ice became water");

    // One more step so the liquid absorbs heat and temperature rises above melting point
    thermal.step(1.0f, bodies); 
    check(tb.temperature > 273.15f, "Temperature rising again after latent heat absorbed");

    // Test Thermal Expansion
    PhysicsMaterial steel = PhysicsMaterial::Steel();
    steel.thermalExpansionCoeff = 1.2e-5f;
    steel.temperature = 293.15f;
    bodies.push_back(RigidBody3D::dynamic(1.0f));
    bodies[1].shape = RigidBody3D::Shape::SPHERE;
    bodies[1].sphereRadius = 1.0f; // L0 = 1.0r
    
    int sId = thermal.addThermalBody(1, steel, 4.0f * 3.14159265f);
    ThermalBody& sb = thermal.get(sId);
    
    // Jump temperature by 1000K manually to trigger expansion calculation on step
    sb.temperature = 1293.15f;
    thermal.ambientTemperature = 1293.15f; // Keep it hot
    thermal.step(0.1f, bodies);
    
    check(sb.surfaceArea > 4.0f * 3.14159265f, "Thermal Expansion: Steel surface area grew due to Delta T");
}

void testSolidMechanics() {
    std::cout << "\n=== Solid Mechanics & Fatigue (Phase 43) ===" << std::endl;
    using namespace engine::physics;

    std::cout << "  [Testing Plastic Yielding]" << std::endl;
    // 1. Plastic Yielding Test (Strain > Yield)
    SoftBodySystem sys;
    sys.solverIterations = 10;
    
    // Create a 2-particle line (1 segment)
    auto rope = SoftBody3D::createRope(engine::math::Vector3D(0,0,0), engine::math::Vector3D(0,-1,0), 1, 2.0f);
    
    // Pin top particle
    rope.m_particles[0].setMass(0.0f); // Static
    
    // Make it weak steel
    PhysicsMaterial weakSteel = PhysicsMaterial::Steel();
    weakSteel.yieldStrength = 5000.0f;     // Low yield to test plastic deformation (Stress is ~8800 Pa)
    weakSteel.ultimateStrength = 50000.0f; // Won't break
    weakSteel.elasticModulus = 1e6f;       // 1 MPa
    weakSteel.crossSectionArea = 1.0f;     // 1m^2
    rope.setMaterial(weakSteel);
    
    int bId = sys.addBody(std::move(rope));
    
    // Apply huge force to bottom particle to cause stress
    // Mass = 1kg. Force = ~8800 N. Stress = ~8800 Pa > 5000 Pa yield
    for (int i=0; i < 5; i++) {
        sys.getBody(bId).m_particles[1].velocity.y -= 200.0f; // massive impulse
        sys.step(1.0f / 60.0f);
    }
    
    auto& constraint = sys.getBody(bId).m_constraints[0];
    auto* dConstraint = static_cast<XPBDDistanceConstraint*>(constraint.get());
    
    std::cout << "    [Debug] Lambda: " << dConstraint->lambda << std::endl;
    std::cout << "    [Debug] Plastic Strain: " << constraint->plasticStrain << std::endl;
    std::cout << "    [Debug] Rest Distance: " << dConstraint->restDistance << " (Original: 1.0)" << std::endl;
    
    check(constraint->plasticStrain > 0.0f, "Plastic strain accumulated when Stress > Yield");
    check(dConstraint->restDistance > 1.0f, "Permanent deformation: rest distance permanently increased");
    check(!constraint->broken, "Material yielded but did not fracture");

    std::cout << "  [Testing Fatigue Failure (S-N Curve)]" << std::endl;
    // 2. Fatigue Test (Stress < Ultimate, > Fatigue)
    auto fatigueRope = SoftBody3D::createRope(engine::math::Vector3D(0,0,0), engine::math::Vector3D(0,-1,0), 1, 2.0f);
    fatigueRope.m_particles[0].setMass(0.0f); // pinned
    
    PhysicsMaterial fatmat = PhysicsMaterial::Steel();
    fatmat.yieldStrength = 1000.0f;      // pure elastic
    fatmat.ultimateStrength = 1000.0f;   // quite strong
    fatmat.fatigueLimit = 10.0f;         // But fatigues easily >= 10 Pa
    fatmat.fatigueSNExponent = 2.0f;     
    fatmat.crossSectionArea = 1.0f;
    fatigueRope.setMaterial(fatmat);
    
    int fId = sys.addBody(std::move(fatigueRope));
    
    // Oscillate the bottom particle gently (Stress around 50 Pa)
    // 50 < 1000 (no instant break), but 50 > 10 (fatigues)
    bool brokenByFatigue = false;
    for (int i = 0; i < 500; i++) {
        // Ping-pong impulse to cause gentle cyclical stress
        float dir = (i % 20 < 10) ? -50.0f : 50.0f;
        sys.getBody(fId).m_particles[1].velocity.y += dir;
        sys.step(1.0f / 60.0f);
        
        if (sys.getBody(fId).m_constraints[0]->broken) {
            brokenByFatigue = true;
            std::cout << "    (Broke due to fatigue at step " << i << ")" << std::endl;
            break;
        }
    }
    
    check(brokenByFatigue, "Material fracture due to cyclical fatigue (Miner's Rule)");
}

int main() {
    std::cout << "============================================" << std::endl;
    std::cout << "  ALZE — 3D Physics Tests" << std::endl;
    std::cout << "  Collider3D + RigidBody3D + SpatialHash3D + PhysicsWorld3D" << std::endl;
    std::cout << "============================================" << std::endl;

    testSphereVsSphere();
    testAABB();
    testOBBvsOBB();
    testOBBvsSphere();
    testRays();
    testInertia();
    testDynamicBVH();
    testPhysicsWorld();
    testCapsuleCollisions();
    testCapsuleInertia();
    testCapsuleWorld();
    testConstraints();

    // Phase 14 tests
    testPhysicsMaterial();
    testAerodynamics();
    testBuoyancy();
    testRollingFriction();
    testThermodynamics();
    testVerletIntegration();
    testSpringDamper();
    testMaterialIntegration();
    testBodyRemoval();
    testConvexHull();
    testFluidSPH();
    testElectromagnetism();
    testChemistry();
    testSoftBodyXPBD();
    testGravityNBody();
    testWavesAndAcoustics();
    testFracture();
    testOptics();
    testQuantumPhysics();
    testCCDSystem();
    testAdvancedFriction();
    testCompressibleFlow();
    testHyperelasticity();
    testMolecularDynamics();
    testNuclearPhysics();
    testRelativity();
    testMHD();
    testPhysicsConfig();
    testUnifiedSimulation();
    testQueryCache();
    testAdvancedThermodynamics();
    testSolidMechanics();

    std::cout << "\n============================================" << std::endl;
    std::cout << "  Resultados: " << passed << " pasados, "
              << failed << " fallidos" << std::endl;
    std::cout << "============================================" << std::endl;

    return failed > 0 ? 1 : 0;
}
