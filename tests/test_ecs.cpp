#include "EntityManager.h"
#include "ComponentStorage.h"
#include "ECSCoordinator.h"
#include "Components.h"
#include "MathUtils.h"
#include <iostream>
#include <vector>

using namespace engine::ecs;
using namespace engine::math;

int passed = 0;
int failed = 0;

#define TEST(name, expr) \
    if (expr) { passed++; std::cout << "  [OK] " << name << std::endl; } \
    else { failed++; std::cerr << "  [FAIL] " << name << std::endl; }

// ═══════════════════════════════════════════════════════════════
// Tests de EntityManager
// ═══════════════════════════════════════════════════════════════
void testEntityManager() {
    std::cout << "\n=== EntityManager Tests ===" << std::endl;

    EntityManager em;

    // Crear entidades
    Entity e1 = em.createEntity();
    Entity e2 = em.createEntity();
    TEST("E1 creada", em.isAlive(e1));
    TEST("E2 creada", em.isAlive(e2));
    TEST("E1 != E2", e1 != e2);
    TEST("Active count = 2", em.getActiveCount() == 2);

    // Índices únicos
    TEST("E1 index = 0", getIndex(e1) == 0);
    TEST("E2 index = 1", getIndex(e2) == 1);
    TEST("E1 gen = 0", getGeneration(e1) == 0);

    // Destruir y reutilizar
    em.destroyEntity(e1);
    TEST("E1 destruida", !em.isAlive(e1));
    TEST("Active count = 1", em.getActiveCount() == 1);

    Entity e3 = em.createEntity();
    TEST("E3 reutiliza índice 0", getIndex(e3) == 0);
    TEST("E3 generación > 0", getGeneration(e3) > 0);
    TEST("E1 original sigue muerta (stale)", !em.isAlive(e1));
    TEST("E3 viva", em.isAlive(e3));

    // Stale detection: e1 tiene gen=0, pero ahora gen=1
    TEST("E1 stale != E3", e1 != e3);

    // CMP Mask
    em.setComponentBit(e3, 0, true);
    em.setComponentBit(e3, 2, true);
    ComponentMask mask = em.getComponentMask(e3);
    TEST("CMP bit 0", mask.test(0));
    TEST("CMP bit 2", mask.test(2));
    TEST("CMP bit 1 = false", !mask.test(1));

    // Signature matching
    ComponentMask sig;
    sig.set(0);
    sig.set(2);
    TEST("Matches signature", em.matchesSignature(e3, sig));

    ComponentMask sigExtra;
    sigExtra.set(0);
    sigExtra.set(2);
    sigExtra.set(5);
    TEST("No matches extra sig", !em.matchesSignature(e3, sigExtra));

    // Tags
    em.setTag(e3, TAG_PLAYER, true);
    TEST("Has player tag", em.hasTag(e3, TAG_PLAYER));
    TEST("No enemy tag", !em.hasTag(e3, TAG_ENEMY));

    // Crear muchas entidades
    std::vector<Entity> entities;
    for (int i = 0; i < 100; i++) {
        entities.push_back(em.createEntity());
    }
    TEST("100 entidades creadas", em.getActiveCount() == 102); // e2 + e3 + 100

    // Destruir todas
    for (auto e : entities) em.destroyEntity(e);
    em.destroyEntity(e2);
    em.destroyEntity(e3);
    TEST("Todas destruidas", em.getActiveCount() == 0);
}

// ═══════════════════════════════════════════════════════════════
// Tests de ComponentStorage
// ═══════════════════════════════════════════════════════════════
void testComponentStorage() {
    std::cout << "\n=== ComponentStorage Tests ===" << std::endl;

    EntityManager em;
    ComponentStorage<TransformComponent> storage;

    Entity e1 = em.createEntity();
    Entity e2 = em.createEntity();
    Entity e3 = em.createEntity();

    // Add
    storage.add(e1, TransformComponent(Vector2D(10, 20)));
    TEST("Has e1", storage.has(e1));
    TEST("Size = 1", storage.size() == 1);

    storage.add(e2, TransformComponent(Vector2D(30, 40)));
    storage.add(e3, TransformComponent(Vector2D(50, 60)));
    TEST("Size = 3", storage.size() == 3);

    // Get
    TEST("E1 pos", storage.get(e1).transform.position == Vector2D(10, 20));
    TEST("E2 pos", storage.get(e2).transform.position == Vector2D(30, 40));
    TEST("E3 pos", storage.get(e3).transform.position == Vector2D(50, 60));

    // Modify via reference
    storage.get(e2).transform.position = Vector2D(100, 200);
    TEST("E2 modified", storage.get(e2).transform.position == Vector2D(100, 200));

    // Remove with swap-and-pop
    storage.remove(e1);
    TEST("E1 removed", !storage.has(e1));
    TEST("E2 still there", storage.has(e2));
    TEST("E3 still there", storage.has(e3));
    TEST("Size = 2", storage.size() == 2);

    // E2 and E3 values intact after swap
    TEST("E2 value after swap", storage.get(e2).transform.position == Vector2D(100, 200));
    TEST("E3 value after swap", storage.get(e3).transform.position == Vector2D(50, 60));

    // Remove all
    storage.remove(e2);
    storage.remove(e3);
    TEST("Empty", storage.empty());

    // Emplace
    Entity e4 = em.createEntity();
    auto& ref = storage.emplace(e4, Vector2D(99, 88));
    TEST("Emplace works", ref.transform.position == Vector2D(99, 88));
    TEST("Size after emplace", storage.size() == 1);

    // Iteration over dense array
    Entity e5 = em.createEntity();
    Entity e6 = em.createEntity();
    storage.add(e5, TransformComponent(Vector2D(1, 1)));
    storage.add(e6, TransformComponent(Vector2D(2, 2)));

    int count = 0;
    for (auto it = storage.begin(); it != storage.end(); ++it) count++;
    TEST("Iteration count = 3", count == 3);
}

