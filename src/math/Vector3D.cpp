#include "Vector3D.h"
#include "Vector2D.h"

namespace engine {
namespace math {

// ── Constantes estáticas ───────────────────────────────────────
const Vector3D Vector3D::Zero    = Vector3D(0.0f, 0.0f, 0.0f);
const Vector3D Vector3D::One     = Vector3D(1.0f, 1.0f, 1.0f);
const Vector3D Vector3D::Up      = Vector3D(0.0f, 1.0f, 0.0f);
const Vector3D Vector3D::Down    = Vector3D(0.0f,-1.0f, 0.0f);
const Vector3D Vector3D::Right   = Vector3D(1.0f, 0.0f, 0.0f);
const Vector3D Vector3D::Left    = Vector3D(-1.0f,0.0f, 0.0f);
const Vector3D Vector3D::Forward = Vector3D(0.0f, 0.0f,-1.0f);
const Vector3D Vector3D::Back    = Vector3D(0.0f, 0.0f, 1.0f);

// ── Constructor desde Vector2D ─────────────────────────────────
Vector3D::Vector3D(const Vector2D& v2, float z_)
#if ENGINE_SIMD_SSE2
    : simd(_mm_set_ps(0.0f, z_, v2.y, v2.x)) {}
#else
    : x(v2.x), y(v2.y), z(z_) {}
#endif

// ── Debug ──────────────────────────────────────────────────────
std::string Vector3D::toString() const {
    char buf[80];
    std::snprintf(buf, sizeof(buf), "Vector3D(%g, %g, %g)", x, y, z);
    return buf;
}

} // namespace math
} // namespace engine
