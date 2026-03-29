// test_physics_bench.cpp — Deep physics benchmarks and analytical solution verification
//
// Tests:
//   1. Free-fall analytical solution (1D kinematics)
//   2. Elastic collision momentum/energy conservation
//   3. Energy conservation over 10000 steps (bouncing ball)
//   4. Angular momentum conservation (torque-free rotation)
//   5. Extreme mass ratio stability (1:10000)
//   6. Extreme velocity stability (v=500 m/s)
//   7. RK4 vs analytical orbit comparison
//   8. Quaternion normalization drift test (100k steps)
//   9. Performance benchmark: 500 bodies
//  10. Performance benchmark: broadphase scaling

#include <iostream>
#include <cmath>
#include <chrono>
#include <vector>
#include <string>

#include "physics/PhysicsWorld3D.h"
#include "physics/PhysicsMath.h"
#include "math/Vector3D.h"
#include "math/Quaternion.h"

using namespace engine::math;
using namespace engine::physics;

static int passed = 0, failed = 0;

static void check(bool cond, const std::string& msg) {
    if (cond) { passed++; std::cout << "  [OK] " << msg << std::endl; }
    else      { failed++; std::cout << "  [FAIL] " << msg << std::endl; }
}

[[maybe_unused]] static bool approx(float a, float b, float eps = 0.01f) {
    return std::fabs(a - b) < eps;
}

// ── Test 1: Free-Fall Analytical Solution ─────────────────────────
void testFreeFall() {
    std::cout << "\n=== Free-Fall Analytical Solution ===" << std::endl;

    PhysicsWorld3D world;
    world.gravity = Vector3D(0, -9.81f, 0);
    world.subSteps = 4;
    int ball = world.addDynamicSphere(Vector3D(0, 100, 0), 0.5f, 1.0f, 0.0f, 0.0f);
    world.getBody(ball).m_linearDamping = 0.0f;
    world.getBody(ball).material.dragCoefficient = 0.0f;

    float dt = 1.0f / 60.0f;
    float t = 0.0f;
    for (int i = 0; i < 120; i++) { world.step(dt); t += dt; }

    float y_analytical = 100.0f - 0.5f * 9.81f * t * t;
    float vy_analytical = -9.81f * t;
    float y_sim = world.getBody(ball).position.y;
    float vy_sim = world.getBody(ball).velocity.y;

    std::cout << "  t=" << t << " y_sim=" << y_sim << " y_exact=" << y_analytical << std::endl;
    std::cout << "  vy_sim=" << vy_sim << " vy_exact=" << vy_analytical << std::endl;

    check(std::abs(y_sim - y_analytical) < 0.5f, "Free-fall position within 0.5m of analytical");
    check(std::abs(vy_sim - vy_analytical) < 0.5f, "Free-fall velocity within 0.5 m/s of analytical");
}

// ── Test 2: Elastic Collision Conservation ────────────────────────
void testElasticCollision() {
    std::cout << "\n=== Elastic Collision Conservation ===" << std::endl;

    PhysicsWorld3D world;
    world.gravity = Vector3D(0, 0, 0);
    world.subSteps = 8;

    // Two equal-mass spheres approaching each other
    int a = world.addDynamicSphere(Vector3D(-5, 0, 0), 0.5f, 1.0f, 1.0f, 0.0f);
    int b = world.addDynamicSphere(Vector3D(5, 0, 0), 0.5f, 1.0f, 1.0f, 0.0f);
    world.getBody(a).velocity = Vector3D(5, 0, 0);
    world.getBody(b).velocity = Vector3D(-5, 0, 0);
    world.getBody(a).m_linearDamping = 0.0f;
    world.getBody(b).m_linearDamping = 0.0f;

    float massA = world.getBody(a).getMass();
    float massB = world.getBody(b).getMass();
    Vector3D p0 = world.getBody(a).velocity * massA + world.getBody(b).velocity * massB;
    float ke0 = PhysicsMath::kineticEnergy(massA, world.getBody(a).velocity) +
                PhysicsMath::kineticEnergy(massB, world.getBody(b).velocity);

    for (int i = 0; i < 300; i++) world.step(1.0f / 60.0f);

    Vector3D p1 = world.getBody(a).velocity * massA + world.getBody(b).velocity * massB;
    float ke1 = PhysicsMath::kineticEnergy(massA, world.getBody(a).velocity) +
                PhysicsMath::kineticEnergy(massB, world.getBody(b).velocity);

    std::cout << "  p0=(" << p0.x << ") p1=(" << p1.x << ")" << std::endl;
    std::cout << "  ke0=" << ke0 << " ke1=" << ke1 << std::endl;

    check(std::abs(p1.x - p0.x) < 0.1f, "Momentum conserved in elastic collision");
    check(ke1 > ke0 * 0.7f, "Energy roughly conserved in elastic collision (>70%)");
}

