#include "EventBus.h"
#include "Profiler.h"
#include "ResourceManager.h"
#include "Vector2D.h"
#include "MathUtils.h"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

using namespace engine::core;
using namespace engine::math;

int passed = 0;
int failed = 0;

#define TEST(name, expr) \
    if (expr) { passed++; std::cout << "  [OK] " << name << std::endl; } \
    else { failed++; std::cerr << "  [FAIL] " << name << std::endl; }

// ═══════════════════════════════════════════════════════════════
// EventBus Tests
// ═══════════════════════════════════════════════════════════════
void testEventBus() {
    std::cout << "\n=== EventBus Tests ===" << std::endl;

    EventBus bus;

    // Subscribe + Emit
    int collisionCount = 0;
    float lastImpulse = 0;

    bus.subscribe<CollisionEvent>([&](const CollisionEvent& e) {
        collisionCount++;
        lastImpulse = e.impulse;
    });

    bus.emit(CollisionEvent{1, 2, 150.0f, 1.0f, 0.0f});
    TEST("Collision received", collisionCount == 1);
    TEST("Impulse value", MathUtils::approxEqual(lastImpulse, 150.0f));

    bus.emit(CollisionEvent{3, 4, 200.0f, 0.0f, -1.0f});
    TEST("Second collision", collisionCount == 2);
    TEST("Second impulse", MathUtils::approxEqual(lastImpulse, 200.0f));

    // Multiple subscribers
    int destroyedCount = 0;
    uint32_t lastDestroyed = 0;

    bus.subscribe<EntityDestroyedEvent>([&](const EntityDestroyedEvent& e) {
        destroyedCount++;
        lastDestroyed = e.entity;
    });

    int destroyedCount2 = 0;
    bus.subscribe<EntityDestroyedEvent>([&](const EntityDestroyedEvent& e) {
        (void)e;
        destroyedCount2++;
    });

    bus.emit(EntityDestroyedEvent{42});
    TEST("Destroy sub1", destroyedCount == 1);
    TEST("Destroy sub2", destroyedCount2 == 1);
    TEST("Destroy entity", lastDestroyed == 42);

    // Player state
    bool playerGrounded = false;
    bus.subscribe<PlayerStateEvent>([&](const PlayerStateEvent& e) {
        playerGrounded = e.onGround;
    });

    bus.emit(PlayerStateEvent{true, false, 100.0f});
    TEST("Player grounded", playerGrounded);

    bus.emit(PlayerStateEvent{false, true, 0.0f});
    TEST("Player airborne", !playerGrounded);

    // Debug event
    std::string debugMsg;
    bus.subscribe<DebugEvent>([&](const DebugEvent& e) {
        debugMsg = e.message;
    });

    bus.emit(DebugEvent{"test_message", 3.14f});
    TEST("Debug message", debugMsg == "test_message");

    // Total emitted
    TEST("Total emitted = 6", bus.getTotalEmitted() == 6);

    // No subscribers = no crash
    struct UnusedEvent { int x; };
    bus.emit(UnusedEvent{99});
    TEST("No crash on empty", bus.getTotalEmitted() == 7);
}

// ═══════════════════════════════════════════════════════════════
// Profiler Tests
// ═══════════════════════════════════════════════════════════════
void testProfiler() {
    std::cout << "\n=== Profiler Tests ===" << std::endl;

    // Basic timing
    Profiler::beginFrame();

    Profiler::begin("TestSection");
    // Simular trabajo
    volatile int x = 0;
    for (int i = 0; i < 100000; i++) x += i;
    Profiler::end("TestSection");

    Profiler::endFrame();

    auto& metric = Profiler::getMetric("TestSection");
    TEST("LastMs > 0", metric.lastMs > 0.0f);
    TEST("AvgMs > 0", metric.avgMs > 0.0f);
    TEST("Calls = 1", metric.calls == 1);

    auto& frameMet = Profiler::getFrameMetric();
    TEST("Frame lastMs > 0", frameMet.lastMs > 0.0f);

    // Multiple frames
    for (int i = 0; i < 10; i++) {
        Profiler::beginFrame();
        Profiler::begin("Loop");
        volatile int y = 0;
        for (int j = 0; j < 10000; j++) y += j;
        Profiler::end("Loop");
        Profiler::endFrame();
    }

    auto& loopMetric = Profiler::getMetric("Loop");
    TEST("Loop calls = 10", loopMetric.calls == 10);

    // Frame history
    auto history = Profiler::getFrameHistory();
    TEST("History has data", history.size() > 0);
    TEST("History values > 0", history.back() > 0.0f);

    // Draw calls
    Profiler::addDrawCall(5);
    TEST("Draw calls = 5", Profiler::getDrawCalls() == 5);

    // Memory
    Profiler::setMemoryUsage(1024 * 1024);
    TEST("Memory = 1MB", Profiler::getMemoryUsage() == 1024 * 1024);

    // Section names
    auto names = Profiler::getSectionNames();
    TEST("Has sections", names.size() >= 2);

    // Report
    std::string report = Profiler::generateReport();
    TEST("Report not empty", !report.empty());
    TEST("Report has PROFILER", report.find("PROFILER") != std::string::npos);
}

