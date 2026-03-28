// test_physics.cpp — Tests para PhysicsSystem + PhysicsComponent
//
// Verifica la integración real de física dentro del ECS:
//   - PhysicsComponent: masa, velocidad, fuerzas, impulsos
//   - PhysicsSystem: integración semi-implícita de Euler, gravedad, drag, sleep
//   - SpatialHash (ya testeado en test_phase25)
//

#include <iostream>
#include <cmath>
#include <string>

#include "math/Vector2D.h"
#include "math/Transform2D.h"
#include "math/MathUtils.h"
#include "ecs/ECSCoordinator.h"
#include "ecs/Components.h"
#include "ecs/systems/PhysicsSystem.h"

using namespace engine;

static int passed = 0;
static int failed = 0;

static void check(bool cond, const std::string& msg) {
    if (cond) {
        std::cout << "  [OK] " << msg << std::endl;
        passed++;
    } else {
        std::cout << "  [FAIL] " << msg << std::endl;
        failed++;
    }
}

static bool approx(float a, float b, float eps = 0.5f) {
    return std::abs(a - b) < eps;
}

// ═════════════════════════════════════════════════════════════════
// PhysicsComponent Tests
// ═════════════════════════════════════════════════════════════════
void testPhysicsComponent() {
    std::cout << "\n=== PhysicsComponent Tests ===" << std::endl;

    ecs::PhysicsComponent phys(2.0f);  // masa = 2
    check(approx(phys.mass, 2.0f), "Mass = 2");
    check(approx(phys.invMass, 0.5f), "InvMass = 0.5");

    // Static body (mass = 0)
    ecs::PhysicsComponent staticPhys(0.0f);
    check(approx(staticPhys.invMass, 0.0f), "Static: invMass = 0");

    // Apply force
    ecs::PhysicsOps::applyForce(phys, {100, 0});
    check(phys.acceleration.x > 0, "applyForce calcula accel inmediato (F*invMass)");

    // Wake/Sleep
    phys.isSleeping = true;
    phys.sleepTimer = 1.0f;
    ecs::PhysicsOps::wake(phys);
    check(!phys.isSleeping, "wake() despierta");
    check(approx(phys.sleepTimer, 0), "wake() resetea sleepTimer");

    // Default values
    ecs::PhysicsComponent def(1.0f);
    check(approx(def.drag, 0.01f), "Drag default = 0.01");
}

