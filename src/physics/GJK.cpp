#include "GJK.h"

namespace engine {
namespace physics {

bool GJK::handleSimplex(math::Vector3D* simplex, int& size, math::Vector3D& dir) {
    if (size == 2) return lineCase(simplex, size, dir);
    if (size == 3) return triangleCase(simplex, size, dir);
    if (size == 4) return tetrahedronCase(simplex, size, dir);
    return false;
}

bool GJK::lineCase(math::Vector3D* simplex, int& size, math::Vector3D& dir) {
    math::Vector3D a = simplex[1];
    math::Vector3D b = simplex[0];
    math::Vector3D ab = b - a;
    math::Vector3D ao = -a;
    
    if (ab.dot(ao) > 0) {
        dir = ab.cross(ao).cross(ab);
    } else {
        size = 1;
        simplex[0] = a;
        dir = ao;
    }
    return false;
}

bool GJK::triangleCase(math::Vector3D* simplex, int& size, math::Vector3D& dir) {
    math::Vector3D a = simplex[2];
    math::Vector3D b = simplex[1];
    math::Vector3D c = simplex[0];
    math::Vector3D ab = b - a;
    math::Vector3D ac = c - a;
    math::Vector3D ao = -a;
    math::Vector3D abc = ab.cross(ac);
    
    if (abc.cross(ac).dot(ao) > 0) {
        if (ac.dot(ao) > 0) {
            simplex[0] = c;
            simplex[1] = a;
            size = 2;
            dir = ac.cross(ao).cross(ac);
        } else {
            return lineCase(simplex, size, dir);
        }
    } else {
        if (ab.cross(abc).dot(ao) > 0) {
            return lineCase(simplex, size, dir);
        } else {
            if (abc.dot(ao) > 0) {
                dir = abc;
            } else {
                simplex[0] = b;
                simplex[1] = c;
                dir = -abc;
            }
        }
    }
    return false;
}

bool GJK::tetrahedronCase(math::Vector3D* simplex, int& size, math::Vector3D& dir) {
    math::Vector3D a = simplex[3];
    math::Vector3D b = simplex[2];
    math::Vector3D c = simplex[1];
    math::Vector3D d = simplex[0];
    
    math::Vector3D ab = b - a;
    math::Vector3D ac = c - a;
    math::Vector3D ad = d - a;
    math::Vector3D ao = -a;
    
    math::Vector3D abc = ab.cross(ac);
    math::Vector3D acd = ac.cross(ad);
    math::Vector3D adb = ad.cross(ab);
    
    if (abc.dot(ao) > 0) {
        size = 3;
        simplex[0] = c; simplex[1] = b; simplex[2] = a;
        return triangleCase(simplex, size, dir);
    }
    
    if (acd.dot(ao) > 0) {
        size = 3;
        simplex[0] = d; simplex[1] = c; simplex[2] = a;
        return triangleCase(simplex, size, dir);
    }
    
    if (adb.dot(ao) > 0) {
        size = 3;
        simplex[0] = b; simplex[1] = d; simplex[2] = a;
        return triangleCase(simplex, size, dir);
    }
    
    return true; // Origin is enclosed in the tetrahedron!
}

} // namespace physics
} // namespace engine
