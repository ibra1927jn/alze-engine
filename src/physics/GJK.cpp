#include "GJK.h"
#include <utility>

namespace engine {
namespace physics {

bool GJK::handleSimplex(math::Vector3D* simplex, math::Vector3D* supA, math::Vector3D* supB,
                         int& size, math::Vector3D& dir) {
    if (size == 2) return lineCase(simplex, supA, supB, size, dir);
    if (size == 3) return triangleCase(simplex, supA, supB, size, dir);
    if (size == 4) return tetrahedronCase(simplex, supA, supB, size, dir);
    return false;
}

bool GJK::lineCase(math::Vector3D* simplex, math::Vector3D* supA, math::Vector3D* supB,
                    int& size, math::Vector3D& dir) {
    // simplex[1] = a (newest), simplex[0] = b
    math::Vector3D a = simplex[1];
    math::Vector3D b = simplex[0];
    math::Vector3D ab = b - a;
    math::Vector3D ao = -a;

    if (ab.dot(ao) > 0) {
        dir = ab.cross(ao).cross(ab);
    } else {
        // Origin closest to a — discard b
        size = 1;
        simplex[0] = simplex[1]; supA[0] = supA[1]; supB[0] = supB[1];
        dir = ao;
    }
    return false;
}

bool GJK::triangleCase(math::Vector3D* simplex, math::Vector3D* supA, math::Vector3D* supB,
                        int& size, math::Vector3D& dir) {
    // simplex[2] = a (newest), [1] = b, [0] = c
    math::Vector3D a = simplex[2];
    math::Vector3D b = simplex[1];
    math::Vector3D c = simplex[0];
    math::Vector3D ab = b - a;
    math::Vector3D ac = c - a;
    math::Vector3D ao = -a;
    math::Vector3D abc = ab.cross(ac);

    if (abc.cross(ac).dot(ao) > 0) {
        if (ac.dot(ao) > 0) {
            // Region AC: line {c, a}
            // c stays at [0], a goes to [1]
            simplex[1] = simplex[2]; supA[1] = supA[2]; supB[1] = supB[2];
            size = 2;
            dir = ac.cross(ao).cross(ac);
        } else {
            // Reduce to line {b, a}: [0]=b, [1]=a
            simplex[0] = simplex[1]; supA[0] = supA[1]; supB[0] = supB[1];
            simplex[1] = simplex[2]; supA[1] = supA[2]; supB[1] = supB[2];
            size = 2;
            return lineCase(simplex, supA, supB, size, dir);
        }
    } else {
        if (ab.cross(abc).dot(ao) > 0) {
            // Reduce to line {b, a}: [0]=b, [1]=a
            simplex[0] = simplex[1]; supA[0] = supA[1]; supB[0] = supB[1];
            simplex[1] = simplex[2]; supA[1] = supA[2]; supB[1] = supB[2];
            size = 2;
            return lineCase(simplex, supA, supB, size, dir);
        } else {
            if (abc.dot(ao) > 0) {
                dir = abc;
            } else {
                // Flip winding: swap b and c
                std::swap(simplex[0], simplex[1]);
                std::swap(supA[0], supA[1]);
                std::swap(supB[0], supB[1]);
                dir = -abc;
            }
        }
    }
    return false;
}

bool GJK::tetrahedronCase(math::Vector3D* simplex, math::Vector3D* supA, math::Vector3D* supB,
                           int& size, math::Vector3D& dir) {
    // simplex[3]=a (newest), [2]=b, [1]=c, [0]=d
    // Save all entries first to avoid aliasing issues during rearrangement
    math::Vector3D sv[4], saA[4], saB[4];
    for (int i = 0; i < 4; i++) { sv[i] = simplex[i]; saA[i] = supA[i]; saB[i] = supB[i]; }

    math::Vector3D a = sv[3], b = sv[2], c = sv[1], d = sv[0];
    math::Vector3D ab = b - a, ac = c - a, ad = d - a;
    math::Vector3D ao = -a;
    math::Vector3D abc = ab.cross(ac);
    math::Vector3D acd = ac.cross(ad);
    math::Vector3D adb = ad.cross(ab);

    if (abc.dot(ao) > 0) {
        // Face ABC: triangle {c, b, a} → [0]=c, [1]=b, [2]=a
        simplex[0] = sv[1]; supA[0] = saA[1]; supB[0] = saB[1]; // c
        simplex[1] = sv[2]; supA[1] = saA[2]; supB[1] = saB[2]; // b
        simplex[2] = sv[3]; supA[2] = saA[3]; supB[2] = saB[3]; // a
        size = 3;
        return triangleCase(simplex, supA, supB, size, dir);
    }
    if (acd.dot(ao) > 0) {
        // Face ACD: triangle {d, c, a} → [0]=d, [1]=c, [2]=a
        simplex[0] = sv[0]; supA[0] = saA[0]; supB[0] = saB[0]; // d
        simplex[1] = sv[1]; supA[1] = saA[1]; supB[1] = saB[1]; // c
        simplex[2] = sv[3]; supA[2] = saA[3]; supB[2] = saB[3]; // a
        size = 3;
        return triangleCase(simplex, supA, supB, size, dir);
    }
    if (adb.dot(ao) > 0) {
        // Face ADB: triangle {b, d, a} → [0]=b, [1]=d, [2]=a
        simplex[0] = sv[2]; supA[0] = saA[2]; supB[0] = saB[2]; // b
        simplex[1] = sv[0]; supA[1] = saA[0]; supB[1] = saB[0]; // d
        simplex[2] = sv[3]; supA[2] = saA[3]; supB[2] = saB[3]; // a
        size = 3;
        return triangleCase(simplex, supA, supB, size, dir);
    }

    return true; // Origin enclosed
}

} // namespace physics
} // namespace engine
