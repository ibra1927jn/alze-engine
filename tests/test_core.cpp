#include "FrameAllocator.h"
#include "StateManager.h"
#include <iostream>
#include <string>
#include <vector>
#include <cstring>

using namespace engine::core;

int passed = 0;
int failed = 0;

#define TEST(name, expr) \
    if (expr) { passed++; std::cout << "  [OK] " << name << std::endl; } \
    else { failed++; std::cerr << "  [FAIL] " << name << std::endl; }

// ================================================================
// FrameAllocator Tests
// ================================================================
void testFrameAllocator() {
    std::cout << "\n=== FrameAllocator Tests ===" << std::endl;

    // Init
    FrameAllocator::init(1024);
    TEST("isInitialized", FrameAllocator::isInitialized());
    TEST("capacity = 1024", FrameAllocator::getCapacity() == 1024);
    TEST("used = 0 after init", FrameAllocator::getUsed() == 0);
    TEST("remaining = capacity", FrameAllocator::getRemaining() == 1024);

    // Basic alloc
    void* p1 = FrameAllocator::alloc(64);
    TEST("alloc returns non-null", p1 != nullptr);
    TEST("used > 0 after alloc", FrameAllocator::getUsed() >= 64);
    TEST("allocCount = 1", FrameAllocator::getAllocCount() == 1);

    // Typed alloc
    int* ints = FrameAllocator::alloc<int>(10);
    TEST("typed alloc non-null", ints != nullptr);
    // Write and read back
    for (int i = 0; i < 10; i++) ints[i] = i * 7;
    bool intDataOk = true;
    for (int i = 0; i < 10; i++) {
        if (ints[i] != i * 7) { intDataOk = false; break; }
    }
    TEST("typed alloc read/write", intDataOk);
    TEST("allocCount = 2", FrameAllocator::getAllocCount() == 2);

    // Zeroed alloc
    void* z = FrameAllocator::allocZeroed(32);
    TEST("allocZeroed non-null", z != nullptr);
    uint8_t* zBytes = static_cast<uint8_t*>(z);
    bool allZero = true;
    for (int i = 0; i < 32; i++) {
        if (zBytes[i] != 0) { allZero = false; break; }
    }
    TEST("allocZeroed is zero-filled", allZero);

    // Peak usage tracks
    size_t peakBefore = FrameAllocator::getPeakUsage();
    TEST("peak > 0", peakBefore > 0);

    // Reset
    FrameAllocator::reset();
    TEST("used = 0 after reset", FrameAllocator::getUsed() == 0);
    TEST("allocCount = 0 after reset", FrameAllocator::getAllocCount() == 0);
    TEST("peak preserved after reset", FrameAllocator::getPeakUsage() == peakBefore);
    TEST("totalAllocCount preserved", FrameAllocator::getTotalAllocCount() == 3);

    // Alloc after reset reuses memory
    void* p2 = FrameAllocator::alloc(64);
    TEST("alloc after reset works", p2 != nullptr);

    // Overflow returns nullptr
    FrameAllocator::reset();
    void* big = FrameAllocator::alloc(2048);
    TEST("overflow returns nullptr", big == nullptr);
    TEST("used unchanged on overflow", FrameAllocator::getUsed() == 0);

    // Alignment
    FrameAllocator::reset();
    FrameAllocator::alloc(1); // 1 byte to misalign
    void* aligned = FrameAllocator::alloc(16, 16);
    TEST("16-byte aligned", (reinterpret_cast<uintptr_t>(aligned) % 16) == 0);

    // Usage percent
    FrameAllocator::reset();
    FrameAllocator::alloc(512);
    float pct = FrameAllocator::getUsagePercent();
    TEST("usage ~50%", pct >= 49.0f && pct <= 51.0f);

    // Shutdown
    FrameAllocator::shutdown();
    TEST("not initialized after shutdown", !FrameAllocator::isInitialized());

    // Re-init for FrameArray tests
    FrameAllocator::init(4096);
}

