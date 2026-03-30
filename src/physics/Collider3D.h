#pragma once

#include "math/Vector3D.h"
#include <array>
#include <vector> // Added for std::vector in HeightfieldCollider

namespace engine {
namespace physics {

struct Ray3D {
    math::Vector3D origin;
    math::Vector3D direction;
    Ray3D() = default;
    Ray3D(const math::Vector3D& o, const math::Vector3D& d) : origin(o), direction(d.normalized()) {}
    math::Vector3D pointAt(float t) const { return origin + direction * t; }
};

struct RayHit3D {
    bool  hit = false;
    float distance = 0;
    math::Vector3D point;
    math::Vector3D normal;
};

// Base class for all colliders
struct Collider3D {
    enum class Shape { SPHERE, AABB, OBB, CAPSULE, HEIGHTFIELD, CONVEX_HULL };
    Shape type;
    virtual ~Collider3D() = default;
};

struct AABB3D : public Collider3D {
    math::Vector3D min, max;
    AABB3D() { type = Shape::AABB; }
    AABB3D(const math::Vector3D& mn, const math::Vector3D& mx) : min(mn), max(mx) { type = Shape::AABB; }
    static AABB3D fromCenterHalf(const math::Vector3D& center, const math::Vector3D& half) {
        return AABB3D(center - half, center + half);
    }
    math::Vector3D center() const { return (min + max) * 0.5f; }
    math::Vector3D extents() const { return (max - min) * 0.5f; }
    bool overlaps(const AABB3D& other) const {
        return min.x <= other.max.x && max.x >= other.min.x &&
               min.y <= other.max.y && max.y >= other.min.y &&
               min.z <= other.max.z && max.z >= other.min.z;
    }
    bool contains(const math::Vector3D& p) const {
        return p.x >= min.x && p.x <= max.x &&
               p.y >= min.y && p.y <= max.y &&
               p.z >= min.z && p.z <= max.z;
    }
    bool contains(const AABB3D& other) const {
        return min.x <= other.min.x && max.x >= other.max.x &&
               min.y <= other.min.y && max.y >= other.max.y &&
               min.z <= other.min.z && max.z >= other.max.z;
    }
    float surfaceArea() const {
        math::Vector3D d = max - min;
        return 2.0f * (d.x * d.y + d.y * d.z + d.z * d.x);
    }
    void merge(const AABB3D& other) {
        min.x = std::min(min.x, other.min.x);
        min.y = std::min(min.y, other.min.y);
        min.z = std::min(min.z, other.min.z);
        max.x = std::max(max.x, other.max.x);
        max.y = std::max(max.y, other.max.y);
        max.z = std::max(max.z, other.max.z);
    }
    math::Vector3D closestPoint(const math::Vector3D& p) const {
        float cx = p.x < min.x ? min.x : (p.x > max.x ? max.x : p.x);
        float cy = p.y < min.y ? min.y : (p.y > max.y ? max.y : p.y);
        float cz = p.z < min.z ? min.z : (p.z > max.z ? max.z : p.z);
        return math::Vector3D(cx, cy, cz);
    }
    math::Vector3D getSupport(const math::Vector3D& dir) const {
        math::Vector3D result = center();
        math::Vector3D ext = extents();
        result.x += (dir.x > 0.0f) ? ext.x : -ext.x;
        result.y += (dir.y > 0.0f) ? ext.y : -ext.y;
        result.z += (dir.z > 0.0f) ? ext.z : -ext.z;
        return result;
    }
};

struct SphereCollider : public Collider3D {
    math::Vector3D center;
    float radius;
    SphereCollider() : radius(0.5f) { type = Shape::SPHERE; }
    SphereCollider(const math::Vector3D& c, float r) : center(c), radius(r) { type = Shape::SPHERE; }
    bool overlaps(const SphereCollider& other) const {
        math::Vector3D d = center - other.center;
        float rSum = radius + other.radius;
        return (d.x*d.x + d.y*d.y + d.z*d.z) < rSum * rSum;
    }
    math::Vector3D getSupport(const math::Vector3D& dir) const {
        return center + dir.normalized() * radius;
    }
};

struct CapsuleCollider : public Collider3D {
    math::Vector3D center;
    float height, radius;
    math::Vector3D axis;
    CapsuleCollider() : height(2.0f), radius(0.5f), axis(0, 1, 0) { type = Shape::CAPSULE; }
    CapsuleCollider(const math::Vector3D& c, float h, float r,
                    const math::Vector3D& ax = math::Vector3D(0,1,0))
        : center(c), height(h), radius(r), axis(ax.normalized()) { type = Shape::CAPSULE; }
    math::Vector3D pointA() const { return center - axis * (height * 0.5f - radius); }
    math::Vector3D pointB() const { return center + axis * (height * 0.5f - radius); }
    AABB3D getAABB() const {
        math::Vector3D a = pointA(), b = pointB();
        return AABB3D(
            math::Vector3D(std::min(a.x,b.x)-radius, std::min(a.y,b.y)-radius, std::min(a.z,b.z)-radius),
            math::Vector3D(std::max(a.x,b.x)+radius, std::max(a.y,b.y)+radius, std::max(a.z,b.z)+radius)
        );
    }
    math::Vector3D getSupport(const math::Vector3D& dir) const {
        math::Vector3D pA = pointA();
        math::Vector3D pB = pointB();
        math::Vector3D extreme = (dir.dot(pA) > dir.dot(pB)) ? pA : pB;
        return extreme + dir.normalized() * radius;
    }
};

// ── Segment utilities ────────────────────────────────────────────
math::Vector3D closestPointOnSegment(const math::Vector3D& A,
                                     const math::Vector3D& B,
                                     const math::Vector3D& P);
void closestPointsSegmentSegment(
    const math::Vector3D& p1, const math::Vector3D& q1,
    const math::Vector3D& p2, const math::Vector3D& q2,
    math::Vector3D& outA, math::Vector3D& outB);

// ── Contact info ─────────────────────────────────────────────────
struct ContactInfo {
    bool     hasContact = false;
    math::Vector3D normal;
    float    penetration = 0;
    math::Vector3D contactPoint;
};

// ── OBB ──────────────────────────────────────────────────────────
struct OBB3D : public Collider3D {
    math::Vector3D center;
    math::Vector3D halfExtents;
    math::Vector3D axes[3];
    OBB3D() { axes[0]={1,0,0}; axes[1]={0,1,0}; axes[2]={0,0,1}; type = Shape::OBB; }
    OBB3D(const math::Vector3D& c, const math::Vector3D& he,
          const math::Vector3D& ax0, const math::Vector3D& ax1, const math::Vector3D& ax2)
        : center(c), halfExtents(he) { axes[0]=ax0; axes[1]=ax1; axes[2]=ax2; type = Shape::OBB; }
    
