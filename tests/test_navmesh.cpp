#include "ai/NavMesh.h"
#include <iostream>
#include <cmath>

using namespace engine::math;
using namespace engine::ai;

int passed = 0;
int failed = 0;

#define TEST(name, expr) \
    if (expr) { passed++; std::cout << "  [OK] " << name << std::endl; } \
    else { failed++; std::cerr << "  [FAIL] " << name << std::endl; }

#define APPROX(a, b) (std::abs((a) - (b)) < 0.1f)

// ═══════════════════════════════════════════════════════════════
// NavPolygon
// ═══════════════════════════════════════════════════════════════
void testNavPolygon() {
    std::cout << "\n=== NavPolygon ===" << std::endl;

    NavPolygon poly;
    poly.vertices = {{0,0}, {10,0}, {10,10}, {0,10}};
    poly.computeCentroid();

    TEST("Centroid x", APPROX(poly.centroid.x, 5.0f));
    TEST("Centroid y", APPROX(poly.centroid.y, 5.0f));

    TEST("Contains center", poly.containsPoint({5, 5}));
    TEST("Contains corner", poly.containsPoint({1, 1}));
    TEST("Not contains outside", !poly.containsPoint({-1, 5}));
    TEST("Not contains far", !poly.containsPoint({20, 20}));

    TEST("Default not blocked", !poly.blocked);
}

// ═══════════════════════════════════════════════════════════════
// NavMesh
// ═══════════════════════════════════════════════════════════════
void testNavMesh() {
    std::cout << "\n=== NavMesh ===" << std::endl;

    NavMesh mesh;

    // Two adjacent squares sharing edge x=10
    // Poly 0: (0,0)-(10,0)-(10,10)-(0,10)
    // Poly 1: (10,0)-(20,0)-(20,10)-(10,10)
    uint32_t p0 = mesh.addPolygon({{0,0}, {10,0}, {10,10}, {0,10}});
    uint32_t p1 = mesh.addPolygon({{10,0}, {20,0}, {20,10}, {10,10}});

    TEST("addPolygon returns 0", p0 == 0);
    TEST("addPolygon returns 1", p1 == 1);
    TEST("Polygon count", mesh.getPolygonCount() == 2);
    TEST("Not built yet", !mesh.isBuilt());

    mesh.build();
    TEST("Built", mesh.isBuilt());

    // Neighbors connected
    const NavPolygon& np0 = mesh.getPolygon(0);
    const NavPolygon& np1 = mesh.getPolygon(1);
    bool p0HasP1 = false, p1HasP0 = false;
    for (auto n : np0.neighbors) if (n == 1) p0HasP1 = true;
    for (auto n : np1.neighbors) if (n == 0) p1HasP0 = true;
    TEST("Poly 0 neighbor 1", p0HasP1);
    TEST("Poly 1 neighbor 0", p1HasP0);

    // findPolygon
    TEST("findPolygon center of 0", mesh.findPolygon({5, 5}) == 0);
    TEST("findPolygon center of 1", mesh.findPolygon({15, 5}) == 1);
    TEST("findPolygon outside", mesh.findPolygon({-5, 5}) == -1);

    // Pathfinding from poly 0 to poly 1
    auto path = mesh.findPath({5, 5}, {15, 5});
    TEST("Path found", !path.empty());
    if (!path.empty()) {
        TEST("Path starts near start", APPROX(path.front().x, 5.0f) && APPROX(path.front().y, 5.0f));
        TEST("Path ends near goal", APPROX(path.back().x, 15.0f) && APPROX(path.back().y, 5.0f));
    }

    // Blocked polygon
    mesh.setBlocked(1, true);
    auto blockedPath = mesh.findPath({5, 5}, {15, 5});
    TEST("Blocked path empty", blockedPath.empty());

    // Unblock
    mesh.setBlocked(1, false);
    auto unblocked = mesh.findPath({5, 5}, {15, 5});
    TEST("Unblocked path found", !unblocked.empty());
}

// ═══════════════════════════════════════════════════════════════
// NavMesh multi-polygon pathfinding
// ═══════════════════════════════════════════════════════════════
void testNavMeshLong() {
    std::cout << "\n=== NavMesh Long Path ===" << std::endl;

    NavMesh mesh;
    // Chain of 5 squares: 0-10, 10-20, 20-30, 30-40, 40-50
    for (int i = 0; i < 5; i++) {
        float x = static_cast<float>(i * 10);
        mesh.addPolygon({{x,0}, {x+10,0}, {x+10,10}, {x,10}});
    }
    mesh.build();

    TEST("5 polygons", mesh.getPolygonCount() == 5);

    auto path = mesh.findPath({5, 5}, {45, 5});
    TEST("Long path found", !path.empty());
    if (!path.empty()) {
        TEST("Long path start", APPROX(path.front().x, 5.0f));
        TEST("Long path end", APPROX(path.back().x, 45.0f));
    }

    // Block middle polygon, path should fail
    mesh.setBlocked(2, true);
    auto blocked = mesh.findPath({5, 5}, {45, 5});
    TEST("Blocked middle = no path", blocked.empty());
}

// ═══════════════════════════════════════════════════════════════
// GridNav
// ═══════════════════════════════════════════════════════════════
void testGridNav() {
    std::cout << "\n=== GridNav ===" << std::endl;

    GridNav grid;
    grid.init(5, 5, 1.0f);

    TEST("Grid width", grid.getWidth() == 5);
    TEST("Grid height", grid.getHeight() == 5);
    TEST("Default walkable", grid.isWalkable(0, 0));

    grid.setWalkable(2, 2, false);
    TEST("Set blocked", !grid.isWalkable(2, 2));

    // Path from (0,0) to (4,4) avoiding (2,2)
    auto path = grid.findPath({0.5f, 0.5f}, {4.5f, 4.5f});
    TEST("Grid path found", !path.empty());
    if (!path.empty()) {
        TEST("Grid path starts near start", path.front().x < 2.0f && path.front().y < 2.0f);
        TEST("Grid path ends near goal", path.back().x > 3.0f && path.back().y > 3.0f);
    }

    // Wall blocking all paths
    GridNav grid2;
    grid2.init(5, 5, 1.0f);
    for (int y = 0; y < 5; y++) grid2.setWalkable(2, y, false);
    auto blocked = grid2.findPath({0.5f, 2.5f}, {4.5f, 2.5f});
    TEST("Wall blocks path", blocked.empty());
}

// ═══════════════════════════════════════════════════════════════
int main() {
    testNavPolygon();
    testNavMesh();
    testNavMeshLong();
    testGridNav();

    std::cout << "\n============================================" << std::endl;
    std::cout << "  Resultados: " << passed << " pasados, " << failed << " fallidos" << std::endl;
    std::cout << "============================================" << std::endl;
    return failed > 0 ? 1 : 0;
}
