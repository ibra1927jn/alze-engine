#include "NavMesh.h"

namespace engine {
namespace ai {

void NavPolygon::computeCentroid() {
    centroid = math::Vector2D::Zero;
    if (vertices.empty()) return;
    for (const auto& v : vertices) centroid = centroid + v;
    centroid = centroid * (1.0f / static_cast<float>(vertices.size()));
}

bool NavPolygon::containsPoint(const math::Vector2D& p) const {
    int n = static_cast<int>(vertices.size());
    bool inside = false;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        if (((vertices[i].y > p.y) != (vertices[j].y > p.y)) &&
            (p.x < (vertices[j].x - vertices[i].x) * (p.y - vertices[i].y) /
             (vertices[j].y - vertices[i].y) + vertices[i].x)) {
            inside = !inside;
        }
    }
    return inside;
}

uint32_t NavMesh::addPolygon(const std::vector<math::Vector2D>& vertices) {
    NavPolygon poly;
    poly.vertices = vertices;
    poly.computeCentroid();
    m_polygons.push_back(poly);
    return static_cast<uint32_t>(m_polygons.size() - 1);
}

void NavMesh::build() {
    uint32_t count = static_cast<uint32_t>(m_polygons.size());
    for (uint32_t i = 0; i < count; i++) {
        m_polygons[i].neighbors.clear();
        for (uint32_t j = i + 1; j < count; j++) {
            if (sharesEdge(m_polygons[i], m_polygons[j])) {
                m_polygons[i].neighbors.push_back(j);
                m_polygons[j].neighbors.push_back(i);
            }
        }
    }
    m_built = true;
}

std::vector<math::Vector2D> NavMesh::findPath(const math::Vector2D& start, const math::Vector2D& goal) const {
    if (!m_built || m_polygons.empty()) return {};

    int startPoly = findPolygon(start);
    int goalPoly = findPolygon(goal);

    if (startPoly < 0 || goalPoly < 0) return {};
    if (startPoly == goalPoly) return {start, goal};

    struct Node {
        uint32_t polyIdx;
        float gCost;
        float fCost;
        bool operator>(const Node& o) const { return fCost > o.fCost; }
    };

    uint32_t count = static_cast<uint32_t>(m_polygons.size());
    std::vector<float> gCost(count, 1e30f);
    std::vector<int> cameFrom(count, -1);
    std::vector<bool> closed(count, false);

    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;

    gCost[startPoly] = 0;
    float h = (m_polygons[goalPoly].centroid - m_polygons[startPoly].centroid).magnitude();
    open.push({static_cast<uint32_t>(startPoly), 0, h});

    while (!open.empty()) {
        Node current = open.top(); open.pop();
        if (current.polyIdx == static_cast<uint32_t>(goalPoly)) break;
        if (closed[current.polyIdx]) continue;
        closed[current.polyIdx] = true;

        for (uint32_t neighbor : m_polygons[current.polyIdx].neighbors) {
            if (closed[neighbor] || m_polygons[neighbor].blocked) continue;

            float edgeCost = (m_polygons[neighbor].centroid - m_polygons[current.polyIdx].centroid).magnitude();
            float newG = gCost[current.polyIdx] + edgeCost;

            if (newG < gCost[neighbor]) {
                gCost[neighbor] = newG;
                cameFrom[neighbor] = static_cast<int>(current.polyIdx);
                float heuristic = (m_polygons[goalPoly].centroid - m_polygons[neighbor].centroid).magnitude();
                open.push({neighbor, newG, newG + heuristic});
            }
        }
    }

    if (cameFrom[goalPoly] < 0 && startPoly != goalPoly) return {};

    std::vector<math::Vector2D> path;
    path.push_back(goal);

    int current = goalPoly;
    while (current != startPoly && cameFrom[current] >= 0) {
        current = cameFrom[current];
        path.push_back(m_polygons[current].centroid);
    }
    path.push_back(start);

    std::reverse(path.begin(), path.end());
    return smoothPath(path);
}

void NavMesh::setBlocked(uint32_t polyIdx, bool blocked) {
    if (polyIdx < m_polygons.size()) m_polygons[polyIdx].blocked = blocked;
}

int NavMesh::findPolygon(const math::Vector2D& point) const {
    for (uint32_t i = 0; i < m_polygons.size(); i++) {
        if (m_polygons[i].containsPoint(point)) return static_cast<int>(i);
    }
    return -1;
}

bool NavMesh::sharesEdge(const NavPolygon& a, const NavPolygon& b) const {
    int shared = 0;
    for (const auto& va : a.vertices) {
        for (const auto& vb : b.vertices) {
            if ((va - vb).sqrMagnitude() < 0.01f) {
                shared++;
                if (shared >= 2) return true;
            }
        }
    }
    return false;
}

std::vector<math::Vector2D> NavMesh::smoothPath(const std::vector<math::Vector2D>& raw) const {
    if (raw.size() <= 2) return raw;

    std::vector<math::Vector2D> smooth;
    smooth.push_back(raw[0]);

    size_t current = 0;
    while (current < raw.size() - 1) {
        size_t farthest = current + 1;
        for (size_t test = raw.size() - 1; test > current + 1; test--) {
            if (isLineWalkable(raw[current], raw[test])) {
                farthest = test;
                break;
            }
        }
        smooth.push_back(raw[farthest]);
        current = farthest;
    }

    return smooth;
}

bool NavMesh::isLineWalkable(const math::Vector2D& a, const math::Vector2D& b) const {
    int samples = static_cast<int>((b - a).magnitude() / 2.0f) + 2;
    for (int i = 0; i <= samples; i++) {
        float t = static_cast<float>(i) / samples;
        math::Vector2D p = math::Vector2D::lerp(a, b, t);
        if (findPolygon(p) < 0) return false;
    }
    return true;
}

void GridNav::init(int width, int height, float cellSize) {
    m_width = width;
    m_height = height;
    m_cellSize = cellSize;
    m_walkable.resize(width * height, true);
}

void GridNav::setWalkable(int x, int y, bool walkable) {
    if (x >= 0 && x < m_width && y >= 0 && y < m_height)
        m_walkable[y * m_width + x] = walkable;
}

bool GridNav::isWalkable(int x, int y) const {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) return false;
    return m_walkable[y * m_width + x];
}