// ═══════════════════════════════════════════════════════════════
// Tests de ECSCoordinator
// ═══════════════════════════════════════════════════════════════
void testCoordinator() {
    std::cout << "\n=== ECSCoordinator Tests ===" << std::endl;

    ECSCoordinator ecs;

    // Register components
    ComponentType tType = ecs.registerComponent<TransformComponent>();
    ComponentType pType = ecs.registerComponent<PhysicsComponent>();
    ComponentType cType = ecs.registerComponent<ColliderComponent>();
    ComponentType sType = ecs.registerComponent<SpriteComponent>();

    TEST("Transform type = 0", tType == 0);
    TEST("Physics type = 1", pType == 1);
    TEST("Collider type = 2", cType == 2);
    TEST("Sprite type = 3", sType == 3);

    // Create entity
    Entity player = ecs.createEntity();
    TEST("Player created", ecs.isAlive(player));

    // Add components
    ecs.addComponent<TransformComponent>(player, TransformComponent(Vector2D(100, 200)));
    ecs.addComponent<PhysicsComponent>(player, PhysicsComponent(1.0f));

    TEST("Has transform", ecs.hasComponent<TransformComponent>(player));
    TEST("Has physics", ecs.hasComponent<PhysicsComponent>(player));
    TEST("No collider", !ecs.hasComponent<ColliderComponent>(player));

    // Get component
    auto& tf = ecs.getComponent<TransformComponent>(player);
    TEST("Get transform pos", tf.transform.position == Vector2D(100, 200));

    auto& phys = ecs.getComponent<PhysicsComponent>(player);
    TEST("Get physics mass", MathUtils::approxEqual(phys.mass, 1.0f));

    // Modify
    tf.transform.position = Vector2D(300, 400);
    TEST("Modified transform", ecs.getComponent<TransformComponent>(player).transform.position == Vector2D(300, 400));

    // Tags
    ecs.setTag(player, TAG_PLAYER);
    TEST("Player tag", ecs.hasTag(player, TAG_PLAYER));

    // Signature matching
    auto sig = ecs.buildSignature<TransformComponent, PhysicsComponent>();
    TEST("Player matches phys sig", ecs.matchesSignature(player, sig));

    auto renderSig = ecs.buildSignature<TransformComponent, SpriteComponent>();
    TEST("Player no render sig", !ecs.matchesSignature(player, renderSig));

    // Create platform (static)
    Entity platform = ecs.createEntity();
    ecs.addComponent<TransformComponent>(platform, TransformComponent(Vector2D(400, 500)));
    ecs.addComponent<ColliderComponent>(platform, ColliderComponent(Vector2D(200, 20), true));
    ecs.addComponent<SpriteComponent>(platform, SpriteComponent(Color::green(), 200, 20));

    TEST("Platform alive", ecs.isAlive(platform));
    TEST("Platform collider static", ecs.getComponent<ColliderComponent>(platform).isStatic);

    // Destroy entity
    ecs.destroyEntity(player);
    TEST("Player destroyed", !ecs.isAlive(player));
    TEST("No stale transform", !ecs.hasComponent<TransformComponent>(player));

    // Generational: recreate with same index
    Entity newEntity = ecs.createEntity();
    TEST("New entity reuses index", getIndex(newEntity) == getIndex(player));
    TEST("New entity different gen", getGeneration(newEntity) != getGeneration(player));
    TEST("Old player still dead", !ecs.isAlive(player));

    // Multiple entities
    std::vector<Entity> many;
    for (int i = 0; i < 50; i++) {
        Entity e = ecs.createEntity();
        ecs.addComponent<TransformComponent>(e, TransformComponent(Vector2D(static_cast<float>(i), 0)));
        many.push_back(e);
    }
    TEST("50 entities created", ecs.getActiveEntityCount() >= 51);

    // Destroy half
    for (int i = 0; i < 25; i++) {
        ecs.destroyEntity(many[i]);
    }
    TEST("25 destroyed", ecs.getActiveEntityCount() >= 26);

    // Remaining entities still accessible
    bool allOk = true;
    for (int i = 25; i < 50; i++) {
        if (!ecs.isAlive(many[i]) || !ecs.hasComponent<TransformComponent>(many[i])) {
            allOk = false;
            break;
        }
    }
    TEST("Remaining entities intact", allOk);
}

