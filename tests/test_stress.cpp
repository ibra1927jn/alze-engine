// test_stress.cpp — Stress test para el motor 2D
//
// Crea 5000 entidades con física + colisión y mide:
//   - Tiempo de creación
//   - Tiempo de update (physics + collision)
//   - FPS equivalente
//   - Pares de colisión procesados
//

#include <iostream>
#include <chrono>
#include <cmath>
#include <string>
#include <cstdlib>

#include "math/Vector2D.h"
#include "math/MathUtils.h"
#include "ecs/ECSCoordinator.h"
#include "ecs/Components.h"
#include "ecs/systems/PhysicsSystem.h"
#include "ecs/systems/CollisionSystem.h"

using namespace engine;
using Clock = std::chrono::high_resolution_clock;

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

void testStress5000() {
    std::cout << "\n=== Stress Test: 5000 Entities ===" << std::endl;

    ecs::ECSCoordinator ecs;
    ecs.registerComponent<ecs::TransformComponent>();
    ecs.registerComponent<ecs::PhysicsComponent>();
    ecs.registerComponent<ecs::ColliderComponent>();
    ecs.registerComponent<ecs::SpriteComponent>();

    auto* physics = ecs.registerSystem<ecs::PhysicsSystem>(ecs);
    auto* collision = ecs.registerSystem<ecs::CollisionSystem>(ecs);
    physics->setGravity(980.0f);

    constexpr int NUM_ENTITIES = 5000;

    // ── Create 5000 entities ──
    auto t0 = Clock::now();
    for (int i = 0; i < NUM_ENTITIES; i++) {
        auto e = ecs.createEntity();
        float x = static_cast<float>(rand() % 2000);
        float y = static_cast<float>(rand() % 2000);
        ecs.addComponent<ecs::TransformComponent>(e, ecs::TransformComponent({x, y}));

        ecs::PhysicsComponent p(1.0f);
        p.velocity = {static_cast<float>(rand() % 200 - 100),
                      static_cast<float>(rand() % 200 - 100)};
        p.drag = 0.01f;
        ecs.addComponent<ecs::PhysicsComponent>(e, p);

        ecs.addComponent<ecs::ColliderComponent>(e,
            ecs::ColliderComponent(math::Vector2D(16, 16)));

        ecs.addComponent<ecs::SpriteComponent>(e,
            ecs::SpriteComponent(math::Color::green(), 16, 16));
    }
    auto t1 = Clock::now();
    double createMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::cout << "  [INFO] Create " << NUM_ENTITIES << " entities: "
              << createMs << "ms" << std::endl;
    check(createMs < 100, "Creation < 100ms");
    check(ecs.getActiveEntityCount() == NUM_ENTITIES,
          "Active count = " + std::to_string(NUM_ENTITIES));

    // ── Simulate 60 frames ──
    constexpr int NUM_FRAMES = 60;
    double totalPhysicsMs = 0;
    double totalCollisionMs = 0;
    double maxFrameMs = 0;

    for (int frame = 0; frame < NUM_FRAMES; frame++) {
        auto f0 = Clock::now();
        physics->update(1.0f / 60.0f);
        auto f1 = Clock::now();
        collision->update(1.0f / 60.0f);
        auto f2 = Clock::now();

        double physMs = std::chrono::duration<double, std::milli>(f1 - f0).count();
        double collMs = std::chrono::duration<double, std::milli>(f2 - f1).count();
        double frameMs = physMs + collMs;

        totalPhysicsMs += physMs;
        totalCollisionMs += collMs;
        if (frameMs > maxFrameMs) maxFrameMs = frameMs;
    }

    double avgPhysics = totalPhysicsMs / NUM_FRAMES;
    double avgCollision = totalCollisionMs / NUM_FRAMES;
    double avgFrame = avgPhysics + avgCollision;
    double equivFPS = 1000.0 / (avgFrame > 0 ? avgFrame : 0.001);

    std::cout << "  [INFO] Avg Physics:   " << avgPhysics << "ms" << std::endl;
    std::cout << "  [INFO] Avg Collision: " << avgCollision << "ms" << std::endl;
    std::cout << "  [INFO] Avg Frame:     " << avgFrame << "ms" << std::endl;
    std::cout << "  [INFO] Max Frame:     " << maxFrameMs << "ms" << std::endl;
    std::cout << "  [INFO] Equiv FPS:     " << static_cast<int>(equivFPS) << std::endl;
    std::cout << "  [INFO] Broad:  " << collision->getBroadPhaseTests()
              << "  Narrow: " << collision->getNarrowPhaseTests()
              << "  Resolved: " << collision->getCollisionsResolved() << std::endl;

    check(avgPhysics < 5, "Avg Physics < 5ms for 5K entities");
    check(avgCollision < 16, "Avg Collision < 16ms for 5K entities");
    check(avgFrame < 16.67, "Avg Frame < 16.67ms (60 FPS target)");
    check(maxFrameMs < 33.33, "Max Frame < 33.33ms (30 FPS floor)");
    check(equivFPS > 30, "Equiv FPS > 30");

    // ── Sleeping entities test ──
    int sleeping = physics->getSleepingCount();
    std::cout << "  [INFO] Sleeping: " << sleeping << " / " << NUM_ENTITIES << std::endl;
}