    math::Vector3D getSupport(const math::Vector3D& dir) const {
        math::Vector3D result = center;
        result += axes[0] * (dir.dot(axes[0]) > 0.0f ? halfExtents.x : -halfExtents.x);
        result += axes[1] * (dir.dot(axes[1]) > 0.0f ? halfExtents.y : -halfExtents.y);
        result += axes[2] * (dir.dot(axes[2]) > 0.0f ? halfExtents.z : -halfExtents.z);
        return result;
    }
};

struct HeightfieldCollider : public Collider3D {
    std::vector<float> heights;
    int numRows = 0;
    int numCols = 0;
    float rowSpacing = 1.0f;
    float colSpacing = 1.0f;
    float minHeight = 0.0f;
    float maxHeight = 0.0f;

    HeightfieldCollider() { type = Shape::HEIGHTFIELD; }

    void recomputeBounds() {
        if (heights.empty()) return;
        minHeight = heights[0];
        maxHeight = heights[0];
        for (float h : heights) {
            if (h < minHeight) minHeight = h;
            if (h > maxHeight) maxHeight = h;
        }
    }

    float getHeightAt(int row, int col) const {
        if (row < 0 || row >= numRows || col < 0 || col >= numCols) return 0.0f;
        return heights[row * numCols + col];
    }
};

// ── Collision detection declarations ─────────────────────────────
ContactInfo sphereVsSphere(const SphereCollider& a, const SphereCollider& b);
ContactInfo sphereVsAABB(const SphereCollider& sphere, const AABB3D& box);
ContactInfo aabbVsAABB(const AABB3D& a, const AABB3D& b);
ContactInfo obbVsOBB(const OBB3D& a, const OBB3D& b);
ContactInfo obbVsSphere(const OBB3D& obb, const SphereCollider& sphere);
ContactInfo capsuleVsSphere(const CapsuleCollider& cap, const SphereCollider& sphere);
ContactInfo capsuleVsCapsule(const CapsuleCollider& a, const CapsuleCollider& b);
ContactInfo capsuleVsOBB(const CapsuleCollider& cap, const OBB3D& obb);

// ── Ray test declarations ─────────────────────────────────────────
RayHit3D rayVsSphere(const Ray3D& ray, const SphereCollider& sphere);
RayHit3D rayVsAABB(const Ray3D& ray, const AABB3D& box);
RayHit3D rayVsCapsule(const Ray3D& ray, const CapsuleCollider& cap);

// ── ConvexHull Collider ─────────────────────────────────────────
struct ConvexHullCollider : public Collider3D {
    std::vector<math::Vector3D> vertices;  // World-space convex vertices
    math::Vector3D center;                 // Centroid

    ConvexHullCollider() { type = Shape::CONVEX_HULL; }
    ConvexHullCollider(const std::vector<math::Vector3D>& verts)
        : vertices(verts) {
        type = Shape::CONVEX_HULL;
        computeCenter();
    }

