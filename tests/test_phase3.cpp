// test_phase3.cpp — Tests para Camera2D, Raycast, Triggers, Z-ordering
//
// Cubre las features nuevas implementadas en la fase de refinamiento 2D:
//   - Camera2D: worldToScreen, screenToWorld, zoom, follow, frustum culling
//   - Raycast: ray-AABB intersection (Slab Method), castFirst
//   - Triggers/Layers: shouldCollide, isTrigger, layer masks
//   - Z-ordering: SpriteComponent zOrder field
//

#include <iostream>
#include <cmath>
#include <cassert>
#include <string>

#include "math/Vector2D.h"
#include "math/AABB.h"
#include "math/MathUtils.h"
#include "math/Color.h"
#include "core/Camera2D.h"
#include "core/EventBus.h"
#include "physics/Raycast.h"
#include "ecs/ECSCoordinator.h"
#include "ecs/Components.h"

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

static bool approx(float a, float b, float eps = 1.0f) {
    return std::abs(a - b) < eps;
}

// ═════════════════════════════════════════════════════════════════
// Camera2D Tests
// ═════════════════════════════════════════════════════════════════
void testCamera2D() {
    std::cout << "\n=== Camera2D Tests ===" << std::endl;

    core::Camera2D cam(800, 600);

    // ── worldToScreen at origin ──
    cam.setPosition({0, 0});
    cam.setTarget({0, 0});
    cam.setZoom(1.0f);

    // Center of camera → center of screen
    math::Vector2D screen = cam.worldToScreen({0, 0});
    check(approx(screen.x, 400) && approx(screen.y, 300),
          "worldToScreen: origin → center of screen");

    // ── worldToScreen offset ──
    screen = cam.worldToScreen({100, 0});
    check(approx(screen.x, 500) && approx(screen.y, 300),
          "worldToScreen: (100,0) → (500,300)");

    // ── screenToWorld roundtrip ──
    math::Vector2D world = cam.screenToWorld(screen);
    check(approx(world.x, 100) && approx(world.y, 0),
          "screenToWorld roundtrip: (500,300) → (100,0)");

    // ── Zoom in x2 ──
    cam.setZoom(2.0f);
    screen = cam.worldToScreen({50, 0});
    check(approx(screen.x, 500) && approx(screen.y, 300),
          "Zoom 2x: worldToScreen (50,0) → (500,300)");

    // ── Zoom preserves roundtrip ──
    world = cam.screenToWorld({500, 300});
    check(approx(world.x, 50) && approx(world.y, 0),
          "Zoom 2x: screenToWorld roundtrip preservado");

    // ── Frustum culling: visible ──
    cam.setZoom(1.0f);
    cam.setPosition({0, 0});
    check(cam.isVisible({0, 0}, {32, 32}), "Frustum: centro visible");
    check(cam.isVisible({380, 280}, {32, 32}), "Frustum: esquina visible");

    // ── Frustum culling: invisible ──
    check(!cam.isVisible({600, 0}, {32, 32}), "Frustum: fuera derecha invisible");
    check(!cam.isVisible({0, 500}, {32, 32}), "Frustum: fuera abajo invisible");
    check(!cam.isVisible({-600, 0}, {32, 32}), "Frustum: fuera izquierda invisible");

    // ── Zoom reduces visible area ──
    cam.setZoom(2.0f);
    check(!cam.isVisible({300, 0}, {32, 32}),
          "Zoom 2x: (300,0) ahora fuera de frustum");
    check(cam.isVisible({150, 0}, {32, 32}),
          "Zoom 2x: (150,0) aún visible");

    // ── worldRectToScreen ──
    cam.setZoom(1.0f);
    cam.setPosition({0, 0});
    SDL_Rect rect = cam.worldRectToScreen({0, 0}, {32, 32});
    check(rect.w == 32 && rect.h == 32, "worldRectToScreen: size correct at zoom 1");

    cam.setZoom(2.0f);
    rect = cam.worldRectToScreen({0, 0}, {32, 32});
    check(rect.w == 64 && rect.h == 64, "worldRectToScreen: size doubled at zoom 2");

    // ── Zoom clamping ──
    cam.setZoom(0.01f);
    check(approx(cam.getZoom(), 0.1f, 0.01f), "Zoom clamp min: 0.01 → 0.1");
    cam.setZoom(20.0f);
    check(approx(cam.getZoom(), 10.0f, 0.01f), "Zoom clamp max: 20 → 10");

    // ── Camera follow (smooth follow test) ──
    cam.setPosition({0, 0});
    cam.setTarget({100, 0});
    cam.setSmoothSpeed(1.0f);  // Instant follow
    cam.update(1.0f / 60.0f);
    check(approx(cam.getPosition().x, 100, 5),
          "Follow con smooth=1.0 → casi instantáneo");

    cam.setPosition({0, 0});
    cam.setTarget({100, 0});
    cam.setSmoothSpeed(0.01f);  // Very slow follow
    cam.update(1.0f / 60.0f);
    check(cam.getPosition().x < 50,
          "Follow con smooth=0.01 → se mueve poco");
}