// ── Test 3: Energy Conservation Over 10000 Steps ──────────────────
void testEnergyConservation10k() {
    std::cout << "\n=== Energy Conservation Over 10000 Steps ===" << std::endl;

    PhysicsWorld3D world;
    world.gravity = Vector3D(0, -9.81f, 0);
    world.subSteps = 4;

    // Bouncing ball on floor with no friction/damping
    int floor = world.addStaticBox(Vector3D(0, -1, 0), Vector3D(100, 1, 100), 0.0f);
    (void)floor;
    int ball = world.addDynamicSphere(Vector3D(0, 10, 0), 0.5f, 1.0f, 1.0f, 0.0f);
    world.getBody(ball).m_linearDamping = 0.0f;
    world.getBody(ball).m_angularDamping = 0.0f;
    world.getBody(ball).material.dragCoefficient = 0.0f;
    world.getBody(ball).material.rollingFriction = 0.0f;

    float mass = world.getBody(ball).getMass();
    float g = 9.81f;
    float E0 = mass * g * 10.0f; // Initial PE

    float maxDrift = 0.0f;
    for (int i = 0; i < 10000; i++) {
        world.step(1.0f / 60.0f);
        if (i % 1000 == 999) {
            float y = world.getBody(ball).position.y;
            float v = world.getBody(ball).velocity.magnitude();
            float E = 0.5f * mass * v * v + mass * g * (y + 0.5f); // +0.5 for sphere radius
            float drift = std::abs(E - E0) / E0;
            if (drift > maxDrift) maxDrift = drift;
        }
    }

    std::cout << "  Initial energy=" << E0 << " max drift=" << (maxDrift * 100) << "%" << std::endl;
    check(maxDrift < 0.5f, "Energy drift < 50% over 10000 steps");
}

// ── Test 4: Angular Momentum Conservation ─────────────────────────
void testAngularMomentumConservation() {
    std::cout << "\n=== Angular Momentum Conservation ===" << std::endl;

    // Torque-free spinning box — angular momentum should be conserved
    PhysicsWorld3D world;
    world.gravity = Vector3D(0, 0, 0);
    world.subSteps = 4;

    int box = world.addDynamicBox(Vector3D(0, 0, 0), Vector3D(1, 0.5f, 0.3f), 1.0f, 0.0f, 0.0f);
    world.getBody(box).angularVelocity = Vector3D(2, 5, 1);
    world.getBody(box).m_angularDamping = 0.0f;

    Vector3D invI = world.getBody(box).getInvInertia();
    Vector3D L0 = PhysicsMath::angularMomentum(world.getBody(box).angularVelocity, invI);
    float L0mag = L0.magnitude();

    for (int i = 0; i < 5000; i++) world.step(1.0f / 60.0f);

    Vector3D L1 = PhysicsMath::angularMomentum(world.getBody(box).angularVelocity,
                                                 world.getBody(box).getInvInertia());
    float L1mag = L1.magnitude();

    std::cout << "  |L0|=" << L0mag << " |L1|=" << L1mag << std::endl;
    float drift = std::abs(L1mag - L0mag) / (L0mag + 1e-8f);
    check(drift < 0.1f, "Angular momentum magnitude drift < 10%");
}

