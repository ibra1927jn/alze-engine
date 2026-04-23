#include "Collider3D.h"
#include <cmath>

namespace engine {
namespace physics {

// ── Segment utilities ────────────────────────────────────────────

math::Vector3D closestPointOnSegment(const math::Vector3D& A,
                                     const math::Vector3D& B,
                                     const math::Vector3D& P)
{
    math::Vector3D AB = B - A;
    float t = (P - A).dot(AB) / (AB.dot(AB) + 1e-10f);
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return A + AB * t;
}

void closestPointsSegmentSegment(
    const math::Vector3D& p1, const math::Vector3D& q1,
    const math::Vector3D& p2, const math::Vector3D& q2,
    math::Vector3D& outA, math::Vector3D& outB)
{
    math::Vector3D d1 = q1 - p1;
    math::Vector3D d2 = q2 - p2;
    math::Vector3D r  = p1 - p2;
    float a = d1.dot(d1), e = d2.dot(d2), f = d2.dot(r);
    float s, t;

    if (a <= 1e-10f && e <= 1e-10f) { outA = p1; outB = p2; return; }
    if (a <= 1e-10f) {
        s = 0.0f; t = f / e;
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    } else {
        float c = d1.dot(r);
        if (e <= 1e-10f) {
            t = 0.0f; s = -c / a;
            s = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s);
        } else {
            float b = d1.dot(d2);
            float denom = a * e - b * b;
            if (denom > 1e-10f) {
                s = (b * f - c * e) / denom;
                s = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s);
            } else { s = 0.0f; }
            t = (b * s + f) / e;
            if (t < 0.0f) {
                t = 0.0f; s = -c / a;
                s = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s);
            } else if (t > 1.0f) {
                t = 1.0f; s = (b - c) / a;
                s = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s);
            }
        }
    }
    outA = p1 + d1 * s;
    outB = p2 + d2 * t;
}

// ── Contact detection ────────────────────────────────────────────

ContactInfo sphereVsSphere(const SphereCollider& a, const SphereCollider& b) {
    ContactInfo info;
    math::Vector3D diff = b.center - a.center;
    float dist2 = diff.x*diff.x + diff.y*diff.y + diff.z*diff.z;
    float rSum = a.radius + b.radius;
    if (dist2 >= rSum * rSum) return info;
    float dist = std::sqrt(dist2);
    info.hasContact = true;
    info.penetration = rSum - dist;
    info.normal = dist > 0.0001f ? diff * (1.0f / dist) : math::Vector3D(0, 1, 0);
    info.contactPoint = a.center + info.normal * a.radius;
    return info;
}

ContactInfo sphereVsAABB(const SphereCollider& sphere, const AABB3D& box) {
    ContactInfo info;
    math::Vector3D closest = box.closestPoint(sphere.center);
    math::Vector3D diff = sphere.center - closest;
    float dist2 = diff.x*diff.x + diff.y*diff.y + diff.z*diff.z;
    if (dist2 >= sphere.radius * sphere.radius) return info;
    float dist = std::sqrt(dist2);
    info.hasContact = true;
    info.penetration = sphere.radius - dist;
    info.contactPoint = closest;
    if (dist > 0.0001f) {
        info.normal = diff * (1.0f / dist);
    } else {
        math::Vector3D c = box.center(), e = box.extents(), d = sphere.center - c;
        float minOverlap = 1e10f; int axis = 0; float sign = 1.0f;
        float dArr[3] = { d.x, d.y, d.z }, eArr[3] = { e.x, e.y, e.z };
        for (int i = 0; i < 3; i++) {
            float overlap = eArr[i] - std::abs(dArr[i]);
            if (overlap < minOverlap) { minOverlap = overlap; axis = i; sign = dArr[i] >= 0 ? 1.0f : -1.0f; }
        }
        info.normal = math::Vector3D(0, 0, 0);
        if (axis == 0) info.normal.x = sign;
        else if (axis == 1) info.normal.y = sign;
        else info.normal.z = sign;
        info.penetration = minOverlap + sphere.radius;
    }
    return info;
}

ContactInfo aabbVsAABB(const AABB3D& a, const AABB3D& b) {
    ContactInfo info;
    if (!a.overlaps(b)) return info;
    math::Vector3D aCenter = a.center(), bCenter = b.center();
    math::Vector3D aExt = a.extents(), bExt = b.extents();
    math::Vector3D diff = bCenter - aCenter;
    float ox = (aExt.x + bExt.x) - std::abs(diff.x);
    float oy = (aExt.y + bExt.y) - std::abs(diff.y);
    float oz = (aExt.z + bExt.z) - std::abs(diff.z);
    info.hasContact = true;
    if (ox <= oy && ox <= oz) { info.penetration = ox; info.normal = math::Vector3D(diff.x >= 0 ? 1.0f : -1.0f, 0, 0); }
    else if (oy <= oz)        { info.penetration = oy; info.normal = math::Vector3D(0, diff.y >= 0 ? 1.0f : -1.0f, 0); }
    else                      { info.penetration = oz; info.normal = math::Vector3D(0, 0, diff.z >= 0 ? 1.0f : -1.0f); }
    info.contactPoint = (aCenter + bCenter) * 0.5f;
    return info;
}