// ═══════════════════════════════════════════════════════════════
// Tests de Swap-and-Pop integrity
// ═══════════════════════════════════════════════════════════════
void testSwapAndPop() {
    std::cout << "\n=== Swap-and-Pop Integrity ===" << std::endl;

    EntityManager em;
    ComponentStorage<PhysicsComponent> storage;

    // Create 5 entities with incrementing mass
    Entity entities[5];
    for (int i = 0; i < 5; i++) {
        entities[i] = em.createEntity();
        PhysicsComponent p;
        p.mass = static_cast<float>(i + 1);
        storage.add(entities[i], p);
    }
    TEST("5 components", storage.size() == 5);

    // Remove middle element (index 2, mass=3)
    storage.remove(entities[2]);
    TEST("Size after remove = 4", storage.size() == 4);
    TEST("E2 gone", !storage.has(entities[2]));

    // All remaining values correct
    TEST("E0 intact (mass=1)", MathUtils::approxEqual(storage.get(entities[0]).mass, 1.0f));
    TEST("E1 intact (mass=2)", MathUtils::approxEqual(storage.get(entities[1]).mass, 2.0f));
    TEST("E3 intact (mass=4)", MathUtils::approxEqual(storage.get(entities[3]).mass, 4.0f));
    TEST("E4 intact (mass=5)", MathUtils::approxEqual(storage.get(entities[4]).mass, 5.0f));

    // Remove first
    storage.remove(entities[0]);
    TEST("E0 gone", !storage.has(entities[0]));
    TEST("Size = 3", storage.size() == 3);

    // Remove last
    storage.remove(entities[4]);
    TEST("E4 gone", !storage.has(entities[4]));
    TEST("Size = 2", storage.size() == 2);

    // Remaining: E1 (mass=2) and E3 (mass=4)
    TEST("E1 still ok", MathUtils::approxEqual(storage.get(entities[1]).mass, 2.0f));
    TEST("E3 still ok", MathUtils::approxEqual(storage.get(entities[3]).mass, 4.0f));
}

// ═══════════════════════════════════════════════════════════════
// Tests de Generational ID safety
// ═══════════════════════════════════════════════════════════════
void testGenerationalSafety() {
    std::cout << "\n=== Generational ID Safety ===" << std::endl;

    ECSCoordinator ecs;
    ecs.registerComponent<TransformComponent>();

    // Create and destroy multiple times
    Entity first = ecs.createEntity();
    ecs.addComponent<TransformComponent>(first, TransformComponent(Vector2D(1, 1)));

    // Save the old entity handle
    Entity staleRef = first;

    ecs.destroyEntity(first);

    // New entity reuses same index
    Entity second = ecs.createEntity();
    ecs.addComponent<TransformComponent>(second, TransformComponent(Vector2D(2, 2)));

    // Stale reference should NOT be alive
    TEST("Stale ref is dead", !ecs.isAlive(staleRef));
    TEST("New entity is alive", ecs.isAlive(second));
    TEST("Same index", getIndex(staleRef) == getIndex(second));
    TEST("Different generation", getGeneration(staleRef) != getGeneration(second));

    // Multiple generations
    for (int i = 0; i < 10; i++) {
        Entity e = ecs.createEntity();
        ecs.addComponent<TransformComponent>(e, TransformComponent(Vector2D(static_cast<float>(i), 0)));
        Entity saved = e;
        ecs.destroyEntity(e);
        TEST("Gen " + std::to_string(i) + " stale", !ecs.isAlive(saved));
    }
}

// ═══════════════════════════════════════════════════════════════
int main() {
    std::cout << "============================================" << std::endl;
    std::cout << "  PhysicsEngine2D — Tests ECS" << std::endl;
    std::cout << "============================================" << std::endl;

    testEntityManager();
    testComponentStorage();
    testCoordinator();
    testSwapAndPop();
    testGenerationalSafety();

    std::cout << "\n============================================" << std::endl;
    std::cout << "  Resultados: " << passed << " pasados, " << failed << " fallidos" << std::endl;
    std::cout << "============================================" << std::endl;

    return (failed > 0) ? 1 : 0;
}