void testFrameArray() {
    std::cout << "\n=== FrameArray Tests ===" << std::endl;

    FrameAllocator::reset();

    // Construction
    FrameArray<int> arr(16);
    TEST("empty on create", arr.empty());
    TEST("size = 0", arr.size() == 0);
    TEST("capacity = 16", arr.capacity() == 16);

    // push_back
    arr.push_back(10);
    arr.push_back(20);
    arr.push_back(30);
    TEST("size = 3 after pushes", arr.size() == 3);
    TEST("not empty", !arr.empty());
    TEST("operator[] read", arr[0] == 10 && arr[1] == 20 && arr[2] == 30);

    // back
    TEST("back = 30", arr.back() == 30);

    // pop_back
    arr.pop_back();
    TEST("size = 2 after pop", arr.size() == 2);
    TEST("back = 20 after pop", arr.back() == 20);

    // clear
    arr.clear();
    TEST("size = 0 after clear", arr.size() == 0);
    TEST("empty after clear", arr.empty());

    // Iteration
    arr.push_back(1);
    arr.push_back(2);
    arr.push_back(3);
    int sum = 0;
    for (int v : arr) sum += v;
    TEST("range-for iteration", sum == 6);

    // begin/end pointer arithmetic
    TEST("end - begin = size", arr.end() - arr.begin() == 3);

    // Capacity limit (should not crash, just ignore)
    FrameArray<int> small(2);
    small.push_back(1);
    small.push_back(2);
    small.push_back(3); // over capacity
    TEST("push over capacity ignored", small.size() == 2);

    // Pop on empty (should not crash)
    FrameArray<int> emptyArr(4);
    emptyArr.pop_back(); // no-op
    TEST("pop on empty = no crash", emptyArr.size() == 0);

    FrameAllocator::shutdown();
}

// ================================================================
// StateManager Tests
// ================================================================

// Test state that tracks lifecycle calls
struct TestState : public IGameState {
    std::string name;
    std::vector<std::string>& log;
    bool transparent;

    TestState(const std::string& n, std::vector<std::string>& l, bool trans = false)
        : name(n), log(l), transparent(trans) {}

    const char* getName() const override { return name.c_str(); }
    void onEnter() override { log.push_back(name + ":enter"); }
    void onExit() override { log.push_back(name + ":exit"); }
    void onPause() override { log.push_back(name + ":pause"); }
    void onResume() override { log.push_back(name + ":resume"); }
    void update(float dt) override { log.push_back(name + ":update"); (void)dt; }
    void render(float alpha) override { log.push_back(name + ":render"); (void)alpha; }
    void handleInput(float dt) override { log.push_back(name + ":input"); (void)dt; }
    bool isTransparent() const override { return transparent; }
};

