#include "StateManager.h"
#include "SpatialHash.h"
#include "Serializer.h"
#include "Vector2D.h"
#include "AABB.h"
#include "MathUtils.h"
#include <iostream>
#include <fstream>
#include <chrono>

using namespace engine::core;
using namespace engine::physics;
using namespace engine::math;

int passed = 0;
int failed = 0;

#define TEST(name, expr) \
    if (expr) { passed++; std::cout << "  [OK] " << name << std::endl; } \
    else { failed++; std::cerr << "  [FAIL] " << name << std::endl; }

// ═══════════════════════════════════════════════════════════════
// StateManager Tests
// ═══════════════════════════════════════════════════════════════

class TestPlayState : public IGameState {
public:
    const char* getName() const override { return "Play"; }
    bool shouldUpdatePhysics() const override { return true; }
    void update(float dt) override { updateCount++; (void)dt; }
    void render(float alpha) override { renderCount++; (void)alpha; }
    void onEnter() override { entered = true; }
    void onExit() override { exited = true; }
    void onPause() override { paused = true; }
    void onResume() override { resumed = true; }

    int updateCount = 0, renderCount = 0;
    bool entered = false, exited = false, paused = false, resumed = false;
};

class TestPauseState : public IGameState {
public:
    const char* getName() const override { return "Pause"; }
    bool shouldUpdatePhysics() const override { return false; }
    bool isTransparent() const override { return true; }
    void update(float dt) override { updateCount++; (void)dt; }
    void render(float alpha) override { renderCount++; (void)alpha; }

    int updateCount = 0, renderCount = 0;
};

void testStateManager() {
    std::cout << "\n=== StateManager Tests ===" << std::endl;

    StateManager sm;
    TEST("Initial empty", sm.empty());
    TEST("Initial depth 0", sm.depth() == 0);
    TEST("Current null", sm.current() == nullptr);

    // Push PlayState
    auto* playPtr = new TestPlayState();
    sm.push(std::unique_ptr<IGameState>(playPtr));

    TEST("Not empty after push", !sm.empty());
    TEST("Depth 1", sm.depth() == 1);
    TEST("Current = Play", std::string(sm.currentName()) == "Play");
    TEST("Play entered", playPtr->entered);
    TEST("UpdatePhysics = true", sm.shouldUpdatePhysics());

    // Update + Render
    sm.update(0.016f);
    sm.render(0.5f);
    TEST("Play updated", playPtr->updateCount == 1);
    TEST("Play rendered", playPtr->renderCount == 1);

    // Push PauseState (Play gets paused)
    auto* pausePtr = new TestPauseState();
    sm.push(std::unique_ptr<IGameState>(pausePtr));

    TEST("Play paused", playPtr->paused);
    TEST("Depth 2", sm.depth() == 2);
    TEST("Current = Pause", std::string(sm.currentName()) == "Pause");
    TEST("UpdatePhysics = false", !sm.shouldUpdatePhysics());

    // Render with transparency (both visible)
    sm.render(0.5f);
    TEST("Play rendered through", playPtr->renderCount == 2);
    TEST("Pause rendered", pausePtr->renderCount == 1);

    // Pop PauseState (Play resumes)
    sm.pop();
    TEST("Play resumed", playPtr->resumed);
    TEST("Depth 1 again", sm.depth() == 1);
    TEST("Current = Play again", std::string(sm.currentName()) == "Play");
    TEST("Physics back on", sm.shouldUpdatePhysics());

    // Change state
    auto* playPtr2 = new TestPlayState();
    sm.change(std::unique_ptr<IGameState>(playPtr2));
    TEST("Old play exited", playPtr->exited);
    TEST("New play entered", playPtr2->entered);
    TEST("Depth still 1", sm.depth() == 1);

    // Clear
    sm.clear();
    TEST("Cleared", sm.empty());
    TEST("New play exited", playPtr2->exited);
}