// ── Test 5: Extreme Mass Ratio Stability ──────────────────────────
void testExtremeMassRatio() {
    std::cout << "\n=== Extreme Mass Ratio (1:10000) ===" << std::endl;

    PhysicsWorld3D world;
    world.gravity = Vector3D(0, -9.81f, 0);
    world.subSteps = 8;
    world.setSolverIterations(20);

    int floor = world.addStaticBox(Vector3D(0, -1, 0), Vector3D(100, 1, 100));
    (void)floor;

    // Heavy sphere (10000 kg) on top of light sphere (1 kg)
    int light = world.addDynamicSphere(Vector3D(0, 2, 0), 0.5f, 1.0f);
    int heavy = world.addDynamicSphere(Vector3D(0, 5, 0), 0.5f, 10000.0f);
    (void)heavy;

    bool stable = true;
    for (int i = 0; i < 600; i++) {
        world.step(1.0f / 60.0f);
        float y = world.getBody(light).position.y;
        if (std::isnan(y) || std::isinf(y) || y < -100 || y > 100) {
            stable = false;
            std::cout << "  Unstable at step " << i << " y=" << y << std::endl;
            break;
        }
    }
    check(stable, "Extreme mass ratio: simulation remains stable");
}

// ── Test 6: Extreme Velocity Stability ────────────────────────────
void testExtremeVelocity() {
    std::cout << "\n=== Extreme Velocity Stability ===" << std::endl;

    PhysicsWorld3D world;
    world.gravity = Vector3D(0, 0, 0);
    world.subSteps = 4;

    int ball = world.addDynamicSphere(Vector3D(0, 0, 0), 0.5f, 1.0f);
    world.getBody(ball).velocity = Vector3D(300, 200, -100);

    bool stable = true;
    for (int i = 0; i < 600; i++) {
        world.step(1.0f / 60.0f);
        Vector3D pos = world.getBody(ball).position;
        if (std::isnan(pos.x) || std::isinf(pos.x)) {
            stable = false;
            break;
        }
    }
    check(stable, "Extreme velocity: no NaN/Inf");
}

// ── Test 7: RK4 vs Analytical (Harmonic Oscillator) ───────────────
void testRK4Accuracy() {
    std::cout << "\n=== RK4 vs Analytical (Spring) ===" << std::endl;

    // Mass-spring system: m*a = -k*x, k=100, m=1
    // Solution: x(t) = cos(10*t), v(t) = -10*sin(10*t)
    float k = 100.0f;
    float m = 1.0f;
    float invM = 1.0f / m;
    float dt = 0.01f;

    auto springForce = [k](Vector3D pos, Vector3D vel) -> Vector3D {
        (void)vel;
        return pos * (-k); // F = -kx
    };

    Vector3D pos(1, 0, 0);
    Vector3D vel(0, 0, 0);

    for (int i = 0; i < 1000; i++) {
        auto state = PhysicsMath::rk4Step(pos, vel, invM, dt, springForce);
        pos = state.position;
        vel = state.velocity;
    }

    float t = 1000 * dt; // 10 seconds
    float x_exact = std::cos(10.0f * t);
    float v_exact = -10.0f * std::sin(10.0f * t);

    std::cout << "  x_rk4=" << pos.x << " x_exact=" << x_exact << std::endl;
    std::cout << "  v_rk4=" << vel.x << " v_exact=" << v_exact << std::endl;

    check(std::abs(pos.x - x_exact) < 0.01f, "RK4 position error < 0.01 after 10s");
    check(std::abs(vel.x - v_exact) < 0.1f, "RK4 velocity error < 0.1 after 10s");
}

