#include "EventBus.h"
#include "Profiler.h"
#include "ResourceManager.h"
#include "Serializer.h"
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
// Serializer Tests
// ═══════════════════════════════════════════════════════════════
void testSerializer() {
    std::cout << "\n=== Serializer Tests ===" << std::endl;

    // JsonWriter round-trip
    {
        JsonWriter w;
        w.beginObject();
        w.keyValue("name", std::string("player"));
        w.keyValue("health", 100);
        w.keyValue("speed", 3.5f);
        w.keyValue("alive", true);
        w.keyValue("pos", Vector2D(1.0f, 2.0f));
        w.endObject();

        std::string json = w.toString();
        TEST("Writer produces output", !json.empty());
        TEST("Writer contains name", json.find("\"name\"") != std::string::npos);
        TEST("Writer contains health", json.find("100") != std::string::npos);

        JsonReader r;
        r.loadFromString(json);
        TEST("Reader loads string", true);

        r.expectChar('{');
        std::string k1 = r.readKey();
        TEST("Key 1 = name", k1 == "name");
        std::string v1 = r.readString();
        TEST("Value 1 = player", v1 == "player");
        r.skipComma();

        std::string k2 = r.readKey();
        TEST("Key 2 = health", k2 == "health");
        int v2 = r.readInt();
        TEST("Value 2 = 100", v2 == 100);
        r.skipComma();

        std::string k3 = r.readKey();
        TEST("Key 3 = speed", k3 == "speed");
        float v3 = r.readFloat();
        TEST("Value 3 ~ 3.5", MathUtils::approxEqual(v3, 3.5f, 0.01f));
        r.skipComma();

        std::string k4 = r.readKey();
        TEST("Key 4 = alive", k4 == "alive");
        bool v4 = r.readBool();
        TEST("Value 4 = true", v4 == true);
        r.skipComma();

        std::string k5 = r.readKey();
        TEST("Key 5 = pos", k5 == "pos");
        Vector2D v5 = r.readVector2D();
        TEST("Pos x = 1", MathUtils::approxEqual(v5.x, 1.0f));
        TEST("Pos y = 2", MathUtils::approxEqual(v5.y, 2.0f));
    }

    // Regression: readFloat con input invalido no crashea (P0 fix -fno-exceptions)
    {
        JsonReader r;
        r.loadFromString("abc");
        float val = r.readFloat();
        TEST("Invalid float = 0", MathUtils::approxEqual(val, 0.0f));
    }

    // readBool con input invalido
    {
        JsonReader r;
        r.loadFromString("xyz");
        bool val = r.readBool();
        TEST("Invalid bool = false", val == false);
    }

    // Cadena vacia
    {
        JsonReader r;
        r.loadFromString("");
        TEST("Empty has no more", !r.hasMore());
        float val = r.readFloat();
        TEST("Empty readFloat = 0", MathUtils::approxEqual(val, 0.0f));
    }

    // Strings con escape
    {
        JsonWriter w;
        w.beginObject();
        w.keyValue("msg", std::string("hello\\nworld"));
        w.endObject();
        std::string json = w.toString();
        TEST("Escaped string in output", json.find("hello\\nworld") != std::string::npos);
    }

    // Array
    {
        JsonWriter w;
        w.beginArray();
        w.value(1);
        w.value(2);
        w.value(3);
        w.endArray();
        std::string json = w.toString();
        TEST("Array output", json.find("[") != std::string::npos);

        JsonReader r;
        r.loadFromString(json);
        r.expectChar('[');
        int a = r.readInt(); r.skipComma();
        int b = r.readInt(); r.skipComma();
        int c = r.readInt();
        TEST("Array val 1", a == 1);
        TEST("Array val 2", b == 2);
        TEST("Array val 3", c == 3);
        r.expectChar(']');
    }

    // skipValue
    {
        JsonReader r;
        r.loadFromString("{\"a\": 42, \"b\": \"hello\"}");
        r.expectChar('{');
        std::string k = r.readKey();
        r.skipValue(); // skip 42
        r.skipComma();
        std::string k2 = r.readKey();
        TEST("Skip value reads next key", k2 == "b");
        std::string v = r.readString();
        TEST("Value after skip", v == "hello");
    }

    // Negative & scientific float
    {
        JsonReader r;
        r.loadFromString("-3.14");
        float val = r.readFloat();
        TEST("Negative float", MathUtils::approxEqual(val, -3.14f, 0.01f));
    }
    {
        JsonReader r;
        r.loadFromString("1.5e2");
        float val = r.readFloat();
        TEST("Scientific float", MathUtils::approxEqual(val, 150.0f, 0.1f));
    }
}

// ═══════════════════════════════════════════════════════════════
int main() {
    std::cout << "============================================" << std::endl;
    std::cout << "  PhysicsEngine2D — Tests Subsystems" << std::endl;
    std::cout << "============================================" << std::endl;

    testEventBus();
    testProfiler();
    testResourceManager();
    testSerializer();

    std::cout << "\n============================================" << std::endl;
    std::cout << "  Resultados: " << passed << " pasados, " << failed << " fallidos" << std::endl;
    std::cout << "============================================" << std::endl;

    return (failed > 0) ? 1 : 0;
}
