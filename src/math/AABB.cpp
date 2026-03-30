#include "AABB.h"
#include "MathUtils.h"

namespace engine {
namespace math {

// ── Constructores ──────────────────────────────────────────────
AABB::AABB() : min(Vector2D::Zero), max(Vector2D::Zero) {}

AABB::AABB(const Vector2D& min_, const Vector2D& max_) : min(min_), max(max_) {}

AABB AABB::fromCenter(const Vector2D& center, const Vector2D& halfSize) {
    return AABB(center - halfSize, center + halfSize);
}

AABB AABB::fromRect(float x, float y, float width, float height) {
    return AABB(Vector2D(x, y), Vector2D(x + width, y + height));
}

// ── Propiedades ────────────────────────────────────────────────
Vector2D AABB::center() const {
    return (min + max) * 0.5f;
}

Vector2D AABB::size() const {
    return max - min;
}

Vector2D AABB::halfSize() const {
    return (max - min) * 0.5f;
}

float AABB::width() const {
    return max.x - min.x;
}

float AABB::height() const {
    return max.y - min.y;
}

float AABB::area() const {
    return width() * height();
}

// ── Tests de colisión ──────────────────────────────────────────

bool AABB::contains(const Vector2D& point) const {
    return point.x >= min.x && point.x <= max.x &&
           point.y >= min.y && point.y <= max.y;
}

bool AABB::overlaps(const AABB& other) const {
    // Dos AABBs NO se solapan si están separadas en algún eje.
    // Se solapan si se superponen en AMBOS ejes X e Y.
    if (max.x < other.min.x || min.x > other.max.x) return false;
    if (max.y < other.min.y || min.y > other.max.y) return false;
    return true;
}

AABB AABB::intersection(const AABB& other) const {
    if (!overlaps(other)) {
        return AABB();  // AABB vacía
    }
    return AABB(
        Vector2D(MathUtils::max(min.x, other.min.x), MathUtils::max(min.y, other.min.y)),
        Vector2D(MathUtils::min(max.x, other.max.x), MathUtils::min(max.y, other.max.y))
    );
}

AABB AABB::merge(const AABB& other) const {
    return AABB(
        Vector2D(MathUtils::min(min.x, other.min.x), MathUtils::min(min.y, other.min.y)),
        Vector2D(MathUtils::max(max.x, other.max.x), MathUtils::max(max.y, other.max.y))
    );
}

// ── Información de colisión ────────────────────────────────────

Vector2D AABB::getOverlap(const AABB& other) const {
    // Calcular cuánto se solapan en cada eje
    float overlapX = MathUtils::min(max.x, other.max.x) - MathUtils::max(min.x, other.min.x);
    float overlapY = MathUtils::min(max.y, other.max.y) - MathUtils::max(min.y, other.min.y);

    if (overlapX <= 0.0f || overlapY <= 0.0f) {
        return Vector2D::Zero;  // No hay solapamiento
    }

    return Vector2D(overlapX, overlapY);
}

Vector2D AABB::getCollisionNormal(const AABB& other) const {
    // La normal apunta en la dirección de mínima penetración (MTV)
    Vector2D overlap = getOverlap(other);

    if (overlap == Vector2D::Zero) {
        return Vector2D::Zero;
    }

    // Dirección del centro de 'this' al centro de 'other'
    Vector2D direction = other.center() - center();

    // La normal va por el eje de menor penetración
    if (overlap.x < overlap.y) {
        // Colisión horizontal
        return Vector2D(direction.x > 0.0f ? 1.0f : -1.0f, 0.0f);
    } else {
        // Colisión vertical
        return Vector2D(0.0f, direction.y > 0.0f ? 1.0f : -1.0f);
    }
}

// ── Raycast (Kay-Kajiya Slab Algorithm) ────────────────────────

AABB::RayHit AABB::raycast(const Vector2D& origin, const Vector2D& direction,
                            float maxDistance) const {
    RayHit result = { false, 0.0f, Vector2D::Zero, Vector2D::Zero };

    float tmin = 0.0f;
    float tmax = maxDistance;
    Vector2D normal;

    // Test X slab
    if (MathUtils::abs(direction.x) < MathUtils::EPSILON) {
        // Rayo paralelo al eje Y — si no está dentro del slab, no hay hit
        if (origin.x < min.x || origin.x > max.x) return result;
    } else {
        float invD = 1.0f / direction.x;
        float t1 = (min.x - origin.x) * invD;
        float t2 = (max.x - origin.x) * invD;

        Vector2D n1(-1, 0);
        Vector2D n2(1, 0);

        if (t1 > t2) { std::swap(t1, t2); std::swap(n1, n2); }

        if (t1 > tmin) { tmin = t1; normal = n1; }
        if (t2 < tmax) { tmax = t2; }

        if (tmin > tmax) return result;
    }

    // Test Y slab
    if (MathUtils::abs(direction.y) < MathUtils::EPSILON) {
        if (origin.y < min.y || origin.y > max.y) return result;
    } else {
        float invD = 1.0f / direction.y;
        float t1 = (min.y - origin.y) * invD;
        float t2 = (max.y - origin.y) * invD;

        Vector2D n1(0, -1);
        Vector2D n2(0, 1);

        if (t1 > t2) { std::swap(t1, t2); std::swap(n1, n2); }

        if (t1 > tmin) { tmin = t1; normal = n1; }
        if (t2 < tmax) { tmax = t2; }

        if (tmin > tmax) return result;
    }

    result.hit = true;
    result.distance = tmin;
    result.point = origin + direction * tmin;
    result.normal = normal;
    return result;
}

// ── Utilidades ─────────────────────────────────────────────────

AABB AABB::expanded(float amount) const {
    Vector2D expand(amount, amount);
    return AABB(min - expand, max + expand);
}

AABB AABB::translated(const Vector2D& offset) const {
    return AABB(min + offset, max + offset);
}

bool AABB::isValid() const {
    return min.x <= max.x && min.y <= max.y;
}

// ── Comparación ────────────────────────────────────────────────
bool AABB::operator==(const AABB& other) const {
    return min == other.min && max == other.max;
}

bool AABB::operator!=(const AABB& other) const {
    return !(*this == other);
}

// ── Debug ──────────────────────────────────────────────────────
std::string AABB::toString() const {
    char buf[128];
    Vector2D sz = size();
    std::snprintf(buf, sizeof(buf), "AABB(min=(%g, %g), max=(%g, %g), size=(%g, %g))",
        min.x, min.y, max.x, max.y, sz.x, sz.y);
    return buf;
}

} // namespace math
} // namespace engine