ContactInfo obbVsOBB(const OBB3D& a, const OBB3D& b) {
    ContactInfo info;
    math::Vector3D d = b.center - a.center;
    float R[3][3], absR[3][3];
    const float SAT_EPSILON = 1e-6f;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            R[i][j] = a.axes[i].dot(b.axes[j]);
            absR[i][j] = std::abs(R[i][j]) + SAT_EPSILON;
        }
    float heA[3] = { a.halfExtents.x, a.halfExtents.y, a.halfExtents.z };
    float heB[3] = { b.halfExtents.x, b.halfExtents.y, b.halfExtents.z };
    float minOverlap = 1e30f;
    math::Vector3D bestAxis;
    auto testAxis = [&](const math::Vector3D& axis, float projA, float projB) -> bool {
        float projD = std::abs(d.dot(axis));
        float overlap = projA + projB - projD;
        if (overlap < 0.0f) return false;
        if (overlap < minOverlap) {
            minOverlap = overlap; bestAxis = axis;
            if (d.dot(axis) < 0.0f) bestAxis = bestAxis * -1.0f;
        }
        return true;
    };
    for (int i = 0; i < 3; i++) {
        float rB = heB[0]*absR[i][0] + heB[1]*absR[i][1] + heB[2]*absR[i][2];
        if (!testAxis(a.axes[i], heA[i], rB)) return info;
    }
    for (int j = 0; j < 3; j++) {
        float rA = heA[0]*absR[0][j] + heA[1]*absR[1][j] + heA[2]*absR[2][j];
        if (!testAxis(b.axes[j], rA, heB[j])) return info;
    }
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            math::Vector3D axis = a.axes[i].cross(b.axes[j]);
            float len = axis.magnitude();
            if (len < 1e-6f) continue;
            axis = axis * (1.0f / len);
            float rA = 0.0f, rB = 0.0f;
            for (int k = 0; k < 3; k++) {
                rA += heA[k] * std::abs(a.axes[k].dot(axis));
                rB += heB[k] * std::abs(b.axes[k].dot(axis));
            }
            if (!testAxis(axis, rA, rB)) return info;
        }
    }
    info.hasContact = true;
    info.penetration = minOverlap;
    info.normal = bestAxis;
    math::Vector3D cpA = a.center, cpB = b.center;
    for (int i = 0; i < 3; i++) {
        cpA += a.axes[i] * (heA[i] * (a.axes[i].dot(info.normal) > 0.0f ? -1.0f : 1.0f));
        cpB += b.axes[i] * (heB[i] * (b.axes[i].dot(info.normal) > 0.0f ? 1.0f : -1.0f));
    }
    info.contactPoint = (cpA + cpB) * 0.5f;
    return info;
}

ContactInfo obbVsSphere(const OBB3D& obb, const SphereCollider& sphere) {
    ContactInfo info;
    math::Vector3D d = sphere.center - obb.center;
    float localX = d.dot(obb.axes[0]), localY = d.dot(obb.axes[1]), localZ = d.dot(obb.axes[2]);
    float heA[3] = { obb.halfExtents.x, obb.halfExtents.y, obb.halfExtents.z };
    float localArr[3] = { localX, localY, localZ };
    float clamped[3]; bool inside = true;
    for (int i = 0; i < 3; i++) {
        clamped[i] = localArr[i];
        if (clamped[i] < -heA[i]) { clamped[i] = -heA[i]; inside = false; }
        else if (clamped[i] > heA[i]) { clamped[i] = heA[i]; inside = false; }
    }
    math::Vector3D closest = obb.center;
    for (int i = 0; i < 3; i++) closest += obb.axes[i] * clamped[i];
    math::Vector3D diff = sphere.center - closest;
    float dist2 = diff.dot(diff);
    if (inside) {
        info.hasContact = true;
        float minOverlap = 1e30f; int bestAxis = 1; float bestSign = 1.0f;
        for (int i = 0; i < 3; i++) {
            float overlap = heA[i] - std::abs(localArr[i]);
            if (overlap < minOverlap) { minOverlap = overlap; bestAxis = i; bestSign = localArr[i] >= 0.0f ? 1.0f : -1.0f; }
        }
        info.normal = obb.axes[bestAxis] * bestSign;
        info.penetration = minOverlap + sphere.radius;
        info.contactPoint = sphere.center - info.normal * sphere.radius;
        return info;
    }
    if (dist2 >= sphere.radius * sphere.radius) return info;
    float dist = std::sqrt(dist2);
    info.hasContact = true;
    info.penetration = sphere.radius - dist;
    info.contactPoint = closest;
    info.normal = diff * (1.0f / dist);
    return info;
}

