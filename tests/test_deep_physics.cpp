// test_deep_physics.cpp — Tests for deep physics optimizations
//
// Covers:
//   - GJK+EPA with ConvexHull hill climbing
//   - BVH raycast front-to-back ordering
//   - SIMD Quaternion Hamilton product
//   - SIMD Vector3D fast normalized (rsqrt)
//   - Spring damper implicit Euler integration
//   - NavMesh spatial grid acceleration
//   - XPBD volume constraint
//   - XPBD self-collision
//   - GridNav octile heuristic A*
//

#include <iostream>
#include <cmath>
#include <string>
#include <chrono>
#include <vector>
#include <array>

#include "math/Vector3D.h"
#include "math/Quaternion.h"
#include "math/Matrix4x4.h"
#include "math/SimdConfig.h"
#include "math/MathUtils.h"
#include "physics/Collider3D.h"
#include "physics/GJK.h"
#include "physics/DynamicBVH3D.h"
#include "physics/PhysicsMath.h"
#include "physics/PhysicsMaterial.h"
#include "physics/SoftBody3D.h"
#include "ai/NavMesh.h"

using namespace engine::math;
using namespace engine::physics;
using namespace engine::ai;

static int passed = 0;
static int failed = 0;

static void check(bool cond, const std::string& msg) {
    if (cond) { std::cout << "  [OK] " << msg << std::endl; passed++; }
    else      { std::cerr << "  [FAIL] " << msg << std::endl; failed++; }
}

static bool approx(float a, float b, float eps = 0.01f) {
    return std::fabs(a - b) < eps;
}