void testStress1000Concentrated() {
    std::cout << "\n=== Stress Test: 1000 entities concentrated ===" << std::endl;

    ecs::ECSCoordinator ecs;
    ecs.registerComponent<ecs::TransformComponent>();
    ecs.registerComponent<ecs::PhysicsComponent>();
    ecs.registerComponent<ecs::ColliderComponent>();
    ecs.registerComponent<ecs::SpriteComponent>();

    auto* physics = ecs.registerSystem<ecs::PhysicsSystem>(ecs);
    auto* collision = ecs.registerSystem<ecs::CollisionSystem>(ecs);
    physics->setGravity(980.0f);

    // All entities in a small area → maximum collision pairs
    for (int i = 0; i < 1000; i++) {
        auto e = ecs.createEntity();
        float x = static_cast<float>(rand() % 200);
        float y = static_cast<float>(rand() % 200);
        ecs.addComponent<ecs::TransformComponent>(e, ecs::TransformComponent({x, y}));

        ecs::PhysicsComponent p(1.0f);
        p.velocity = {static_cast<float>(rand() % 50 - 25),
                      static_cast<float>(rand() % 50 - 25)};
        ecs.addComponent<ecs::PhysicsComponent>(e, p);
        ecs.addComponent<ecs::ColliderComponent>(e,
            ecs::ColliderComponent(math::Vector2D(16, 16)));
    }

    // Simulate worst case: all overlapping
    auto t0 = Clock::now();
    for (int i = 0; i < 10; i++) {
        physics->update(1.0f / 60.0f);
        collision->update(1.0f / 60.0f);
    }
    auto t1 = Clock::now();
    double totalMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double avgFrame = totalMs / 10.0;

    std::cout << "  [INFO] Avg Frame (concentrated): " << avgFrame << "ms" << std::endl;
    std::cout << "  [INFO] Broad:  " << collision->getBroadPhaseTests()
              << "  Narrow: " << collision->getNarrowPhaseTests()
              << "  Resolved: " << collision->getCollisionsResolved() << std::endl;

    check(avgFrame < 50, "Concentrated collision < 50ms/frame");
}

int main() {
    std::cout << "============================================" << std::endl;
    std::cout << "  PhysicsEngine2D — Stress Tests" << std::endl;
    std::cout << "  5000 entities | Concentrated collision" << std::endl;
    std::cout << "============================================" << std::endl;

    testStress5000();
    testStress1000Concentrated();

    std::cout << "\n============================================" << std::endl;
    std::cout << "  Resultados: " << passed << " pasados, "
              << failed << " fallidos" << std::endl;
    std::cout << "============================================" << std::endl;

    return failed > 0 ? 1 : 0;
}