ContactInfo capsuleVsSphere(const CapsuleCollider& cap, const SphereCollider& sphere) {
    ContactInfo info;
    math::Vector3D closest = closestPointOnSegment(cap.pointA(), cap.pointB(), sphere.center);
    math::Vector3D diff = sphere.center - closest;
    float dist2 = diff.dot(diff), rSum = cap.radius + sphere.radius;
    if (dist2 >= rSum * rSum) return info;
    float dist = std::sqrt(dist2);
    info.hasContact = true;
    info.penetration = rSum - dist;
    info.contactPoint = closest + diff * (cap.radius / (rSum + 1e-10f));
    info.normal = dist > 0.0001f ? diff * (1.0f / dist) : math::Vector3D(0, 1, 0);
    return info;
}

ContactInfo capsuleVsCapsule(const CapsuleCollider& a, const CapsuleCollider& b) {
    ContactInfo info;
    math::Vector3D closestA, closestB;
    closestPointsSegmentSegment(a.pointA(), a.pointB(), b.pointA(), b.pointB(), closestA, closestB);
    math::Vector3D diff = closestB - closestA;
    float dist2 = diff.dot(diff), rSum = a.radius + b.radius;
    if (dist2 >= rSum * rSum) return info;
    float dist = std::sqrt(dist2);
    info.hasContact = true;
    info.penetration = rSum - dist;
    info.contactPoint = closestA + diff * (a.radius / (rSum + 1e-10f));
    info.normal = dist > 0.0001f ? diff * (1.0f / dist) : math::Vector3D(0, 1, 0);
    return info;
}

ContactInfo capsuleVsOBB(const CapsuleCollider& cap, const OBB3D& obb) {
    ContactInfo best;
    best.penetration = -1e30f;
    math::Vector3D capA = cap.pointA(), capB = cap.pointB();
    // 16 muestras para mejor cobertura del segmento de la capsula
    const int samples = 16;
    for (int i = 0; i < samples; i++) {
        float t = static_cast<float>(i) / static_cast<float>(samples - 1);
        math::Vector3D sampleCenter = capA + (capB - capA) * t;
        SphereCollider sampleSphere(sampleCenter, cap.radius);
        ContactInfo ci = obbVsSphere(obb, sampleSphere);
        if (ci.hasContact && ci.penetration > best.penetration) best = ci;
    }
    best.hasContact = best.penetration > 0.0f;
    return best;
}

// ── Ray tests ────────────────────────────────────────────────────

RayHit3D rayVsSphere(const Ray3D& ray, const SphereCollider& sphere) {
    RayHit3D hit;
    math::Vector3D oc = ray.origin - sphere.center;
    float b = oc.dot(ray.direction);
    float c = oc.dot(oc) - sphere.radius * sphere.radius;
    float discriminant = b * b - c;
    if (discriminant < 0) return hit;
    float t = -b - std::sqrt(discriminant);
    if (t < 0) t = -b + std::sqrt(discriminant);
    if (t < 0) return hit;
    hit.hit = true; hit.distance = t;
    hit.point = ray.pointAt(t);
    hit.normal = (hit.point - sphere.center).normalized();
    return hit;
}

RayHit3D rayVsAABB(const Ray3D& ray, const AABB3D& box) {
    RayHit3D hit;
    float tmin = -1e30f, tmax = 1e30f; int hitAxis = 0;
    float dirArr[3] = { ray.direction.x, ray.direction.y, ray.direction.z };
    float oriArr[3] = { ray.origin.x, ray.origin.y, ray.origin.z };
    float minArr[3] = { box.min.x, box.min.y, box.min.z };
    float maxArr[3] = { box.max.x, box.max.y, box.max.z };
    for (int i = 0; i < 3; i++) {
        float d = dirArr[i], o = oriArr[i];
        if (std::abs(d) < 1e-8f) {
            if (o < minArr[i] || o > maxArr[i]) return hit;
        } else {
            float invD = 1.0f / d;
            float t1 = (minArr[i] - o) * invD, t2 = (maxArr[i] - o) * invD;
            if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
            if (t1 > tmin) { tmin = t1; hitAxis = i; }
            if (t2 < tmax) tmax = t2;
            if (tmin > tmax) return hit;
        }
    }
    if (tmin < 0) return hit;
    hit.hit = true; hit.distance = tmin; hit.point = ray.pointAt(tmin);
    hit.normal = math::Vector3D(0, 0, 0);
    if (hitAxis == 0) hit.normal.x = dirArr[0] > 0 ? -1.0f : 1.0f;
    else if (hitAxis == 1) hit.normal.y = dirArr[1] > 0 ? -1.0f : 1.0f;
    else hit.normal.z = dirArr[2] > 0 ? -1.0f : 1.0f;
    return hit;
}