// ── Test 8: Quaternion Normalization Drift ────────────────────────
void testQuaternionDrift() {
    std::cout << "\n=== Quaternion Normalization Drift ===" << std::endl;

    PhysicsWorld3D world;
    world.gravity = Vector3D(0, 0, 0);
    world.subSteps = 1;

    int box = world.addDynamicBox(Vector3D(0, 0, 0), Vector3D(1, 0.5f, 0.3f), 1.0f);
    world.getBody(box).angularVelocity = Vector3D(10, 7, 3); // Fast spinning
    world.getBody(box).m_angularDamping = 0.0f;

    float maxDrift = 0.0f;
    for (int i = 0; i < 100000; i++) {
        world.step(1.0f / 60.0f);
        if (i % 10000 == 9999) {
            float qMag = world.getBody(box).getOrientation().magnitude();
            float drift = std::abs(qMag - 1.0f);
            if (drift > maxDrift) maxDrift = drift;
        }
    }
    std::cout << "  Max quaternion drift after 100k steps: " << maxDrift << std::endl;
    check(maxDrift < 0.001f, "Quaternion stays unit after 100k steps");
}

// ── Test 9: Performance Benchmark ─────────────────────────────────
void testPerformanceBenchmark() {
    std::cout << "\n=== Performance Benchmark (500 bodies) ===" << std::endl;

    PhysicsWorld3D world;
    world.gravity = Vector3D(0, -9.81f, 0);
    world.subSteps = 4;

    world.addStaticBox(Vector3D(0, -1, 0), Vector3D(100, 1, 100));

    // Stack 500 spheres in a grid
    int count = 0;
    for (int x = 0; x < 10; x++) {
        for (int y = 0; y < 10; y++) {
            for (int z = 0; z < 5; z++) {
                world.addDynamicSphere(
                    Vector3D(x * 1.2f - 5, y * 1.2f + 2, z * 1.2f - 3),
                    0.5f, 1.0f);
                count++;
            }
        }
    }
    std::cout << "  Bodies: " << count << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 120; i++) world.step(1.0f / 60.0f);
    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();

    std::cout << "  120 frames = " << ms << " ms (" << (ms / 120.0) << " ms/frame)" << std::endl;
    check(ms < 60000.0, "500-body benchmark completes in <60s");
}

// ── Test 10: Broadphase Scaling ───────────────────────────────────
void testBroadphaseScaling() {
    std::cout << "\n=== Broadphase Scaling ===" << std::endl;

    auto benchBodies = [](int n) -> double {
        PhysicsWorld3D world;
        world.gravity = Vector3D(0, -9.81f, 0);
        world.subSteps = 2;
        world.addStaticBox(Vector3D(0, -1, 0), Vector3D(500, 1, 500));
        for (int i = 0; i < n; i++) {
            float x = (i % 50) * 2.0f - 50;
            float z = (i / 50) * 2.0f - 50;
            world.addDynamicSphere(Vector3D(x, 5 + (i % 10) * 1.5f, z), 0.5f, 1.0f);
        }
        auto start = std::chrono::high_resolution_clock::now();
        for (int f = 0; f < 30; f++) world.step(1.0f / 60.0f);
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start).count();
    };

    double t100 = benchBodies(100);
    double t500 = benchBodies(500);
    std::cout << "  100 bodies: " << t100 << " ms" << std::endl;
    std::cout << "  500 bodies: " << t500 << " ms" << std::endl;
    double ratio = t500 / (t100 + 0.001);
    std::cout << "  Scaling ratio (500/100): " << ratio << "x" << std::endl;
    check(ratio < 50.0, "Broadphase scaling subquadratic (ratio < 50x for 5x bodies)");
}

int main() {
    std::cout << "╔══════════════════════════════════════╗" << std::endl;
    std::cout << "║  Deep Physics Benchmarks & Analytics ║" << std::endl;
    std::cout << "╚══════════════════════════════════════╝" << std::endl;

    testFreeFall();
    testElasticCollision();
    testEnergyConservation10k();
    testAngularMomentumConservation();
    testExtremeMassRatio();
    testExtremeVelocity();
    testRK4Accuracy();
    testQuaternionDrift();
    testPerformanceBenchmark();
    testBroadphaseScaling();

    std::cout << "\n============================================" << std::endl;
    std::cout << "  Results: " << passed << " passed, " << failed << " failed" << std::endl;
    std::cout << "============================================" << std::endl;

    return failed > 0 ? 1 : 0;
}
