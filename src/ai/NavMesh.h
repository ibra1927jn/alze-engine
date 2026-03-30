#pragma once

#include "math/Vector2D.h"
#include "math/MathUtils.h"
#include <vector>
#include <queue>
#include <cstdint>

namespace engine {
namespace ai {

struct NavPolygon {
    std::vector<math::Vector2D> vertices;
    math::Vector2D centroid;
    bool blocked = false;
    std::vector<uint32_t> neighbors;

    void computeCentroid();
    bool containsPoint(const math::Vector2D& p) const;
};

class NavMesh {
public:
    uint32_t addPolygon(const std::vector<math::Vector2D>& vertices);
    void build();
    std::vector<math::Vector2D> findPath(const math::Vector2D& start, const math::Vector2D& goal) const;
    void setBlocked(uint32_t polyIdx, bool blocked);
    int findPolygon(const math::Vector2D& point) const;

    uint32_t getPolygonCount() const { return static_cast<uint32_t>(m_polygons.size()); }
    const NavPolygon& getPolygon(uint32_t i) const { return m_polygons[i]; }
    bool isBuilt() const { return m_built; }

private:
    bool sharesEdge(const NavPolygon& a, const NavPolygon& b) const;
    std::vector<math::Vector2D> smoothPath(const std::vector<math::Vector2D>& raw) const;
    bool isLineWalkable(const math::Vector2D& a, const math::Vector2D& b) const;
    void buildSpatialGrid();
    int findPolygonGrid(const math::Vector2D& point) const;

    std::vector<NavPolygon> m_polygons;
    bool m_built = false;

    // Spatial acceleration grid for O(1) findPolygon
    std::vector<std::vector<uint32_t>> m_grid;
    math::Vector2D m_gridMin;
    float m_gridCellSize = 0.0f;
    int m_gridW = 0, m_gridH = 0;
};

class GridNav {
public:
    void init(int width, int height, float cellSize);
    void setWalkable(int x, int y, bool walkable);
    bool isWalkable(int x, int y) const;
    std::vector<math::Vector2D> findPath(math::Vector2D start, math::Vector2D goal) const;

    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }

private:
    std::vector<bool> m_walkable;
    int m_width = 0, m_height = 0;
    float m_cellSize = 1.0f;
};

} // namespace ai
} // namespace engine
