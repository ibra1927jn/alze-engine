#pragma once

#include <algorithm>

namespace engine {
namespace physics {

template<typename ShapeA, typename ShapeB>
GJKResult GJK::intersect(const ShapeA& a, const ShapeB& b) {
    GJKResult result;
    // Initial guess: direction between centers, or up if coincident
    math::Vector3D initialDir = (a.center - b.center);
    if (initialDir.sqrMagnitude() < 1e-6f) initialDir = math::Vector3D(0, 1, 0);
    
    math::Vector3D dir = initialDir.normalized();

    SupportData sd = supportFull(a, b, dir);
    result.simplex[0] = sd.point;
    result.supportA[0] = sd.pointA;
    result.supportB[0] = sd.pointB;
    result.simplexSize = 1;
    dir = -sd.point;

    for (int i = 0; i < 64; ++i) {
        if (dir.sqrMagnitude() < 1e-6f) dir = math::Vector3D(0, 1, 0);
        else dir.normalize();

        sd = supportFull(a, b, dir);
        if (sd.point.dot(dir) < 0.0f) {
            result.isIntersecting = false;
            return result;
        }

        result.simplex[result.simplexSize] = sd.point;
        result.supportA[result.simplexSize] = sd.pointA;
        result.supportB[result.simplexSize] = sd.pointB;
        result.simplexSize++;

        if (handleSimplex(result.simplex, result.supportA, result.supportB, result.simplexSize, dir)) {
            result.isIntersecting = true;
            return result;
        }
    }
    result.isIntersecting = false;
    return result;
}

template<typename ShapeA, typename ShapeB>
EPAResult GJK::solveEPA(const ShapeA& a, const ShapeB& b, GJKResult& gjkResult) {
    EPAResult result;
    if (!gjkResult.isIntersecting || gjkResult.simplexSize != 4) return result;

    struct Face {
        int a, b, c;
        math::Vector3D normal;
        float dist; // Distance from origin to face plane (cached)
    };

    // Pre-allocate polytope storage
    std::vector<math::Vector3D> polytope;
    std::vector<math::Vector3D> supportA;
    std::vector<math::Vector3D> supportB;
    polytope.reserve(32);
    supportA.reserve(32);
    supportB.reserve(32);

    polytope.assign(gjkResult.simplex, gjkResult.simplex + 4);
    supportA.resize(4);
    supportB.resize(4);

    // Use exact support points tracked through GJK simplex operations
    for (int i = 0; i < 4; ++i) {
        supportA[i] = gjkResult.supportA[i];
        supportB[i] = gjkResult.supportB[i];
    }

    std::vector<Face> faces;
    faces.reserve(64);

    // Face indices sorted by distance (min-heap order), indices into faces[]
    std::vector<int> faceOrder;
    faceOrder.reserve(64);

    auto addFace = [&](int ia, int ib, int ic) {
        math::Vector3D ab = polytope[ib] - polytope[ia];
        math::Vector3D ac = polytope[ic] - polytope[ia];
        math::Vector3D n = ab.cross(ac);
        float nLen = n.magnitude();
        if (nLen < 1e-10f) return; // Degenerate face, skip
        n = n * (1.0f / nLen);
        if (n.dot(polytope[ia]) < 0) {
            n = -n;
            faces.push_back({ic, ib, ia, n, 0.0f});
        } else {
            faces.push_back({ia, ib, ic, n, 0.0f});
        }
        Face& f = faces.back();
        f.dist = f.normal.dot(polytope[f.a]);
    };

    addFace(0, 1, 2);
    addFace(0, 3, 1);
    addFace(0, 2, 3);
    addFace(1, 3, 2);

    int closestFace = -1;

    // Barycentric contact point computation
    auto computeContact = [&](int faceIdx) {
        const Face& f = faces[faceIdx];
        const math::Vector3D& va = polytope[f.a];
        const math::Vector3D& vb = polytope[f.b];
        const math::Vector3D& vc = polytope[f.c];

        math::Vector3D n = f.normal;
        math::Vector3D v0 = vb - va;
        math::Vector3D v1 = vc - va;
        math::Vector3D p = n * n.dot(va);
        math::Vector3D v2 = p - va;

        float d00 = v0.dot(v0);
        float d01 = v0.dot(v1);
        float d11 = v1.dot(v1);
        float d20 = v2.dot(v0);
        float d21 = v2.dot(v1);

        float denom = d00 * d11 - d01 * d01;
        if (std::abs(denom) < 1e-10f) {
            result.contactPoint = (supportA[f.a] + supportA[f.b] + supportA[f.c]) * (1.0f / 3.0f);
            return;
        }

        float u = (d11 * d20 - d01 * d21) / denom;
        float v = (d00 * d21 - d01 * d20) / denom;
        float w = 1.0f - u - v;

        u = std::max(0.0f, u);
        v = std::max(0.0f, v);
        w = std::max(0.0f, w);
        float sum = u + v + w;
        if (sum > 1e-10f) { u /= sum; v /= sum; w /= sum; }
        else { u = v = w = 1.0f / 3.0f; }

        math::Vector3D contactOnA = supportA[f.a] * w + supportA[f.b] * u + supportA[f.c] * v;
        math::Vector3D contactOnB = supportB[f.a] * w + supportB[f.b] * u + supportB[f.c] * v;

        result.contactPoint = (contactOnA + contactOnB) * 0.5f;
    };

    std::vector<std::pair<int, int>> edges;
    edges.reserve(32);

    for (int iter = 0; iter < EPA_MAX_ITERATIONS; ++iter) {
        // Find closest face — O(N) but with cached distances
        float minDist = 1e30f;
        closestFace = -1;

        for (int i = 0; i < static_cast<int>(faces.size()); ++i) {
            if (faces[i].dist < minDist) {
                minDist = faces[i].dist;
                closestFace = i;
            }
        }

        if (closestFace == -1) break;

        math::Vector3D searchDir = faces[closestFace].normal;
        SupportData sd = supportFull(a, b, searchDir);
        float pDist = sd.point.dot(searchDir);

        if (pDist - minDist < EPA_TOLERANCE) {
            result.success = true;
            result.normal = searchDir;
            result.penetration = pDist;
            computeContact(closestFace);
            return result;
        }

        // Remove faces visible from new point, collect horizon edges
        edges.clear();
        int writeIdx = 0;
        for (int i = 0; i < static_cast<int>(faces.size()); i++) {
            if (faces[i].normal.dot(sd.point - polytope[faces[i].a]) > 0) {
                // Face visible — collect edges and mark for removal
                auto addEdge = [&](int e1, int e2) {
                    for (int j = 0; j < static_cast<int>(edges.size()); j++) {
                        if (edges[j].first == e2 && edges[j].second == e1) {
                            edges[j] = edges.back();
                            edges.pop_back();
                            return;
                        }
                    }
                    edges.push_back({e1, e2});
                };
                addEdge(faces[i].a, faces[i].b);
                addEdge(faces[i].b, faces[i].c);
                addEdge(faces[i].c, faces[i].a);
            } else {
                // Keep face — compact in place
                if (writeIdx != i) faces[writeIdx] = faces[i];
                writeIdx++;
            }
        }
        faces.resize(writeIdx);

        int pIdx = static_cast<int>(polytope.size());
        polytope.push_back(sd.point);
        supportA.push_back(sd.pointA);
        supportB.push_back(sd.pointB);

        for (auto& e : edges) {
            addFace(e.first, e.second, pIdx);
        }
    }

    if (closestFace != -1 && closestFace < static_cast<int>(faces.size())) {
        result.success = true;
        result.normal = faces[closestFace].normal;
        result.penetration = faces[closestFace].dist;
        computeContact(closestFace);
    }
    return result;
}

} // namespace physics
} // namespace engine