    void computeCenter() {
        center = math::Vector3D::Zero;
        if (vertices.empty()) return;
        for (const auto& v : vertices) center += v;
        center = center * (1.0f / static_cast<float>(vertices.size()));
    }

    /// Support function with hill climbing optimization.
    /// For large hulls, uses adjacency-based hill climbing O(sqrt(N)) amortized.
    /// Falls back to brute force O(N) if no adjacency is built.
    math::Vector3D getSupport(const math::Vector3D& dir) const {
        if (vertices.empty()) return center;
        if (adjacency.empty() || adjacency.size() != vertices.size()) {
            return getSupportBruteForce(dir);
        }
        return getSupportHillClimb(dir);
    }

    /// Build adjacency list from convex hull faces for hill climbing support.
    /// Call this once after setting vertices. Each vertex stores indices of its neighbors.
    std::vector<std::vector<int>> adjacency;
    mutable int lastSupportIdx = 0; // Temporal coherence hint

    void buildAdjacency(const std::vector<std::array<int,3>>& faces) {
        adjacency.resize(vertices.size());
        for (auto& a : adjacency) a.clear();
        for (const auto& f : faces) {
            auto addEdge = [&](int a, int b) {
                auto& adj = adjacency[a];
                bool found = false;
                for (int n : adj) { if (n == b) { found = true; break; } }
                if (!found) adj.push_back(b);
            };
            addEdge(f[0], f[1]); addEdge(f[1], f[0]);
            addEdge(f[1], f[2]); addEdge(f[2], f[1]);
            addEdge(f[2], f[0]); addEdge(f[0], f[2]);
        }
    }

private:
    math::Vector3D getSupportBruteForce(const math::Vector3D& dir) const {
        float bestDot = -1e30f;
        int bestIdx = 0;
        for (int i = 0; i < static_cast<int>(vertices.size()); i++) {
            float d = vertices[i].dot(dir);
            if (d > bestDot) { bestDot = d; bestIdx = i; }
        }
        lastSupportIdx = bestIdx;
        return vertices[bestIdx];
    }

    math::Vector3D getSupportHillClimb(const math::Vector3D& dir) const {
        int current = lastSupportIdx;
        if (current < 0 || current >= static_cast<int>(vertices.size())) current = 0;
        float bestDot = vertices[current].dot(dir);

        for (int iter = 0; iter < static_cast<int>(vertices.size()); iter++) {
            bool improved = false;
            for (int neighbor : adjacency[current]) {
                float d = vertices[neighbor].dot(dir);
                if (d > bestDot) {
                    bestDot = d;
                    current = neighbor;
                    improved = true;
                }
            }
            if (!improved) break;
        }
        lastSupportIdx = current;
        return vertices[current];
    }
public:

    AABB3D getAABB() const {
        if (vertices.empty()) return AABB3D();
        math::Vector3D mn = vertices[0], mx = vertices[0];
        for (size_t i = 1; i < vertices.size(); i++) {
            mn.x = std::min(mn.x, vertices[i].x);
            mn.y = std::min(mn.y, vertices[i].y);
            mn.z = std::min(mn.z, vertices[i].z);
            mx.x = std::max(mx.x, vertices[i].x);
            mx.y = std::max(mx.y, vertices[i].y);
            mx.z = std::max(mx.z, vertices[i].z);
        }
        return AABB3D(mn, mx);
    }
};

// ── Generic GJK+EPA contact (works for any two shapes with getSupport) ──
template<typename ShapeA, typename ShapeB>
ContactInfo gjkEpaContact(const ShapeA& a, const ShapeB& b);

// Heightfield collisions
ContactInfo sphereVsHeightfield(const SphereCollider& sphere, const HeightfieldCollider& hf, const math::Vector3D& hfPos);

// ── Continuous Collision Detection (CCD) ───────────────────────
/// Calcula el Tiempo Exacto de Impacto [0, 1] resolviendo cuadratica
float sphereVsSphereTOI(const SphereCollider& sA, const math::Vector3D& posA, const math::Vector3D& vA,
                        const SphereCollider& sB, const math::Vector3D& posB, const math::Vector3D& vB);

} // namespace physics
} // namespace engine

// Template implementation (must be after GJK.h is available)
#include "GJK.h"

namespace engine {
namespace physics {

template<typename ShapeA, typename ShapeB>
ContactInfo gjkEpaContact(const ShapeA& a, const ShapeB& b) {
    ContactInfo result;
    GJKResult gjk = GJK::intersect(a, b);
    if (!gjk.isIntersecting) return result;
    EPAResult epa = GJK::solveEPA(a, b, gjk);
    if (!epa.success) return result;
    result.hasContact = true;
    result.normal = epa.normal;
    result.penetration = epa.penetration;
    result.contactPoint = epa.contactPoint;
    return result;
}

} // namespace physics
} // namespace engine
