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

    // Almacenar puntos del polytope con sus soportes individuales
    // para poder reconstruir el contacto por interpolacion baricentrica
    std::vector<math::Vector3D> polytope(gjkResult.simplex, gjkResult.simplex + 4);
    std::vector<math::Vector3D> supportA(4);
    std::vector<math::Vector3D> supportB(4);

    // Recalcular los support points individuales del simplex inicial
    for (int i = 0; i < 4; ++i) {
        math::Vector3D dir = gjkResult.simplex[i].normalized();
        if (dir.sqrMagnitude() < 1e-6f) dir = math::Vector3D(0, 1, 0);
        supportA[i] = a.getSupport(dir);
        supportB[i] = b.getSupport(-dir);
    }

    std::vector<Face> faces;

    auto addFace = [&](int ia, int ib, int ic) {
        math::Vector3D ab = polytope[ib] - polytope[ia];
        math::Vector3D ac = polytope[ic] - polytope[ia];
        math::Vector3D n = ab.cross(ac).normalized();
        // Asegurar que la normal apunta hacia afuera (lejos del origen)
        if (n.dot(polytope[ia]) < 0) {
            n = -n;
            faces.push_back({ic, ib, ia, n});
        } else {
            faces.push_back({ia, ib, ic, n});
        }
    };

    addFace(0, 1, 2);
    addFace(0, 3, 1);
    addFace(0, 2, 3);
    addFace(1, 3, 2);

    int closestFace = 0;

    // Lambda para calcular contacto por interpolacion baricentrica
    // Proyecta el origen sobre la cara mas cercana y usa las coordenadas
    // baricentricas para interpolar los support points de cada shape
    auto computeContact = [&](int faceIdx) {
        const Face& f = faces[faceIdx];
        const math::Vector3D& va = polytope[f.a];
        const math::Vector3D& vb = polytope[f.b];
        const math::Vector3D& vc = polytope[f.c];

        math::Vector3D n = f.normal;

        // Coordenadas baricentricas del origen proyectado sobre el triangulo
        // Usando el metodo de areas con productos cruz
        math::Vector3D v0 = vb - va;
        math::Vector3D v1 = vc - va;
        // Punto proyectado = origen proyectado al plano = va + t*(n) donde t = n.dot(va)
        // p = n * n.dot(va) (proyeccion del origen al plano)
        math::Vector3D p = n * n.dot(va);
        math::Vector3D v2 = p - va;

        float d00 = v0.dot(v0);
        float d01 = v0.dot(v1);
        float d11 = v1.dot(v1);
        float d20 = v2.dot(v0);
        float d21 = v2.dot(v1);

        float denom = d00 * d11 - d01 * d01;
        if (std::abs(denom) < 1e-10f) {
            // Cara degenerada — usar punto medio como fallback
            result.contactPoint = (supportA[f.a] + supportA[f.b] + supportA[f.c]) * (1.0f / 3.0f);
            return;
        }

        float u = (d11 * d20 - d01 * d21) / denom;
        float v = (d00 * d21 - d01 * d20) / denom;
        float w = 1.0f - u - v;

        // Clampear coordenadas baricentricas para robustez numerica
        u = std::max(0.0f, u);
        v = std::max(0.0f, v);
        w = std::max(0.0f, w);
        float sum = u + v + w;
        if (sum > 1e-10f) { u /= sum; v /= sum; w /= sum; }
        else { u = v = w = 1.0f / 3.0f; }

        // Interpolar support points de shape A para obtener contacto en A
        math::Vector3D contactOnA = supportA[f.a] * w + supportA[f.b] * u + supportA[f.c] * v;
        math::Vector3D contactOnB = supportB[f.a] * w + supportB[f.b] * u + supportB[f.c] * v;

        // Punto de contacto = punto medio entre ambas superficies
        result.contactPoint = (contactOnA + contactOnB) * 0.5f;
    };

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
        SupportData sd = supportFull(a, b, searchDir);
        float pDist = sd.point.dot(searchDir);

        if (pDist - minDist < EPA_TOLERANCE) {
            result.success = true;
            result.normal = searchDir;
            result.penetration = pDist;
            computeContact(closestFace);
            return result;
        }

        std::vector<std::pair<int, int>> edges;
        for (auto it = faces.begin(); it != faces.end(); ) {
            if (it->normal.dot(sd.point - polytope[it->a]) > 0) {
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
        polytope.push_back(sd.point);
        supportA.push_back(sd.pointA);
        supportB.push_back(sd.pointB);

        for (auto& e : edges) {
            addFace(e.first, e.second, pIdx);
        }
    }

    if (closestFace != -1) {
        result.success = true;
        result.normal = faces[closestFace].normal;
        result.penetration = faces[closestFace].normal.dot(polytope[faces[closestFace].a]);
        computeContact(closestFace);
    }
    return result;
}

} // namespace physics
} // namespace engine
