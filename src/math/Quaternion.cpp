#include "Quaternion.h"
#include "Matrix4x4.h"

namespace engine {
namespace math {

// ── Constante ──────────────────────────────────────────────────
const Quaternion Quaternion::Identity = Quaternion(0.0f, 0.0f, 0.0f, 1.0f);

// ── lookRotation ───────────────────────────────────────────────
Quaternion Quaternion::lookRotation(const Vector3D& forward, const Vector3D& up) {
    Vector3D f = forward.normalized();
    Vector3D r = f.cross(up).normalized();
    Vector3D u = r.cross(f);

    // Construir matriz de rotación 3x3 y extraer cuaternión
    // Basado en el algoritmo de Shepperd para extracción robusta
    float m00 = r.x,  m01 = u.x, m02 = -f.x;
    float m10 = r.y,  m11 = u.y, m12 = -f.y;
    float m20 = r.z,  m21 = u.z, m22 = -f.z;

    float trace = m00 + m11 + m22;

    Quaternion q;
    if (trace > 0.0f) {
        float s = 0.5f / std::sqrt(trace + 1.0f);
        q.w = 0.25f / s;
        q.x = (m21 - m12) * s;
        q.y = (m02 - m20) * s;
        q.z = (m10 - m01) * s;
    } else if (m00 > m11 && m00 > m22) {
        float s = 2.0f * std::sqrt(1.0f + m00 - m11 - m22);
        q.w = (m21 - m12) / s;
        q.x = 0.25f * s;
        q.y = (m01 + m10) / s;
        q.z = (m02 + m20) / s;
    } else if (m11 > m22) {
        float s = 2.0f * std::sqrt(1.0f + m11 - m00 - m22);
        q.w = (m02 - m20) / s;
        q.x = (m01 + m10) / s;
        q.y = 0.25f * s;
        q.z = (m12 + m21) / s;
    } else {
        float s = 2.0f * std::sqrt(1.0f + m22 - m00 - m11);
        q.w = (m10 - m01) / s;
        q.x = (m02 + m20) / s;
        q.y = (m12 + m21) / s;
        q.z = 0.25f * s;
    }

    return q.normalized();
}

// ── toMatrix ───────────────────────────────────────────────────
Matrix4x4 Quaternion::toMatrix() const {
    float xx = x * x, yy = y * y, zz = z * z;
    float xy = x * y, xz = x * z, yz = y * z;
    float wx = w * x, wy = w * y, wz = w * z;

    Matrix4x4 mat;
    mat.set(0, 0, 1.0f - 2.0f * (yy + zz));
    mat.set(1, 0, 2.0f * (xy + wz));
    mat.set(2, 0, 2.0f * (xz - wy));
    mat.set(3, 0, 0.0f);

    mat.set(0, 1, 2.0f * (xy - wz));
    mat.set(1, 1, 1.0f - 2.0f * (xx + zz));
    mat.set(2, 1, 2.0f * (yz + wx));
    mat.set(3, 1, 0.0f);

    mat.set(0, 2, 2.0f * (xz + wy));
    mat.set(1, 2, 2.0f * (yz - wx));
    mat.set(2, 2, 1.0f - 2.0f * (xx + yy));
    mat.set(3, 2, 0.0f);

    mat.set(0, 3, 0.0f);
    mat.set(1, 3, 0.0f);
    mat.set(2, 3, 0.0f);
    mat.set(3, 3, 1.0f);

    return mat;
}

// ── Matrix4x4::fromQuaternion ──────────────────────────────────
Matrix4x4 Matrix4x4::fromQuaternion(const Quaternion& q) {
    return q.toMatrix();
}

// ── Debug ──────────────────────────────────────────────────────
std::ostream& operator<<(std::ostream& os, const Quaternion& q) {
    return os << "Quaternion(" << q.x << ", " << q.y << ", " << q.z << ", " << q.w << ")";
}

} // namespace math
} // namespace engine