// ═════════════════════════════════════════════════════════════════
// Raycast Tests
// ═════════════════════════════════════════════════════════════════
void testRaycast() {
    std::cout << "\n=== Raycast Tests ===" << std::endl;

    // ── Ray-AABB intersection ──
    math::AABB box = math::AABB::fromCenter({100, 0}, {20, 20});

    float tMin, tMax;
    bool hit = physics::Raycast::rayAABBIntersect(
        {0, 0}, {1, 0}, box, tMin, tMax);
    check(hit, "Ray →x hits AABB at (100,0)");
    check(approx(tMin, 80, 2), "Hit distance ≈ 80 (left edge)");

    // ── Miss (perpendicular) ──
    hit = physics::Raycast::rayAABBIntersect(
        {0, 0}, {0, 1}, box, tMin, tMax);
    check(!hit || tMin > 1000, "Ray →y misses AABB at (100,0)");

    // ── Miss (behind ray) ──
    hit = physics::Raycast::rayAABBIntersect(
        {200, 0}, {1, 0}, box, tMin, tMax);
    check(!hit || tMin < 0, "Ray starting past AABB reports negative t");

    // ── castFirst with ECS ──
    ecs::ECSCoordinator ecs;
    ecs.registerComponent<ecs::TransformComponent>();
    ecs.registerComponent<ecs::ColliderComponent>();
    ecs.registerComponent<ecs::PhysicsComponent>();
    ecs.registerComponent<ecs::SpriteComponent>();

    // Crear una pared a (200, 0)
    auto wall = ecs.createEntity();
    ecs.addComponent<ecs::TransformComponent>(wall, ecs::TransformComponent({200, 0}));
    auto& wallCol = ecs.emplaceComponent<ecs::ColliderComponent>(wall,
        math::Vector2D(40, 40), true);
    wallCol.aabb = math::AABB::fromCenter({200, 0}, {20, 20});

    // Crear un bloque a (400, 0)
    auto block = ecs.createEntity();
    ecs.addComponent<ecs::TransformComponent>(block, ecs::TransformComponent({400, 0}));
    auto& blockCol = ecs.emplaceComponent<ecs::ColliderComponent>(block,
        math::Vector2D(40, 40), true);
    blockCol.aabb = math::AABB::fromCenter({400, 0}, {20, 20});

    physics::RayHit outHit;
    bool found = physics::Raycast::castFirst(ecs, {0, 0}, {1, 0}, 500.0f, outHit);
    check(found, "castFirst: encuentra hit");
    check(outHit.entity == wall, "castFirst: primer hit es la pared (más cercana)");
    check(approx(outHit.distance, 180, 5), "castFirst: distancia ≈ 180");
    check(approx(outHit.normal.x, -1, 0.1f), "castFirst: normal = (-1, 0)");

    // ── castFirst with ignore ──
    found = physics::Raycast::castFirst(ecs, {0, 0}, {1, 0}, 500.0f, outHit, wall);
    check(found, "castFirst ignore: encuentra segundo hit");
    check(outHit.entity == block, "castFirst ignore: salta la pared");

    // ── castAll ──
    auto allHits = physics::Raycast::castAll(ecs, {0, 0}, {1, 0}, 500.0f);
    check(allHits.size() == 2, "castAll: 2 hits");
}

