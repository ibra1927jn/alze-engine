#include "LinearAllocator.h"
#include "PoolAllocator.h"
#include "Vector2D.h"
#include "MathUtils.h"
#include "SimdConfig.h"
#include <iostream>
#include <chrono>
#include <vector>

using namespace engine::memory;
using namespace engine::math;

int passed = 0;
int failed = 0;

#define TEST(name, expr) \
    if (expr) { passed++; std::cout << "  [OK] " << name << std::endl; } \
    else { failed++; std::cerr << "  [FAIL] " << name << std::endl; }

// ═══════════════════════════════════════════════════════════════
// Tests de LinearAllocator
// ═══════════════════════════════════════════════════════════════
void testLinearAllocator() {
    std::cout << "\n=== LinearAllocator Tests ===" << std::endl;

    LinearAllocator arena(1024);
    TEST("Total = 1024", arena.getTotal() == 1024);
    TEST("Used = 0", arena.getUsed() == 0);
    TEST("Remaining = 1024", arena.getRemaining() == 1024);

    // Asignar memoria
    void* p1 = arena.allocate(64);
    TEST("Allocate 64 bytes", p1 != nullptr);
    TEST("Used >= 64", arena.getUsed() >= 64);

    void* p2 = arena.allocate(128);
    TEST("Allocate 128 bytes", p2 != nullptr);
    TEST("p1 != p2", p1 != p2);

    // Crear un Vector2D en la arena
    Vector2D* v = arena.create<Vector2D>(3.0f, 4.0f);
    TEST("Create Vector2D", v != nullptr);
    TEST("Vector value", MathUtils::approxEqual(v->x, 3.0f) && MathUtils::approxEqual(v->y, 4.0f));
    TEST("Vector operations work", MathUtils::approxEqual(v->magnitude(), 5.0f));

    // Alineación SIMD (16 bytes)
    void* aligned = arena.allocate(32, 16);
    TEST("Aligned alloc", aligned != nullptr);
    TEST("16-byte aligned", reinterpret_cast<uintptr_t>(aligned) % 16 == 0);

    // Reset
    arena.reset();
    TEST("Reset: used = 0", arena.getUsed() == 0);
    TEST("Reset: remaining = total", arena.getRemaining() == arena.getTotal());

    // Asignar después de reset (reutiliza la misma memoria)
    void* p3 = arena.allocate(64);
    TEST("Post-reset alloc works", p3 != nullptr);

    // Overflow: intentar asignar más de lo que hay
    LinearAllocator tiny(32);
    void* ok = tiny.allocate(16);
    TEST("Small arena alloc ok", ok != nullptr);
    void* fail = tiny.allocate(32);
    TEST("Overflow returns nullptr", fail == nullptr);
}

// ═══════════════════════════════════════════════════════════════
// Tests de PoolAllocator
// ═══════════════════════════════════════════════════════════════
void testPoolAllocator() {
    std::cout << "\n=== PoolAllocator Tests ===" << std::endl;

    PoolAllocator pool(sizeof(Vector2D), 100);
    TEST("Block count = 100", pool.getBlockCount() == 100);
    TEST("Free count = 100", pool.getFreeCount() == 100);
    TEST("isEmpty", pool.isEmpty());
    TEST("!isFull", !pool.isFull());

    // Asignar un bloque
    void* b1 = pool.allocate();
    TEST("Allocate block", b1 != nullptr);
    TEST("Used = 1", pool.getUsedCount() == 1);
    TEST("Free = 99", pool.getFreeCount() == 99);

    // Crear un Vector2D en el pool
    Vector2D* v1 = pool.create<Vector2D>(10.0f, 20.0f);
    TEST("Create Vector2D", v1 != nullptr);
    TEST("Vector value", MathUtils::approxEqual(v1->x, 10.0f));
    TEST("Used = 2", pool.getUsedCount() == 2);

    // Liberar y reusar
    pool.destroy(v1);
    TEST("After destroy: used = 1", pool.getUsedCount() == 1);

    Vector2D* v2 = pool.create<Vector2D>(30.0f, 40.0f);
    TEST("Reused block after destroy", v2 != nullptr);
    TEST("Reused value correct", MathUtils::approxEqual(v2->x, 30.0f));

    // Llenar todo el pool
    std::vector<void*> blocks;
    pool.reset();
    for (size_t i = 0; i < 100; i++) {
        void* b = pool.allocate();
        TEST("Fill pool block " + std::to_string(i), b != nullptr);
        blocks.push_back(b);
    }
    TEST("Pool is full", pool.isFull());

    // Overflow
    void* overflow = pool.allocate();
    TEST("Overflow returns nullptr", overflow == nullptr);

    // Liberar uno y volver a asignar
    pool.deallocate(blocks[50]);
    void* reused = pool.allocate();
    TEST("Reuse after dealloc", reused != nullptr);

    // Reset
    pool.reset();
    TEST("After reset: isEmpty", pool.isEmpty());
    TEST("After reset: free = 100", pool.getFreeCount() == 100);
}

