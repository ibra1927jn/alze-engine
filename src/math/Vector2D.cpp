#include "Vector2D.h"

namespace engine {
namespace math {

// ── Constantes estáticas ───────────────────────────────────────
// Definidas aquí porque son static class members.
const Vector2D Vector2D::Zero  = Vector2D(0.0f, 0.0f);
const Vector2D Vector2D::One   = Vector2D(1.0f, 1.0f);
const Vector2D Vector2D::Up    = Vector2D(0.0f, -1.0f);   // Y negativo = arriba en pantalla
const Vector2D Vector2D::Down  = Vector2D(0.0f, 1.0f);
const Vector2D Vector2D::Left  = Vector2D(-1.0f, 0.0f);
const Vector2D Vector2D::Right = Vector2D(1.0f, 0.0f);

// ── ostream ────────────────────────────────────────────────────
std::ostream& operator<<(std::ostream& os, const Vector2D& v) {
    os << "(" << v.x << ", " << v.y << ")";
    return os;
}

} // namespace math
} // namespace engine
