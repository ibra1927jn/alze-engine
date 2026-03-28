#pragma once

#include "Vector3D.h"
#include "Quaternion.h"
#include "Matrix4x4.h"

namespace engine {
namespace math {

/// Transform3D — Puente entre las físicas y los gráficos en 3D.
///
/// Empaqueta la posición, rotación (cuaternión) y escala de cualquier
/// objeto 3D del motor. Cada entidad del juego tendrá un Transform3D
/// que define DÓNDE está, CÓMO está rotada y QUÉ TAMAÑO tiene.
///
/// Usa Quaternion para rotación en lugar de ángulos de Euler,
/// evitando gimbal lock y permitiendo interpolación suave (slerp).
///
struct Transform3D {
    Vector3D   position = Vector3D::Zero;     // Posición en el mundo
    Quaternion rotation;                       // Rotación (identidad por defecto)
    Vector3D   scale    = Vector3D::One;       // Escala (1,1,1 = tamaño original)

    // ── Constructores ──────────────────────────────────────────
    Transform3D() = default;

    Transform3D(const Vector3D& pos)
        : position(pos) {}

    Transform3D(const Vector3D& pos, const Quaternion& rot)
        : position(pos), rotation(rot) {}

    Transform3D(const Vector3D& pos, const Quaternion& rot, const Vector3D& scl)
        : position(pos), rotation(rot), scale(scl) {}

    // ── Direcciones locales ────────────────────────────────────

    /// Dirección "adelante" (forward) según la rotación actual (-Z en espacio local)
    Vector3D forward() const {
        return rotation.rotate(Vector3D::Forward);
    }

    /// Dirección "derecha" (right) según la rotación actual (+X en espacio local)
    Vector3D right() const {
        return rotation.rotate(Vector3D::Right);
    }

    /// Dirección "arriba" (up) según la rotación actual (+Y en espacio local)
    Vector3D up() const {
        return rotation.rotate(Vector3D::Up);
    }

    // ── Conversión a matriz ────────────────────────────────────

    /// Genera la Matrix4x4 de transformación completa (TRS)
    /// Orden: primero Escala, luego Rotación, luego Traslación
    /// Model Matrix = T * R * S
    Matrix4x4 toMatrix() const {
        return Matrix4x4::translation(position)
             * rotation.toMatrix()
             * Matrix4x4::scale(scale);
    }

    /// Transforma un punto local a coordenadas del mundo
    Vector3D localToWorld(const Vector3D& localPoint) const {
        return toMatrix().transformPoint(localPoint);
    }

    /// Transforma un punto del mundo a coordenadas locales
    Vector3D worldToLocal(const Vector3D& worldPoint) const {
        return toMatrix().inverse().transformPoint(worldPoint);
    }

    // ── Interpolación ──────────────────────────────────────────

    /// Interpolación entre dos transforms (lerp posición/escala, slerp rotación)
    static Transform3D lerp(const Transform3D& a, const Transform3D& b, float t) {
        return Transform3D(
            Vector3D::lerp(a.position, b.position, t),
            Quaternion::slerp(a.rotation, b.rotation, t),
            Vector3D::lerp(a.scale, b.scale, t)
        );
    }
};

} // namespace math
} // namespace engine