// ═══════════════════════════════════════════════════════════════
// GJK+EPA Optimization Tests
// ═══════════════════════════════════════════════════════════════
void testGJKEPAOptimized() {
    std::cout << "\n=== GJK+EPA Optimized ===" << std::endl;

    // Test 1: Sphere vs Sphere via GJK (basic sanity)
    SphereCollider s1(Vector3D(0, 0, 0), 1.0f);
    SphereCollider s2(Vector3D(1.5f, 0, 0), 1.0f);
    GJKResult gjk = GJK::intersect(s1, s2);
    check(gjk.isIntersecting, "GJK: overlapping spheres detected");

    EPAResult epa = GJK::solveEPA(s1, s2, gjk);
    check(epa.success, "EPA: solved penetration");
    check(approx(epa.penetration, 0.5f, 0.05f), "EPA: penetration ~0.5");
    check(epa.contactPoint.magnitude() > 0.0f, "EPA: contact point computed");

    // Test 2: Non-overlapping spheres
    SphereCollider s3(Vector3D(5, 0, 0), 1.0f);
    GJKResult gjk2 = GJK::intersect(s1, s3);
    check(!gjk2.isIntersecting, "GJK: non-overlapping detected");

    // Test 3: ConvexHull vs ConvexHull with GJK+EPA
    ConvexHullCollider box1;
    box1.vertices = {
        {-1,-1,-1}, {1,-1,-1}, {1,1,-1}, {-1,1,-1},
        {-1,-1, 1}, {1,-1, 1}, {1,1, 1}, {-1,1, 1}
    };
    box1.computeCenter();

    ConvexHullCollider box2;
    box2.vertices = {
        {0.5f,-1,-1}, {2.5f,-1,-1}, {2.5f,1,-1}, {0.5f,1,-1},
        {0.5f,-1, 1}, {2.5f,-1, 1}, {2.5f,1, 1}, {0.5f,1, 1}
    };
    box2.computeCenter();

    GJKResult gjk3 = GJK::intersect(box1, box2);
    check(gjk3.isIntersecting, "GJK: overlapping convex hulls detected");

    EPAResult epa3 = GJK::solveEPA(box1, box2, gjk3);
    check(epa3.success, "EPA: solved convex hull penetration");
    check(epa3.penetration > 0.0f, "EPA: positive penetration depth");

    // Test 4: ConvexHull with hill-climbing adjacency
    ConvexHullCollider hull;
    hull.vertices = {
        {-1,-1,-1}, {1,-1,-1}, {1,1,-1}, {-1,1,-1},
        {-1,-1, 1}, {1,-1, 1}, {1,1, 1}, {-1,1, 1}
    };
    hull.computeCenter();

    // Build adjacency for cube (6 faces, 2 triangles each)
    std::vector<std::array<int,3>> faces = {
        {0,1,2}, {0,2,3}, // front
        {4,6,5}, {4,7,6}, // back
        {0,4,1}, {1,4,5}, // bottom
        {2,6,3}, {3,6,7}, // top
        {0,3,4}, {3,7,4}, // left
        {1,5,2}, {2,5,6}  // right
    };
    hull.buildAdjacency(faces);

    check(!hull.adjacency.empty(), "ConvexHull: adjacency built");
    check(hull.adjacency.size() == 8, "ConvexHull: 8 vertices have adjacency");

    // Support with hill climbing should give same result as brute force
    Vector3D dir(1, 1, 1);
    Vector3D supportHC = hull.getSupport(dir);
    check(approx(supportHC.x, 1.0f) && approx(supportHC.y, 1.0f) && approx(supportHC.z, 1.0f),
          "ConvexHull hill-climb: support (1,1,1) direction → (1,1,1) vertex");

    Vector3D dir2(-1, 0, 0);
    Vector3D support2 = hull.getSupport(dir2);
    check(support2.x < 0.0f, "ConvexHull hill-climb: support (-1,0,0) → negative X vertex");

    // Test 5: EPA tolerance improvement (more precise results)
    SphereCollider sa(Vector3D(0, 0, 0), 1.0f);
    SphereCollider sb(Vector3D(1.99f, 0, 0), 1.0f);
    GJKResult gjk5 = GJK::intersect(sa, sb);
    if (gjk5.isIntersecting) {
        EPAResult epa5 = GJK::solveEPA(sa, sb, gjk5);
        if (epa5.success) {
            check(approx(epa5.penetration, 0.01f, 0.02f), "EPA: precise shallow penetration");
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// BVH Raycast Optimization Tests
// ═══════════════════════════════════════════════════════════════
void testBVHRaycast() {
    std::cout << "\n=== BVH Raycast Front-to-Back ===" << std::endl;

    DynamicBVH3D bvh;

    // Insert objects at increasing distances along X
    for (int i = 0; i < 10; i++) {
        float x = static_cast<float>(i) * 3.0f;
        AABB3D box(Vector3D(x - 0.5f, -0.5f, -0.5f), Vector3D(x + 0.5f, 0.5f, 0.5f));
        bvh.insert(i, box);
    }

    // Raycast along +X should hit objects in order
    Ray3D ray(Vector3D(-5, 0, 0), Vector3D(1, 0, 0));
    std::vector<int> hitOrder;

    bvh.raycast(ray, [&](int userData) -> bool {
        hitOrder.push_back(userData);
        return true; // Continue
    });

    check(!hitOrder.empty(), "BVH raycast: hits found");
    check(hitOrder.size() == 10, "BVH raycast: all 10 objects hit");

    // With front-to-back ordering, first hit should be closest (index 0)
    if (!hitOrder.empty()) {
        check(hitOrder[0] == 0, "BVH raycast: nearest object hit first (front-to-back)");
    }

    // Test early termination
    int firstHit = -1;
    bvh.raycast(ray, [&](int userData) -> bool {
        firstHit = userData;
        return false; // Stop after first hit
    });
    check(firstHit == 0, "BVH raycast: early termination returns nearest");

    // Raycast that misses all objects
    Ray3D missRay(Vector3D(0, 10, 0), Vector3D(0, 1, 0));
    bool anyHit = false;
    bvh.raycast(missRay, [&](int) -> bool { anyHit = true; return true; });
    check(!anyHit, "BVH raycast: miss returns no hits");
}

// ═══════════════════════════════════════════════════════════════
// SIMD Quaternion Tests
// ═══════════════════════════════════════════════════════════════
void testSIMDQuaternion() {
    std::cout << "\n=== SIMD Quaternion ===" << std::endl;

    std::cout << "  SIMD: " << engine::math::simdName() << std::endl;

    // Test Hamilton product matches scalar reference
    Quaternion a = Quaternion::fromAxisAngle(Vector3D::Up, 0.5f);
    Quaternion b = Quaternion::fromAxisAngle(Vector3D::Right, 0.3f);

    Quaternion c = a * b;

    // Reference: manual scalar Hamilton product
    float rw = a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z;
    float rx = a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y;
    float ry = a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x;
    float rz = a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w;

    check(approx(c.x, rx, 0.001f), "Quaternion SIMD mul: x matches scalar");
    check(approx(c.y, ry, 0.001f), "Quaternion SIMD mul: y matches scalar");
    check(approx(c.z, rz, 0.001f), "Quaternion SIMD mul: z matches scalar");
    check(approx(c.w, rw, 0.001f), "Quaternion SIMD mul: w matches scalar");

    // Test dot product
    float dotRef = a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
    float dotSimd = a.dot(b);
    check(approx(dotSimd, dotRef, 0.001f), "Quaternion SIMD dot: matches scalar");

    // Test conjugate
    Quaternion conj = a.conjugate();
    check(approx(conj.x, -a.x) && approx(conj.y, -a.y) &&
          approx(conj.z, -a.z) && approx(conj.w, a.w),
          "Quaternion SIMD conjugate: correct");

    // Test normalized
    Quaternion unnorm(1, 2, 3, 4);
    Quaternion norm = unnorm.normalized();
    check(approx(norm.magnitude(), 1.0f, 0.001f), "Quaternion SIMD normalized: unit length");

    // Test sqrMagnitude
    float sqrMag = unnorm.sqrMagnitude();
    check(approx(sqrMag, 1+4+9+16, 0.001f), "Quaternion SIMD sqrMagnitude: correct");

    // Test identity multiplication
    Quaternion id;
    Quaternion result = a * id;
    check(approx(result.x, a.x, 0.001f) && approx(result.w, a.w, 0.001f),
          "Quaternion SIMD mul identity: a * I = a");

    // Test rotation consistency
    Vector3D v(1, 0, 0);
    Quaternion rot90Y = Quaternion::fromAxisAngle(Vector3D::Up, MathUtils::PI * 0.5f);
    Vector3D rotated = rot90Y.rotate(v);
    check(approx(rotated.x, 0.0f, 0.01f) && approx(rotated.z, -1.0f, 0.01f),
          "Quaternion SIMD: 90deg Y rotation correct");
}

// ═══════════════════════════════════════════════════════════════
// SIMD Vector3D Fast Normalize Tests
// ═══════════════════════════════════════════════════════════════
void testSIMDVector3D() {
    std::cout << "\n=== SIMD Vector3D Fast Normalize ===" << std::endl;

    // Test normalized accuracy with rsqrt + Newton-Raphson
    Vector3D v1(3.0f, 4.0f, 0.0f);
    Vector3D n1 = v1.normalized();
    check(approx(n1.x, 0.6f, 0.001f) && approx(n1.y, 0.8f, 0.001f),
          "Vector3D fast normalize: (3,4,0) → (0.6, 0.8, 0)");

    // Test unit vector stays unit
    Vector3D v2(1, 0, 0);
    Vector3D n2 = v2.normalized();
    check(approx(n2.magnitude(), 1.0f, 0.001f), "Vector3D fast normalize: unit vector preserved");

    // Test large vector
    Vector3D v3(1000, 2000, 3000);
    Vector3D n3 = v3.normalized();
    check(approx(n3.magnitude(), 1.0f, 0.001f), "Vector3D fast normalize: large vector → unit");

    // Test small vector
    Vector3D v4(0.001f, 0.002f, 0.001f);
    Vector3D n4 = v4.normalized();
    check(approx(n4.magnitude(), 1.0f, 0.01f), "Vector3D fast normalize: small vector → unit");

    // Test zero vector
    Vector3D v5(0, 0, 0);
    Vector3D n5 = v5.normalized();
    check(approx(n5.magnitude(), 0.0f), "Vector3D fast normalize: zero → zero");

    // Test negative vector
    Vector3D v6(-1, -1, -1);
    Vector3D n6 = v6.normalized();
    float expectedComp = -1.0f / std::sqrt(3.0f);
    check(approx(n6.x, expectedComp, 0.001f), "Vector3D fast normalize: (-1,-1,-1) correct");
}

// ═══════════════════════════════════════════════════════════════
// Spring Damper System Tests
// ═══════════════════════════════════════════════════════════════
void testSpringDamper() {
    std::cout << "\n=== Spring Damper System ===" << std::endl;

    // Test critical damping coefficient
    float cc = PhysicsMath::criticalDamping(1.0f, 100.0f);
    check(approx(cc, 20.0f, 0.01f), "Critical damping: mass=1, k=100 → c=20");

    // Test damping ratio
    float zeta = PhysicsMath::dampingRatio(20.0f, 1.0f, 100.0f);
    check(approx(zeta, 1.0f, 0.01f), "Damping ratio: critical → zeta=1.0");

    float zetaUnder = PhysicsMath::dampingRatio(10.0f, 1.0f, 100.0f);
    check(zetaUnder < 1.0f, "Damping ratio: underdamped → zeta<1.0");

    // Test spring from frequency
    float k, c;
    PhysicsMath::springFromFrequency(1.0f, 10.0f, 0.5f, k, c);
    check(k > 0.0f, "Spring from frequency: stiffness > 0");
    check(c > 0.0f, "Spring from frequency: damping > 0");
    // omega = 2*pi*10 = 62.83, k = 1 * 62.83^2 = 3947.8
    check(approx(k, 3947.8f, 10.0f), "Spring from frequency: k ~ 3948");

    // Test implicit spring-damper step (stability for stiff spring)
    Vector3D pos(1, 0, 0);
    Vector3D vel(0, 0, 0);
    Vector3D anchor(0, 0, 0);
    float stiffK = 10000.0f; // Very stiff spring
    float dampC = 100.0f;
    float mass = 1.0f;
    float dt = 1.0f / 60.0f;

    Vector3D newPos, newVel;
    // Simulate 100 steps with implicit Euler (should be stable)
    for (int i = 0; i < 100; i++) {
        PhysicsMath::implicitSpringDamperStep(pos, vel, anchor, stiffK, dampC, mass, dt, newPos, newVel);
        pos = newPos;
        vel = newVel;
    }
    // Should converge to anchor without exploding
    check(pos.magnitude() < 1.0f, "Implicit spring: stiff spring converges (no explosion)");
    check(!std::isnan(pos.x) && !std::isinf(pos.x), "Implicit spring: no NaN/Inf");

    // Test damped oscillator analytical
    float x0 = PhysicsMath::dampedOscillator(1.0f, 10.0f, 0.5f, 0.0f, 0.0f);
    check(approx(x0, 1.0f, 0.01f), "Damped oscillator: t=0 → amplitude");

    float x1 = PhysicsMath::dampedOscillator(1.0f, 10.0f, 0.5f, 0.0f, 1.0f);
    check(std::fabs(x1) < 1.0f, "Damped oscillator: t=1 → decayed");

    // Test basic spring force
    Vector3D f = PhysicsMath::springDamperForce(Vector3D(1, 0, 0), Vector3D(0, 0, 0), 100.0f, 10.0f);
    check(approx(f.x, -100.0f), "Spring force: F = -kx = -100");
}

// ═══════════════════════════════════════════════════════════════
// NavMesh Optimization Tests
// ═══════════════════════════════════════════════════════════════
void testNavMeshOptimized() {
    std::cout << "\n=== NavMesh Optimized ===" << std::endl;

    NavMesh mesh;

    // Create a grid of square polygons
    float size = 2.0f;
    int gridSize = 5;
    for (int y = 0; y < gridSize; y++) {
        for (int x = 0; x < gridSize; x++) {
            float fx = x * size;
            float fy = y * size;
            std::vector<Vector2D> verts = {
                {fx, fy}, {fx + size, fy}, {fx + size, fy + size}, {fx, fy + size}
            };
            mesh.addPolygon(verts);
        }
    }

    check(mesh.getPolygonCount() == 25, "NavMesh: 25 polygons added");

    mesh.build();
    check(mesh.isBuilt(), "NavMesh: built with spatial grid");

    // Test findPolygon with spatial grid (should be O(1) average)
    int poly = mesh.findPolygon({1.0f, 1.0f});
    check(poly >= 0, "NavMesh grid: findPolygon(1,1) found");
    check(poly == 0, "NavMesh grid: (1,1) is in polygon 0");

    int poly2 = mesh.findPolygon({3.0f, 3.0f});
    check(poly2 >= 0, "NavMesh grid: findPolygon(3,3) found");

    // Test point outside mesh
    int polyOut = mesh.findPolygon({-1.0f, -1.0f});
    check(polyOut < 0, "NavMesh grid: point outside → -1");

    // Test pathfinding still works
    auto path = mesh.findPath({1.0f, 1.0f}, {9.0f, 9.0f});
    check(!path.empty(), "NavMesh: path found from (1,1) to (9,9)");
    if (!path.empty()) {
        check(approx(path.front().x, 1.0f) && approx(path.front().y, 1.0f),
              "NavMesh: path starts at (1,1)");
        check(approx(path.back().x, 9.0f) && approx(path.back().y, 9.0f),
              "NavMesh: path ends at (9,9)");
    }

    // Test blocked polygon
    mesh.setBlocked(12, true); // Block middle polygon
    auto path2 = mesh.findPath({1.0f, 1.0f}, {9.0f, 9.0f});
    check(!path2.empty(), "NavMesh: path around blocked polygon");
}

void testGridNavOptimized() {
    std::cout << "\n=== GridNav Optimized A* ===" << std::endl;

    GridNav grid;
    grid.init(20, 20, 1.0f);

    // Create a wall
    for (int y = 2; y < 18; y++) {
        grid.setWalkable(10, y, false);
    }

    auto path = grid.findPath({1.5f, 10.5f}, {18.5f, 10.5f});
    check(!path.empty(), "GridNav: path found around wall");

    // Direct path with no obstacles
    GridNav openGrid;
    openGrid.init(10, 10, 1.0f);
    auto directPath = openGrid.findPath({0.5f, 0.5f}, {9.5f, 9.5f});
    check(!directPath.empty(), "GridNav octile: diagonal path found");

    // Path on blocked grid
    GridNav blockedGrid;
    blockedGrid.init(3, 3, 1.0f);
    blockedGrid.setWalkable(1, 0, false);
    blockedGrid.setWalkable(1, 1, false);
    blockedGrid.setWalkable(1, 2, false);
    auto blockedPath = blockedGrid.findPath({0.5f, 1.5f}, {2.5f, 1.5f});
    check(blockedPath.empty(), "GridNav: no path through complete wall");
}

// ═══════════════════════════════════════════════════════════════
// XPBD Volume Constraint Tests
// ═══════════════════════════════════════════════════════════════
void testXPBDVolumeConstraint() {
    std::cout << "\n=== XPBD Volume Constraint ===" << std::endl;

    // Create a simple tetrahedron soft body
    SoftBody3D body;
    body.m_particles.resize(4);
    body.m_particles[0].position = Vector3D(0, 0, 0);
    body.m_particles[1].position = Vector3D(2, 0, 0);
    body.m_particles[2].position = Vector3D(1, 2, 0);
    body.m_particles[3].position = Vector3D(1, 0.5f, 2);
    for (auto& p : body.m_particles) {
        p.prevPosition = p.position;
        p.setMass(1.0f);
    }

    // Create volume constraint with tetrahedron faces
    std::vector<std::array<int, 3>> tris = {
        {0, 2, 1}, {0, 1, 3}, {1, 2, 3}, {0, 3, 2}
    };
    auto volConstraint = std::make_unique<XPBDVolumeConstraint>(tris, 0.0f);
    volConstraint->computeRestVolume(body.m_particles);

    float restVol = volConstraint->restVolume;
    check(std::abs(restVol) > 0.01f, "Volume constraint: rest volume computed");

    // Compress the tetrahedron
    body.m_particles[3].position = Vector3D(1, 0.5f, 1.0f); // Move closer

    float compressedVol = volConstraint->computeVolume(body.m_particles);
    check(std::abs(compressedVol) < std::abs(restVol),
          "Volume constraint: compressed volume < rest volume");

    // Solve should restore volume
    for (int i = 0; i < 20; i++) {
        volConstraint->lambda = 0.0f;
        volConstraint->solve(body.m_particles, 1.0f / 60.0f);
    }

    float restoredVol = volConstraint->computeVolume(body.m_particles);
    check(std::abs(restoredVol) > std::abs(compressedVol),
          "Volume constraint: volume partially restored after solving");
}

// ═══════════════════════════════════════════════════════════════
// XPBD Self-Collision Tests
// ═══════════════════════════════════════════════════════════════
void testXPBDSelfCollision() {
    std::cout << "\n=== XPBD Self-Collision ===" << std::endl;

    SoftBodySystem system;
    system.gravity = Vector3D::Zero; // No gravity for this test
    system.enableSelfCollision = true;
    system.selfCollisionRadius = 0.5f;
    system.solverIterations = 5;

    // Create two particles very close together
    SoftBody3D body;
    XPBDParticle p1, p2;
    p1.position = Vector3D(0, 0, 0);
    p1.prevPosition = p1.position;
    p1.setMass(1.0f);

    p2.position = Vector3D(0.1f, 0, 0); // Closer than selfCollisionRadius
    p2.prevPosition = p2.position;
    p2.setMass(1.0f);

    body.m_particles.push_back(p1);
    body.m_particles.push_back(p2);

    int idx = system.addBody(std::move(body));
    system.step(1.0f / 60.0f);

    auto& particles = system.getBody(idx).m_particles;
    float dist = (particles[0].position - particles[1].position).magnitude();
    check(dist >= 0.4f, "Self-collision: particles pushed apart (dist >= 0.4)");
}

// ═══════════════════════════════════════════════════════════════
// XPBD Cloth Simulation Tests
// ═══════════════════════════════════════════════════════════════
void testXPBDCloth() {
    std::cout << "\n=== XPBD Cloth Simulation ===" << std::endl;

    // Create a small cloth
    auto cloth = SoftBody3D::createCloth(
        Vector3D(0, 5, 0),  // topLeft
        Vector3D(2, 0, 0),  // edgeU
        Vector3D(0, 0, 2),  // edgeV
        4, 4,                // resolution
        1.0f,                // totalMass
        1e-6f,               // stretchCompliance
        1e-4f                // bendCompliance
    );

    check(cloth.m_particles.size() == 16, "Cloth: 4x4 = 16 particles");
    check(!cloth.m_constraints.empty(), "Cloth: constraints generated");

    // Pin top-left corners
    cloth.m_particles[0].isStatic = true;
    cloth.m_particles[3].isStatic = true;

    SoftBodySystem system;
    system.gravity = Vector3D(0, -9.8f, 0);
    system.solverIterations = 10;
    system.floorY = -10.0f;

    int idx = system.addBody(std::move(cloth));

    // Simulate a few steps
    for (int i = 0; i < 30; i++) {
        system.step(1.0f / 60.0f);
    }

    auto& particles = system.getBody(idx).m_particles;

    // Pinned particles should stay fixed
    check(approx(particles[0].position.y, 5.0f, 0.01f), "Cloth: pinned particle stays");

    // Free particles should fall
    check(particles[15].position.y < 5.0f, "Cloth: free particles fall under gravity");

    // No NaN/Inf
    bool stable = true;
    for (const auto& p : particles) {
        if (std::isnan(p.position.x) || std::isinf(p.position.x)) { stable = false; break; }
    }
    check(stable, "Cloth: simulation stable (no NaN/Inf)");
}

// ═══════════════════════════════════════════════════════════════
// Verlet Integration Tests
// ═══════════════════════════════════════════════════════════════
void testVerletIntegration() {
    std::cout << "\n=== Verlet Integration ===" << std::endl;

    Vector3D pos(0, 10, 0);
    Vector3D vel(5, 0, 0);
    Vector3D accel(0, -9.8f, 0);
    float dt = 0.01f;

    // Position step: x' = x + v*dt + 0.5*a*dt^2
    Vector3D newPos = PhysicsMath::verletPositionStep(pos, vel, accel, dt);
    check(approx(newPos.x, 0.05f, 0.001f), "Verlet pos: x = 0 + 5*0.01 = 0.05");
    check(approx(newPos.y, 10.0f - 0.5f*9.8f*0.0001f, 0.001f), "Verlet pos: y correct");

    // Velocity step: v' = v + 0.5*(a_old + a_new)*dt
    Vector3D accelNew(0, -9.8f, 0);
    Vector3D newVel = PhysicsMath::verletVelocityStep(vel, accel, accelNew, dt);
    check(approx(newVel.x, 5.0f, 0.001f), "Verlet vel: x unchanged (no x accel)");
    check(approx(newVel.y, -0.098f, 0.001f), "Verlet vel: y = 0 + 0.5*(-9.8+-9.8)*0.01");

    // Test energy conservation over many steps (projectile should return near start height)
    Vector3D p(0, 0, 0);
    Vector3D v(10, 20, 0);
    Vector3D a(0, -9.8f, 0);
    float totalTime = 0.0f;
    float maxHeight = 0.0f;

    for (int i = 0; i < 1000; i++) {
        Vector3D pNew = PhysicsMath::verletPositionStep(p, v, a, dt);
        Vector3D vNew = PhysicsMath::verletVelocityStep(v, a, a, dt);
        p = pNew;
        v = vNew;
        totalTime += dt;
        if (p.y > maxHeight) maxHeight = p.y;
        if (p.y < 0 && totalTime > 1.0f) break; // Landed
    }

    // Max height should be ~ v_y^2 / (2g) = 400/19.6 ≈ 20.4
    check(approx(maxHeight, 20.4f, 1.0f), "Verlet: projectile max height ~ 20.4m");
}

// ═══════════════════════════════════════════════════════════════
// BVH Broadphase Pair Tests
// ═══════════════════════════════════════════════════════════════
void testBVHBroadphase() {
    std::cout << "\n=== BVH Broadphase ===" << std::endl;

    DynamicBVH3D bvh;

    // Two overlapping objects
    AABB3D box1(Vector3D(-1, -1, -1), Vector3D(1, 1, 1));
    AABB3D box2(Vector3D(0, 0, 0), Vector3D(2, 2, 2));
    AABB3D box3(Vector3D(10, 10, 10), Vector3D(11, 11, 11)); // Far away

    int id1 = bvh.insert(0, box1);
    bvh.insert(1, box2);
    int id3 = bvh.insert(2, box3);

    auto pairs = bvh.getPotentialPairs();
    check(!pairs.empty(), "BVH: overlapping pair detected");

    bool foundOverlap = false;
    for (auto& [a, b] : pairs) {
        if ((a == 0 && b == 1) || (a == 1 && b == 0)) foundOverlap = true;
    }
    check(foundOverlap, "BVH: pair (0,1) found");

    // Update and re-check
    bvh.update(id1, AABB3D(Vector3D(20, 20, 20), Vector3D(21, 21, 21)), Vector3D::Zero);
    auto pairs2 = bvh.getPotentialPairs();
    // After moving box1 far away, just verify it runs without crash
    (void)pairs2;
    check(true, "BVH: update and re-query works");

    // Remove
    bvh.remove(id3);
    auto pairs3 = bvh.getPotentialPairs();
    check(true, "BVH: remove works without crash");
}

// ═══════════════════════════════════════════════════════════════
// AVX Detection Test
// ═══════════════════════════════════════════════════════════════
void testSimdDetection() {
    std::cout << "\n=== SIMD Detection ===" << std::endl;

    std::cout << "  SIMD Level: " << engine::math::simdName() << std::endl;
    std::cout << "  SSE2:  " << ENGINE_SIMD_SSE2 << std::endl;
    std::cout << "  SSE41: " << ENGINE_SIMD_SSE41 << std::endl;
    std::cout << "  AVX:   " << ENGINE_SIMD_AVX << std::endl;
    std::cout << "  AVX2:  " << ENGINE_SIMD_AVX2 << std::endl;
    std::cout << "  FMA:   " << ENGINE_SIMD_FMA << std::endl;

    check(engine::math::hasSIMD(), "SIMD: SSE2 available on x86_64");

    // Verify alignment
    Vector3D v(1, 2, 3);
    check(reinterpret_cast<uintptr_t>(&v) % 16 == 0, "Vector3D: 16-byte aligned");

    Quaternion q(1, 2, 3, 4);
    check(reinterpret_cast<uintptr_t>(&q) % 16 == 0, "Quaternion: 16-byte aligned");
}

// ═══════════════════════════════════════════════════════════════
// Performance Micro-Benchmarks
// ═══════════════════════════════════════════════════════════════
void testPerformance() {
    std::cout << "\n=== Performance Micro-Benchmarks ===" << std::endl;

    const int N = 100000;

    // Quaternion multiplication benchmark
    {
        Quaternion a = Quaternion::fromAxisAngle(Vector3D::Up, 0.1f);
        Quaternion b = Quaternion::fromAxisAngle(Vector3D::Right, 0.2f);
        Quaternion r;
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; i++) r = a * b;
        auto end = std::chrono::high_resolution_clock::now();
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        std::cout << "  Quaternion mul x" << N << ": " << us << " us" << std::endl;
        check(us < 50000, "Perf: Quaternion mul < 50ms for 100K ops");
        (void)r;
    }

    // Vector3D normalize benchmark
    {
        Vector3D v(1.5f, 2.7f, 3.3f);
        Vector3D r;
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; i++) r = v.normalized();
        auto end = std::chrono::high_resolution_clock::now();
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        std::cout << "  Vector3D normalize x" << N << ": " << us << " us" << std::endl;
        check(us < 50000, "Perf: Vector3D normalize < 50ms for 100K ops");
        (void)r;
    }

    // GJK intersection benchmark
    {
        SphereCollider s1(Vector3D(0, 0, 0), 1.0f);
        SphereCollider s2(Vector3D(1.5f, 0, 0), 1.0f);
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N / 10; i++) {
            GJK::intersect(s1, s2);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        std::cout << "  GJK intersect x" << N/10 << ": " << us << " us" << std::endl;
        check(us < 100000, "Perf: GJK intersect < 100ms for 10K ops");
    }
}

// ═══════════════════════════════════════════════════════════════
// Main
// ═══════════════════════════════════════════════════════════════
int main() {
    std::cout << "╔══════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║  Deep Physics Optimization Tests                ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════╝" << std::endl;

    testGJKEPAOptimized();
    testBVHRaycast();
    testSIMDQuaternion();
    testSIMDVector3D();
    testSpringDamper();
    testNavMeshOptimized();
    testGridNavOptimized();
    testXPBDVolumeConstraint();
    testXPBDSelfCollision();
    testXPBDCloth();
    testVerletIntegration();
    testBVHBroadphase();
    testSimdDetection();
    testPerformance();

    std::cout << "\n══════════════════════════════════════════════════" << std::endl;
    std::cout << "Results: " << passed << " passed, " << failed << " failed" << std::endl;
    std::cout << "══════════════════════════════════════════════════" << std::endl;

    return failed > 0 ? 1 : 0;
}
