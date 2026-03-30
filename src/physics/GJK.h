#pragma once

#include <vector>
#include <algorithm>
#include <cmath>
#include "math/Vector3D.h"

namespace engine {
namespace physics {

struct GJKResult {
    bool isIntersecting = false;
    math::Vector3D simplex[4];
    math::Vector3D supportA[4];
    math::Vector3D supportB[4];
    int simplexSize = 0;
};

struct EPAResult {
    bool success = false;
    math::Vector3D normal; // Apunta desde B hacia A
    float penetration = 0.0f;
    math::Vector3D contactPoint;
};

class GJK {
public:
    template<typename ShapeA, typename ShapeB>
    static GJKResult intersect(const ShapeA& a, const ShapeB& b);

    template<typename ShapeA, typename ShapeB>
    static EPAResult solveEPA(const ShapeA& a, const ShapeB& b, GJKResult& gjkResult);

private:
    static bool handleSimplex(math::Vector3D* simplex, math::Vector3D* supA, math::Vector3D* supB, int& size, math::Vector3D& dir);
    static bool lineCase(math::Vector3D* simplex, math::Vector3D* supA, math::Vector3D* supB, int& size, math::Vector3D& dir);
    static bool triangleCase(math::Vector3D* simplex, math::Vector3D* supA, math::Vector3D* supB, int& size, math::Vector3D& dir);
    static bool tetrahedronCase(math::Vector3D* simplex, math::Vector3D* supA, math::Vector3D* supB, int& size, math::Vector3D& dir);
    
    /// Soporte en el Minkowski difference (solo el punto)
    template<typename ShapeA, typename ShapeB>
    static math::Vector3D support(const ShapeA& a, const ShapeB& b, const math::Vector3D& dir) {
        return a.getSupport(dir) - b.getSupport(-dir);
    }

    /// Soporte con puntos individuales de cada shape (para calcular contacto)
    struct SupportData {
        math::Vector3D point;  // Punto en Minkowski difference
        math::Vector3D pointA; // Punto de soporte en shape A
        math::Vector3D pointB; // Punto de soporte en shape B
    };

    template<typename ShapeA, typename ShapeB>
    static SupportData supportFull(const ShapeA& a, const ShapeB& b, const math::Vector3D& dir) {
        SupportData s;
        s.pointA = a.getSupport(dir);
        s.pointB = b.getSupport(-dir);
        s.point = s.pointA - s.pointB;
        return s;
    }
    
    static const int EPA_MAX_ITERATIONS = 64;
    static constexpr float EPA_TOLERANCE = 0.00001f;
};

} // namespace physics
} // namespace engine

// Include template implementations
#include "GJK.inl"