// ═════════════════════════════════════════════════════════════════
// PhysicsSystem Integration Tests
// ═════════════════════════════════════════════════════════════════
void testPhysicsSystem() {
    std::cout << "\n=== PhysicsSystem Tests ===" << std::endl;

    ecs::ECSCoordinator ecs;
    ecs.registerComponent<ecs::TransformComponent>();
    ecs.registerComponent<ecs::PhysicsComponent>();
    ecs.registerComponent<ecs::ColliderComponent>();
    ecs.registerComponent<ecs::SpriteComponent>();

    auto* physics = ecs.registerSystem<ecs::PhysicsSystem>(ecs);
    physics->setGravity(0.0f);  // Sin gravedad para tests controlados

    // ── Test 1: Velocity integration ──
    auto e1 = ecs.createEntity();
    ecs.addComponent<ecs::TransformComponent>(e1, ecs::TransformComponent({0, 0}));
    ecs::PhysicsComponent p1(1.0f);
    p1.velocity = {100, 0};
    ecs.addComponent<ecs::PhysicsComponent>(e1, p1);

    physics->update(1.0f / 60.0f);

    auto& tf1 = ecs.getComponent<ecs::TransformComponent>(e1);
    check(tf1.transform.position.x > 0, "Velocity moves entity right");
    check(approx(tf1.transform.position.x, 100.0f / 60.0f, 0.5f),
          "Position ≈ v * dt");

    // ── Test 2: Force application ──
    auto e2 = ecs.createEntity();
    ecs.addComponent<ecs::TransformComponent>(e2, ecs::TransformComponent({0, 0}));
    ecs::PhysicsComponent p2(1.0f);  // masa = 1
    ecs.addComponent<ecs::PhysicsComponent>(e2, p2);

    auto& phys2 = ecs.getComponent<ecs::PhysicsComponent>(e2);
    ecs::PhysicsOps::applyForce(phys2, {600, 0});  // F=600, m=1 → a=600

    physics->update(1.0f / 60.0f);

    check(phys2.velocity.x > 0, "Force accelerates entity");
    check(approx(phys2.velocity.x, 10.0f, 2.0f), "v ≈ a*dt = 600/60 = 10");

    // ── Test 3: Gravity ──
    physics->setGravity(980.0f);

    auto e3 = ecs.createEntity();
    ecs.addComponent<ecs::TransformComponent>(e3, ecs::TransformComponent({0, 0}));
    ecs::PhysicsComponent p3(1.0f);
    ecs.addComponent<ecs::PhysicsComponent>(e3, p3);

    physics->update(1.0f / 60.0f);

    auto& phys3 = ecs.getComponent<ecs::PhysicsComponent>(e3);
    check(phys3.velocity.y > 0, "Gravity pulls down (positive Y)");
    check(approx(phys3.velocity.y, 980.0f / 60.0f, 2.0f),
          "v_y ≈ g*dt = 16.33");

    physics->setGravity(0.0f);  // Reset for remaining tests

    // ── Test 4: Drag ──
    auto e4 = ecs.createEntity();
    ecs.addComponent<ecs::TransformComponent>(e4, ecs::TransformComponent({0, 0}));
    ecs::PhysicsComponent p4(1.0f);
    p4.velocity = {200, 0};
    p4.drag = 5.0f;
    ecs.addComponent<ecs::PhysicsComponent>(e4, p4);

    physics->update(1.0f / 60.0f);

    auto& phys4 = ecs.getComponent<ecs::PhysicsComponent>(e4);
    check(phys4.velocity.x < 200.0f, "Drag reduces velocity");
    check(phys4.velocity.x > 0, "Drag doesn't reverse velocity");

    // ── Test 5: previousPosition updated ──
    auto e5 = ecs.createEntity();
    ecs.addComponent<ecs::TransformComponent>(e5, ecs::TransformComponent({50, 50}));
    ecs::PhysicsComponent p5(1.0f);
    p5.velocity = {100, 0};
    p5.previousPosition = {50, 50};
    ecs.addComponent<ecs::PhysicsComponent>(e5, p5);

    physics->update(1.0f / 60.0f);

    auto& phys5 = ecs.getComponent<ecs::PhysicsComponent>(e5);
    check(approx(phys5.previousPosition.x, 50.0f, 1.0f),
          "previousPosition = old position");

    // ── Test 6: Sleep check ──
    auto e6 = ecs.createEntity();
    ecs.addComponent<ecs::TransformComponent>(e6, ecs::TransformComponent({0, 0}));
    ecs::PhysicsComponent p6(1.0f);
    p6.velocity = {0.001f, 0};  // Casi quieto
    ecs.addComponent<ecs::PhysicsComponent>(e6, p6);

    // Simular muchos frames para que duerma
    for (int i = 0; i < 120; i++) {
        physics->update(1.0f / 60.0f);
    }

    auto& phys6 = ecs.getComponent<ecs::PhysicsComponent>(e6);
    check(phys6.isSleeping, "Entity sleeps after being still");

    // ── Test 7: Static bodies don't move ──
    auto e7 = ecs.createEntity();
    ecs.addComponent<ecs::TransformComponent>(e7, ecs::TransformComponent({100, 100}));
    ecs::PhysicsComponent p7(0.0f, true);  // Static (mass=0, isStatic=true)
    p7.velocity = {999, 999};
    ecs.addComponent<ecs::PhysicsComponent>(e7, p7);

    physics->setGravity(980.0f);
    physics->update(1.0f / 60.0f);

    auto& tf7 = ecs.getComponent<ecs::TransformComponent>(e7);
    check(approx(tf7.transform.position.x, 100.0f) &&
          approx(tf7.transform.position.y, 100.0f),
          "Static body doesn't move");
}

int main() {
    std::cout << "============================================" << std::endl;
    std::cout << "  PhysicsEngine2D — Physics Tests (Live)" << std::endl;
    std::cout << "  PhysicsComponent + PhysicsSystem" << std::endl;
    std::cout << "============================================" << std::endl;

    testPhysicsComponent();
    testPhysicsSystem();

    std::cout << "\n============================================" << std::endl;
    std::cout << "  Resultados: " << passed << " pasados, "
              << failed << " fallidos" << std::endl;
    std::cout << "============================================" << std::endl;

    return failed > 0 ? 1 : 0;
}