// ═══════════════════════════════════════════════════════════════
// SpatialHash Tests
// ═══════════════════════════════════════════════════════════════
void testSpatialHash() {
    std::cout << "\n=== SpatialHash Tests ===" << std::endl;

    SpatialHash grid(100.0f);  // 100px cells

    // Insert entities
    AABB boxA = AABB::fromCenter({50, 50}, {10, 10});
    AABB boxB = AABB::fromCenter({60, 50}, {10, 10});
    AABB boxC = AABB::fromCenter({500, 500}, {10, 10});

    grid.insert(1, boxA);
    grid.insert(2, boxB);
    grid.insert(3, boxC);

    TEST("Entity count = 3", grid.getEntityCount() == 3);
    TEST("Cells used >= 1", grid.getCellCount() >= 1);

    // Query near A should find A and B (same cell)
    auto near = grid.query(boxA);
    TEST("Query A finds >= 2", near.size() >= 2);

    // Query near C should not find A or B
    auto farAway = grid.query(boxC);
    bool hasA = false, hasB = false;
    for (auto id : farAway) {
        if (id == 1) hasA = true;
        if (id == 2) hasB = true;
    }
    TEST("Far query no A", !hasA);
    TEST("Far query no B", !hasB);

    // GetPotentialPairs
    auto pairs = grid.getPotentialPairs();
    bool foundAB = false;
    for (auto& p : pairs) {
        if ((p.first == 1 && p.second == 2) || (p.first == 2 && p.second == 1))
            foundAB = true;
    }
    TEST("Pair A-B found", foundAB);

    // Clear
    grid.clear();
    TEST("Clear resets", grid.getEntityCount() == 0);
    TEST("Clear cells", grid.getCellCount() == 0);

    // ── Benchmark: 10K entities ─────────────────────────────────
    SpatialHash bigGrid(128.0f);
    const int N = 10000;

    auto t1 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; i++) {
        float x = static_cast<float>(i % 100) * 50.0f;
        float y = static_cast<float>(i / 100) * 50.0f;
        bigGrid.insert(static_cast<uint32_t>(i), AABB::fromCenter({x, y}, {20, 20}));
    }
    auto t2 = std::chrono::high_resolution_clock::now();
    float insertMs = std::chrono::duration<float, std::milli>(t2 - t1).count();

    auto bigPairs = bigGrid.getPotentialPairs();
    auto t3 = std::chrono::high_resolution_clock::now();
    float pairMs = std::chrono::duration<float, std::milli>(t3 - t2).count();

    // Brute force comparison
    int bruteForceComparisons = N * (N - 1) / 2;  // ~50M

    std::cout << "  [INFO] 10K insert: " << insertMs << "ms" << std::endl;
    std::cout << "  [INFO] 10K getPairs: " << pairMs << "ms, "
              << bigPairs.size() << " pairs (vs " << bruteForceComparisons << " brute)" << std::endl;

    TEST("10K insert ok", bigGrid.getEntityCount() == N);
    TEST("Spatial reduces pairs", static_cast<int>(bigPairs.size()) < bruteForceComparisons);
    TEST("Insert < 10ms", insertMs < 10.0f);
}

// ═══════════════════════════════════════════════════════════════
// Serializer (JSON Writer/Reader) Tests
// ═══════════════════════════════════════════════════════════════
void testSerializer() {
    std::cout << "\n=== Serializer Tests ===" << std::endl;

    // ── Writer ─────────────────────────────────────────────────
    JsonWriter w;
    w.beginObject();
    w.keyValue("name", std::string("TestScene"));
    w.keyValue("version", 1);
    w.keyValue("gravity", 9.81f);
    w.keyValue("active", true);
    w.keyValue("pos", Vector2D(10.5f, 20.3f));

    w.beginArray("items");
    w.value(1);
    w.value(2);
    w.value(3);
    w.endArray();

    w.key("nested");
    w.beginObject();
    w.keyValue("x", 42);
    w.endObject();

    w.endObject();

    std::string json = w.toString();
    TEST("JSON not empty", !json.empty());
    TEST("Has name", json.find("TestScene") != std::string::npos);
    TEST("Has gravity", json.find("9.81") != std::string::npos);
    TEST("Has items", json.find("items") != std::string::npos);

    // ── Save to file ───────────────────────────────────────────
    bool saved = w.saveToFile("test_output.json");
    TEST("File saved", saved);

    // ── Reader ─────────────────────────────────────────────────
    JsonReader r;
    TEST("Load from string", r.loadFromString(json));

    r.expectChar('{');

    std::string k1 = r.readKey();
    TEST("Key = name", k1 == "name");
    std::string v1 = r.readString();
    TEST("Value = TestScene", v1 == "TestScene");
    r.skipComma();

    std::string k2 = r.readKey();
    TEST("Key = version", k2 == "version");
    int v2 = r.readInt();
    TEST("Value = 1", v2 == 1);
    r.skipComma();

    std::string k3 = r.readKey();
    TEST("Key = gravity", k3 == "gravity");
    float v3 = r.readFloat();
    TEST("Value ~= 9.81", MathUtils::approxEqual(v3, 9.81f, 0.01f));
    r.skipComma();

    std::string k4 = r.readKey();
    TEST("Key = active", k4 == "active");
    bool v4 = r.readBool();
    TEST("Value = true", v4);
    r.skipComma();

    // Vector2D
    std::string k5 = r.readKey();
    TEST("Key = pos", k5 == "pos");
    Vector2D v5 = r.readVector2D();
    TEST("Vec x ~= 10.5", MathUtils::approxEqual(v5.x, 10.5f, 0.1f));
    TEST("Vec y ~= 20.3", MathUtils::approxEqual(v5.y, 20.3f, 0.1f));
    r.skipComma();

    // Array
    std::string k6 = r.readKey();
    TEST("Key = items", k6 == "items");
    r.expectChar('[');
    int arr1 = r.readInt(); r.skipComma();
    int arr2 = r.readInt(); r.skipComma();
    int arr3 = r.readInt();
    TEST("Array [1,2,3]", arr1 == 1 && arr2 == 2 && arr3 == 3);
    r.expectChar(']');
    r.skipComma();

    // File read
    JsonReader fileReader;
    TEST("Load from file", fileReader.loadFromFile("test_output.json"));

    // Cleanup
    std::remove("test_output.json");
}

// ═══════════════════════════════════════════════════════════════
int main() {
    std::cout << "============================================" << std::endl;
    std::cout << "  PhysicsEngine2D — Tests Phase 2.5" << std::endl;
    std::cout << "============================================" << std::endl;

    testStateManager();
    testSpatialHash();
    testSerializer();

    std::cout << "\n============================================" << std::endl;
    std::cout << "  Resultados: " << passed << " pasados, " << failed << " fallidos" << std::endl;
    std::cout << "============================================" << std::endl;

    return (failed > 0) ? 1 : 0;
}