RayHit3D rayVsCapsule(const Ray3D& ray, const CapsuleCollider& cap) {
    RayHit3D best; best.distance = 1e30f;
    math::Vector3D capA = cap.pointA(), capB = cap.pointB();
    const int samples = 5;
    for (int i = 0; i < samples; i++) {
        float t = static_cast<float>(i) / static_cast<float>(samples - 1);
        math::Vector3D center = capA + (capB - capA) * t;
        SphereCollider sphere(center, cap.radius);
        RayHit3D hit = rayVsSphere(ray, sphere);
        if (hit.hit && hit.distance < best.distance) best = hit;
    }
    return best;
}

ContactInfo sphereVsHeightfield(const SphereCollider& sphere, const HeightfieldCollider& hf, const math::Vector3D& hfPos) {
    ContactInfo info;
    info.hasContact = false; // Changed from info.hit to info.hasContact
    
    // Convert sphere center to heightfield local space
    math::Vector3D localCenter = sphere.center - hfPos;
    
    float gridX = localCenter.x + (hf.numCols * hf.colSpacing * 0.5f);
    float gridZ = localCenter.z + (hf.numRows * hf.rowSpacing * 0.5f);
    
    int col = static_cast<int>(std::floor(gridX / hf.colSpacing));
    int row = static_cast<int>(std::floor(gridZ / hf.rowSpacing));
    
    // Bounds check
    if (row >= 0 && row < hf.numRows - 1 && col >= 0 && col < hf.numCols - 1) {
        float h00 = hf.getHeightAt(row, col);
        float h10 = hf.getHeightAt(row + 1, col);
        float h01 = hf.getHeightAt(row, col + 1);
        float h11 = hf.getHeightAt(row + 1, col + 1);
        
        float maxH = std::max(std::max(h00, h10), std::max(h01, h11));
        
        // Fast rejection
        if (localCenter.y - sphere.radius < maxH) {
            float dx = (h01 - h00) / hf.colSpacing;
            float dz = (h10 - h00) / hf.rowSpacing;
            math::Vector3D normal(-dx, 1.0f, -dz);
            normal = normal.normalized();
            
            // Bilinear interpolation for exact height
            float tx = (gridX - col * hf.colSpacing) / hf.colSpacing;
            float tz = (gridZ - row * hf.rowSpacing) / hf.rowSpacing;
            float heightTop = h00 * (1.0f - tx) + h01 * tx;
            float heightBot = h10 * (1.0f - tx) + h11 * tx;
            float exactH = heightTop * (1.0f - tz) + heightBot * tz;
            
            if (localCenter.y - sphere.radius < exactH) {
                info.hasContact = true; // Changed from info.hit to info.hasContact
                info.normal = normal;
                info.contactPoint = math::Vector3D(sphere.center.x, hfPos.y + exactH, sphere.center.z);
                info.penetration = exactH - (localCenter.y - sphere.radius);
            }
        }
    }
    return info;
}

// ── Continuous Collision Detection (CCD) ───────────────────────
float sphereVsSphereTOI(const SphereCollider& sA, const math::Vector3D& posA, const math::Vector3D& vA,
                        const SphereCollider& sB, const math::Vector3D& posB, const math::Vector3D& vB) {
    math::Vector3D p = posA - posB;
    math::Vector3D v = vA - vB;
    float r = sA.radius + sB.radius;
    
    float a = v.sqrMagnitude();
    float b = 2.0f * p.dot(v);
    float c = p.sqrMagnitude() - r * r;
    
    if (c <= 0.0f) return 0.0f; // Already overlapping
    if (b >= 0.0f || a < 1e-8f) return -1.0f; // Moving apart or static
    
    float d = b * b - 4.0f * a * c;
    if (d < 0.0f) return -1.0f; // Missed each other
    
    float t = (-b - std::sqrt(d)) / (2.0f * a);
    if (t >= 0.0f && t <= 1.0f) return t;
    return -1.0f;
}

} // namespace physics
} // namespace engine