void testStateManager() {
    std::cout << "\n=== StateManager Tests ===" << std::endl;

    // Empty state
    {
        StateManager sm;
        TEST("empty initially", sm.empty());
        TEST("depth = 0", sm.depth() == 0);
        TEST("current = nullptr", sm.current() == nullptr);
        TEST("currentName = None", std::string(sm.currentName()) == "None");
        // These should not crash on empty
        sm.update(0.016f);
        sm.render(1.0f);
        sm.handleInput(0.016f);
        sm.pop();
        TEST("operations on empty no crash", true);
        TEST("shouldUpdatePhysics = false when empty", !sm.shouldUpdatePhysics());
    }

    // Push
    {
        std::vector<std::string> log;
        StateManager sm;
        sm.push(std::make_unique<TestState>("Menu", log));
        TEST("push: depth = 1", sm.depth() == 1);
        TEST("push: current name", std::string(sm.currentName()) == "Menu");
        TEST("push: onEnter called", log.back() == "Menu:enter");
        TEST("push: not empty", !sm.empty());
    }

    // Push pauses previous
    {
        std::vector<std::string> log;
        StateManager sm;
        sm.push(std::make_unique<TestState>("Play", log));
        sm.push(std::make_unique<TestState>("Pause", log));
        TEST("push2: depth = 2", sm.depth() == 2);
        TEST("push2: current = Pause", std::string(sm.currentName()) == "Pause");
        // Log should be: Play:enter, Play:pause, Pause:enter
        TEST("push2: play paused", log[1] == "Play:pause");
        TEST("push2: pause entered", log[2] == "Pause:enter");
    }

    // Pop resumes previous
    {
        std::vector<std::string> log;
        StateManager sm;
        sm.push(std::make_unique<TestState>("Play", log));
        sm.push(std::make_unique<TestState>("Pause", log));
        log.clear();
        sm.pop();
        TEST("pop: depth = 1", sm.depth() == 1);
        TEST("pop: current = Play", std::string(sm.currentName()) == "Play");
        // Log should be: Pause:exit, Play:resume
        TEST("pop: 2 events logged", log.size() == 2);
        if (log.size() >= 2) {
            TEST("pop: pause exited", log[0] == "Pause:exit");
            TEST("pop: play resumed", log[1] == "Play:resume");
        }
    }

    // Change replaces top
    {
        std::vector<std::string> log;
        StateManager sm;
        sm.push(std::make_unique<TestState>("Menu", log));
        log.clear();
        sm.change(std::make_unique<TestState>("Play", log));
        TEST("change: depth = 1", sm.depth() == 1);
        TEST("change: current = Play", std::string(sm.currentName()) == "Play");
        TEST("change: 2 events logged", log.size() == 2);
        if (log.size() >= 2) {
            TEST("change: menu exited", log[0] == "Menu:exit");
            TEST("change: play entered", log[1] == "Play:enter");
        }
    }

    // Update/render/input dispatch to top
    {
        std::vector<std::string> log;
        StateManager sm;
        sm.push(std::make_unique<TestState>("Game", log));
        log.clear();
        sm.update(0.016f);
        sm.render(1.0f);
        sm.handleInput(0.016f);
        TEST("dispatch: 3 events", log.size() == 3);
        if (log.size() >= 3) {
            TEST("dispatch: update", log[0] == "Game:update");
            TEST("dispatch: render", log[1] == "Game:render");
            TEST("dispatch: input", log[2] == "Game:input");
        }
    }

    // Transparent rendering (draws states below)
    {
        std::vector<std::string> log;
        StateManager sm;
        sm.push(std::make_unique<TestState>("Play", log, false));
        sm.push(std::make_unique<TestState>("HUD", log, true));
        log.clear();
        sm.render(1.0f);
        // Both should render, Play first then HUD
        TEST("transparent: 2 renders", log.size() == 2);
        if (log.size() >= 2) {
            TEST("transparent: play renders first", log[0] == "Play:render");
            TEST("transparent: hud renders second", log[1] == "HUD:render");
        }
    }

    // Multiple transparent layers
    {
        std::vector<std::string> log;
        StateManager sm;
        sm.push(std::make_unique<TestState>("World", log, false));
        sm.push(std::make_unique<TestState>("HUD", log, true));
        sm.push(std::make_unique<TestState>("Dialog", log, true));
        log.clear();
        sm.render(1.0f);
        TEST("multi-transparent: 3 renders", log.size() == 3);
        if (log.size() >= 3) {
            TEST("multi-transparent: order", log[0] == "World:render" && log[1] == "HUD:render" && log[2] == "Dialog:render");
        }
    }

    // All transparent (edge case - should still render all)
    {
        std::vector<std::string> log;
        StateManager sm;
        sm.push(std::make_unique<TestState>("A", log, true));
        sm.push(std::make_unique<TestState>("B", log, true));
        log.clear();
        sm.render(1.0f);
        TEST("all-transparent: renders all", log.size() == 2);
    }

    // Clear
    {
        std::vector<std::string> log;
        StateManager sm;
        sm.push(std::make_unique<TestState>("A", log));
        sm.push(std::make_unique<TestState>("B", log));
        sm.push(std::make_unique<TestState>("C", log));
        log.clear();
        sm.clear();
        TEST("clear: empty", sm.empty());
        TEST("clear: depth = 0", sm.depth() == 0);
        // All should have gotten onExit (in reverse order)
        TEST("clear: 3 exits", log.size() == 3);
        if (log.size() >= 3) {
            TEST("clear: C exits first", log[0] == "C:exit");
            TEST("clear: B exits second", log[1] == "B:exit");
            TEST("clear: A exits last", log[2] == "A:exit");
        }
    }

    // shouldUpdatePhysics
    {
        struct NoPhysicsState : public IGameState {
            const char* getName() const override { return "NoPhysics"; }
            void update(float) override {}
            void render(float) override {}
            bool shouldUpdatePhysics() const override { return false; }
        };

        StateManager sm;
        std::vector<std::string> log;
        sm.push(std::make_unique<TestState>("Play", log));
        TEST("shouldUpdatePhysics default true", sm.shouldUpdatePhysics());

        sm.push(std::make_unique<NoPhysicsState>());
        TEST("shouldUpdatePhysics = false", !sm.shouldUpdatePhysics());

        sm.pop();
        TEST("shouldUpdatePhysics restored", sm.shouldUpdatePhysics());
    }
}

// ================================================================
// Main
// ================================================================
int main() {
    std::cout << "============================================" << std::endl;
    std::cout << "  Test Core (FrameAllocator + StateManager)" << std::endl;
    std::cout << "============================================" << std::endl;

    testFrameAllocator();
    testFrameArray();
    testStateManager();

    std::cout << "\n============================================" << std::endl;
    std::cout << "  Resultados: " << passed << " pasados, " << failed << " fallidos" << std::endl;
    std::cout << "============================================" << std::endl;
    return failed > 0 ? 1 : 0;
}