// ═══════════════════════════════════════════════════════════════
// ResourceManager Tests
// ═══════════════════════════════════════════════════════════════
struct MockResource {
    std::string data;
    static int loadCount;
    MockResource(const std::string& d) : data(d) { loadCount++; }
};
int MockResource::loadCount = 0;

void testResourceManager() {
    std::cout << "\n=== ResourceManager Tests ===" << std::endl;

    ResourceManager<MockResource> mgr;
    MockResource::loadCount = 0;

    mgr.setLoader([](const std::string& id) {
        return std::make_shared<MockResource>("data_" + id);
    });

    // First load
    auto r1 = mgr.get("texture_player");
    TEST("First load", r1 != nullptr);
    TEST("Correct data", r1->data == "data_texture_player");
    TEST("Load count = 1", MockResource::loadCount == 1);

    // Cache hit
    auto r2 = mgr.get("texture_player");
    TEST("Cache hit same ptr", r1.get() == r2.get());
    TEST("Load count still 1", MockResource::loadCount == 1);
    TEST("Cache hits = 1", mgr.getCacheHits() == 1);

    // Different resource
    auto r3 = mgr.get("texture_enemy");
    TEST("Different resource", r3->data == "data_texture_enemy");
    TEST("Load count = 2", MockResource::loadCount == 2);

    // has()
    TEST("Has player", mgr.has("texture_player"));
    TEST("Has enemy", mgr.has("texture_enemy"));
    TEST("No ghost", !mgr.has("ghost"));

    // Stats
    TEST("Cache size = 2", mgr.getCacheSize() == 2);
    TEST("Total loads = 2", mgr.getTotalLoads() == 2);

    // Insert manually — must hold shared_ptr alive (cache uses weak_ptr)
    auto manualRes = std::make_shared<MockResource>("manual_data");
    mgr.insert("manual", manualRes);
    TEST("Has manual", mgr.has("manual"));
    TEST("Manual data", mgr.get("manual")->data == "manual_data");

    // Garbage collection (all alive — we hold r1, r2, r3, manualRes)
    int freed = mgr.collectGarbage();
    TEST("No GC (all alive)", freed == 0);

    // Let a resource expire
    {
        auto temp = mgr.get("temporary");
        TEST("Temp loaded", temp != nullptr);
    }
    // temp went out of scope -> weak_ptr expired
    freed = mgr.collectGarbage();
    TEST("GC freed temp", freed == 1);

    // Hit rate
    float rate = mgr.getHitRate();
    TEST("Hit rate > 0", rate > 0.0f);

    // Clear
    mgr.clear();
    TEST("Cleared", mgr.getCacheSize() == 0);
}

// ═══════════════════════════════════════════════════════════════
int main() {
    std::cout << "============================================" << std::endl;
    std::cout << "  PhysicsEngine2D — Tests Subsystems" << std::endl;
    std::cout << "============================================" << std::endl;

    testEventBus();
    testProfiler();
    testResourceManager();

    std::cout << "\n============================================" << std::endl;
    std::cout << "  Resultados: " << passed << " pasados, " << failed << " fallidos" << std::endl;
    std::cout << "============================================" << std::endl;

    return (failed > 0) ? 1 : 0;
}