std::vector<math::Vector2D> GridNav::findPath(math::Vector2D start, math::Vector2D goal) const {
    int sx = static_cast<int>(start.x / m_cellSize);
    int sy = static_cast<int>(start.y / m_cellSize);
    int gx = static_cast<int>(goal.x / m_cellSize);
    int gy = static_cast<int>(goal.y / m_cellSize);

    if (!isWalkable(sx, sy) || !isWalkable(gx, gy)) return {};

    struct Node {
        int x, y;
        float g, f;
        bool operator>(const Node& o) const { return f > o.f; }
    };

    int total = m_width * m_height;
    std::vector<float> gCost(total, 1e30f);
    std::vector<int> cameFrom(total, -1);
    std::vector<bool> closed(total, false);
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;

    auto idx = [this](int x, int y) { return y * m_width + x; };
    auto heuristic = [](int x1, int y1, int x2, int y2) -> float {
        return std::sqrt(static_cast<float>((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1)));
    };

    gCost[idx(sx, sy)] = 0;
    open.push({sx, sy, 0, heuristic(sx, sy, gx, gy)});

    static const int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    static const int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};
    static const float dcost[] = {1.414f, 1, 1.414f, 1, 1, 1.414f, 1, 1.414f};

    while (!open.empty()) {
        Node cur = open.top(); open.pop();
        if (cur.x == gx && cur.y == gy) break;
        int ci = idx(cur.x, cur.y);
        if (closed[ci]) continue;
        closed[ci] = true;

        for (int d = 0; d < 8; d++) {
            int nx = cur.x + dx[d], ny = cur.y + dy[d];
            if (!isWalkable(nx, ny)) continue;
            int ni = idx(nx, ny);
            if (closed[ni]) continue;

            if (dx[d] != 0 && dy[d] != 0) {
                if (!isWalkable(cur.x + dx[d], cur.y) ||
                    !isWalkable(cur.x, cur.y + dy[d])) continue;
            }

            float newG = gCost[ci] + dcost[d];
            if (newG < gCost[ni]) {
                gCost[ni] = newG;
                cameFrom[ni] = ci;
                open.push({nx, ny, newG, newG + heuristic(nx, ny, gx, gy)});
            }
        }
    }

    int gi = idx(gx, gy);
    if (cameFrom[gi] < 0 && !(sx == gx && sy == gy)) return {};

    std::vector<math::Vector2D> path;
    int current = gi;
    while (current >= 0) {
        int cx = current % m_width;
        int cy = current / m_width;
        path.push_back({(cx + 0.5f) * m_cellSize, (cy + 0.5f) * m_cellSize});
        current = cameFrom[current];
    }
    std::reverse(path.begin(), path.end());
    return path;
}

} // namespace ai
} // namespace engine
