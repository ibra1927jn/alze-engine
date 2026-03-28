#pragma once

namespace engine {
namespace physics {

template<typename ShapeA, typename ShapeB>
GJKResult GJK::intersect(const ShapeA& a, const ShapeB& b) {
    GJKResult result;
    // Initial guess: direction between centers, or up if coincident
    math::Vector3D initialDir = (a.center - b.center);
    if (initialDir.sqrMagnitude() < 1e-6f) initialDir = math::Vector3D(0, 1, 0);
    
    math::Vector3D dir = initialDir.normalized();
    math::Vector3D supportP = support(a, b, dir);
    
    result.simplex[0] = supportP;
    result.simplexSize = 1;
    dir = -supportP;

    for (int i = 0; i < 64; ++i) { // max iterations
        if (dir.sqrMagnitude() < 1e-6f) dir = math::Vector3D(0, 1, 0);
        else dir.normalize();
        
        supportP = support(a, b, dir);
        if (supportP.dot(dir) < 0.0f) {
            result.isIntersecting = false;
            return result; // No intersection
        }
        
        result.simplex[result.simplexSize++] = supportP;
        if (handleSimplex(result.simplex, result.simplexSize, dir)) {
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
    };
    
    std::vector<math::Vector3D> polytope(gjkResult.simplex, gjkResult.simplex + 4);
    std::vector<Face> faces;
    
    auto addFace = [&](int a, int b, int c) {
        math::Vector3D ab = polytope[b] - polytope[a];
        math::Vector3D ac = polytope[c] - polytope[a];
        math::Vector3D n = ab.cross(ac).normalized();
        // Check winding
        if (n.dot(polytope[a]) < 0) {
            n = -n;
            faces.push_back({c, b, a, n});
        } else {
            faces.push_back({a, b, c, n});
        }
    };
    
    addFace(0, 1, 2);
    addFace(0, 3, 1);
    addFace(0, 2, 3);
    addFace(1, 3, 2);

    int closestFace = 0;
    
    for (int iter = 0; iter < EPA_MAX_ITERATIONS; ++iter) {
        float minDist = 1e30f;
        closestFace = -1;
        
        for (int i = 0; i < (int)faces.size(); ++i) {
            float dist = faces[i].normal.dot(polytope[faces[i].a]);
            if (dist < minDist) {
                minDist = dist;
                closestFace = i;
            }
        }
        
        if (closestFace == -1) break;
        
        math::Vector3D searchDir = faces[closestFace].normal;
        math::Vector3D p = support(a, b, searchDir);
        float pDist = p.dot(searchDir);
        
        if (pDist - minDist < EPA_TOLERANCE) {
            result.success = true;
            result.normal = searchDir;
            result.penetration = pDist;
            return result;
        }

        std::vector<std::pair<int, int>> edges;
        for (auto it = faces.begin(); it != faces.end(); ) {
            if (it->normal.dot(p - polytope[it->a]) > 0) {
                auto addEdge = [&](int e1, int e2) {
                    auto match = std::find_if(edges.begin(), edges.end(), [&](const std::pair<int, int>& edge) {
                        return edge.first == e2 && edge.second == e1;
                    });
                    if (match != edges.end()) edges.erase(match);
                    else edges.push_back({e1, e2});
                };
                addEdge(it->a, it->b);
                addEdge(it->b, it->c);
                addEdge(it->c, it->a);
                it = faces.erase(it);
            } else {
                ++it;
            }
        }
        
        int pIdx = (int)polytope.size();
        polytope.push_back(p);
        
        for (auto& e : edges) {
            addFace(e.first, e.second, pIdx);
        }
    }
    
    if (closestFace != -1) {
        result.success = true;
        result.normal = faces[closestFace].normal;
        result.penetration = faces[closestFace].normal.dot(polytope[faces[closestFace].a]);
    }
    return result;
}

} // namespace physics
} // namespace engine