// ═══════════════════════════════════════════════════════════════
// SIMD Verification
// ═══════════════════════════════════════════════════════════════
void testSIMD() {
    std::cout << "\n=== SIMD Verification ===" << std::endl;
    std::cout << "  SIMD: " << simdName() << std::endl;
    TEST("SIMD detectado", hasSIMD());
    TEST("sizeof(Vector2D) >= 8", sizeof(Vector2D) >= 8);

    // Verificar que las operaciones SIMD dan los mismos resultados
    Vector2D a(3.0f, 4.0f);
    Vector2D b(1.0f, 2.0f);

    TEST("SIMD add", (a + b) == Vector2D(4.0f, 6.0f));
    TEST("SIMD sub", (a - b) == Vector2D(2.0f, 2.0f));
    TEST("SIMD mul scalar", (a * 2.0f) == Vector2D(6.0f, 8.0f));
    TEST("SIMD div scalar", (a / 2.0f) == Vector2D(1.5f, 2.0f));
    TEST("SIMD dot", MathUtils::approxEqual(a.dot(b), 11.0f));
    TEST("SIMD magnitude", MathUtils::approxEqual(a.magnitude(), 5.0f));
    TEST("SIMD lerp", Vector2D::lerp(a, b, 0.5f) == Vector2D(2.0f, 3.0f));
    TEST("SIMD negation", (-a) == Vector2D(-3.0f, -4.0f));

#if ENGINE_SIMD_SSE2
    TEST("SIMD aligned (16-byte)", alignof(Vector2D) >= 16);
#endif
}

// ═══════════════════════════════════════════════════════════════
// Benchmark: SIMD vs conceptual scalar
// ═══════════════════════════════════════════════════════════════
void benchmarkSIMD() {
    std::cout << "\n=== SIMD Benchmark ===" << std::endl;

    const int ITERATIONS = 1'000'000;

    // Benchmark: sum de vectores
    auto start = std::chrono::high_resolution_clock::now();
    Vector2D acc(0, 0);
    for (int i = 0; i < ITERATIONS; i++) {
        Vector2D v(static_cast<float>(i), static_cast<float>(i + 1));
        acc += v;
        acc = acc * 0.999f;
    }
    auto end = std::chrono::high_resolution_clock::now();

    float ms = std::chrono::duration<float, std::milli>(end - start).count();
    std::cout << "  " << ITERATIONS << " vector ops: " << ms << " ms"
              << " (SIMD: " << simdName() << ")" << std::endl;
    std::cout << "  Result: " << acc << " (prevents optimization)" << std::endl;
    TEST("Benchmark completed", ms < 1000.0f);
}

// ═══════════════════════════════════════════════════════════════
int main() {
    std::cout << "============================================" << std::endl;
    std::cout << "  PhysicsEngine2D — Tests de Memoria y SIMD" << std::endl;
    std::cout << "============================================" << std::endl;

    testLinearAllocator();
    testPoolAllocator();
    testSIMD();
    benchmarkSIMD();

    std::cout << "\n============================================" << std::endl;
    std::cout << "  Resultados: " << passed << " pasados, " << failed << " fallidos" << std::endl;
    std::cout << "============================================" << std::endl;

    return (failed > 0) ? 1 : 0;
}
