#pragma once

#include "Vector2D.h"
#include "Matrix3x3.h"

namespace engine {
namespace math {

/// Transform2D — Puente entre las físicas y los gráficos.
///
/// Empaqueta la posición, rotación y escala de cualquier objeto del motor.
/// Cada entidad del juego tendrá un Transform2D que define DÓNDE está,
/// CÓMO está rotada y QUÉ TAmaño tiene.
///
/// Este componente será usado tanto por Body2D (física) como por el
/// sistema de renderizado (gráficos).
struct Transform2D {
    Vector2D position = Vector2D::Zero;   // Posición en el mundo (píxeles)
    float    rotation = 0.0f;             // Rotación en radianes
    Vector2D scale    = Vector2D::One;    // Escala (1,1 = tamaño original)

    // ── Constructores ──────────────────────────────────────────
    Transform2D() = default;

    Transform2D(const Vector2D& pos)
        : position(pos) {}

    Transform2D(const Vector2D& pos, float rot)
        : position(pos), rotation(rot) {}

    Transform2D(const Vector2D& pos, float rot, const Vector2D& scl)
        : position(pos), rotation(rot), scale(scl) {}

    // ── Utilidades ─────────────────────────────────────────────

    /// Dirección "adelante" basada en la rotación actual
    Vector2D forward() const {
        return Vector2D(std::cos(rotation), std::sin(rotation));
    }

    /// Dirección "derecha" (perpendicular a forward)
    Vector2D right() const {
        return forward().perpendicular();
    }

    /// Genera la Matrix3x3 de transformación completa (TRS)
    /// Orden: primero Escala, luego Rotación, luego Traslación
    Matrix3x3 toMatrix() const {
        return Matrix3x3::translation(position.x, position.y)
             * Matrix3x3::rotation(rotation)
             * Matrix3x3::scale(scale.x, scale.y);
    }

    /// Transforma un punto local a coordenadas del mundo
    Vector2D localToWorld(const Vector2D& localPoint) const {
        return toMatrix().transformPoint(localPoint);
    }
};

} // namespace math
} // namespace engine