// ═════════════════════════════════════════════════════════════════
// Trigger / Layer Tests
// ═════════════════════════════════════════════════════════════════
void testTriggersAndLayers() {
    std::cout << "\n=== Trigger/Layer Tests ===" << std::endl;

    // ── shouldCollide: same layer ──
    ecs::ColliderComponent a;
    a.layer = 1;
    a.collisionMask = 0xFFFFFFFF;

    ecs::ColliderComponent b;
    b.layer = 1;
    b.collisionMask = 0xFFFFFFFF;

    check(a.shouldCollide(b), "shouldCollide: same layer → true");

    // ── shouldCollide: different layers ──
    a.layer = 1;
    a.collisionMask = 1;   // Solo colisiona con layer 1
    b.layer = 2;
    b.collisionMask = 2;   // Solo colisiona con layer 2
    check(!a.shouldCollide(b), "shouldCollide: incompatible layers → false");

    // ── shouldCollide: mask allows ──
    a.layer = 1;
    a.collisionMask = 3;  // Colisiona con layers 1 y 2
    b.layer = 2;
    b.collisionMask = 3;
    check(a.shouldCollide(b), "shouldCollide: mask incluye → true");

    // ── isTrigger default ──
    ecs::ColliderComponent trigger;
    check(!trigger.isTrigger, "isTrigger default = false");
    trigger.isTrigger = true;
    check(trigger.isTrigger, "isTrigger set = true");

    // ── TriggerEvent struct ──
    core::TriggerEvent te{42, 99};
    check(te.entityA == 42 && te.entityB == 99, "TriggerEvent fields");

    // ── EventBus with TriggerEvent ──
    core::EventBus bus;
    bool triggered = false;
    bus.subscribe<core::TriggerEvent>([&](const core::TriggerEvent& e) {
        triggered = true;
        check(e.entityA == 1 && e.entityB == 2, "TriggerEvent via EventBus");
    });
    bus.emit(core::TriggerEvent{1, 2});
    check(triggered, "TriggerEvent was received");

    // ── Layer bitmask operations ──
    ecs::ColliderComponent player;
    player.layer = 0x01;          // Layer PLAYER
    player.collisionMask = 0x06;  // Colisiona con ENEMY (0x02) y WALL (0x04)

    ecs::ColliderComponent enemy;
    enemy.layer = 0x02;           // Layer ENEMY
    enemy.collisionMask = 0x01;   // Solo colisiona con PLAYER

    ecs::ColliderComponent wall;
    wall.layer = 0x04;            // Layer WALL
    wall.collisionMask = 0xFF;    // Colisiona con todo

    check(player.shouldCollide(enemy), "Player↔Enemy: both masks match");
    check(player.shouldCollide(wall), "Player↔Wall: both masks match");
    check(!enemy.shouldCollide(wall), "Enemy↔Wall: enemy mask doesn't include wall");
}

// ═════════════════════════════════════════════════════════════════
// Z-ordering Tests
// ═════════════════════════════════════════════════════════════════
void testZOrdering() {
    std::cout << "\n=== Z-ordering Tests ===" << std::endl;

    ecs::SpriteComponent bg(math::Color::blue(), 100, 100, -100);
    check(bg.zOrder == -100, "SpriteComponent: zOrder = -100 (background)");

    ecs::SpriteComponent normal(math::Color::white(), 32, 32);
    check(normal.zOrder == 0, "SpriteComponent: zOrder default = 0");

    ecs::SpriteComponent fg(math::Color::red(), 16, 16, 100);
    check(fg.zOrder == 100, "SpriteComponent: zOrder = 100 (foreground)");

    // Verify sort order
    check(bg.zOrder < normal.zOrder, "bg < normal");
    check(normal.zOrder < fg.zOrder, "normal < fg");
}

// ═════════════════════════════════════════════════════════════════
// Main
// ═════════════════════════════════════════════════════════════════
int main() {
    std::cout << "============================================" << std::endl;
    std::cout << "  PhysicsEngine2D — Tests Phase 3" << std::endl;
    std::cout << "  Camera2D | Raycast | Triggers | Z-order" << std::endl;
    std::cout << "============================================" << std::endl;

    testCamera2D();
    testRaycast();
    testTriggersAndLayers();
    testZOrdering();

    std::cout << "\n============================================" << std::endl;
    std::cout << "  Resultados: " << passed << " pasados, "
              << failed << " fallidos" << std::endl;
    std::cout << "============================================" << std::endl;

    return failed > 0 ? 1 : 0;
}
