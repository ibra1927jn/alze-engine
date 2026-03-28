#pragma once

#include "Vector2D.h"

namespace engine {
namespace math {

/// AABB — Axis-Aligned Bounding Box (Caja Delimitadora Alineada a Ejes).
///
/// Rectángulo que no rota, siempre alineado con los ejes X e Y.
/// Es la estructura más simple y rápida para detección de colisiones.
/// Se define por sus esquinas mínima (arriba-izquierda) y máxima (abajo-derecha).
///
/// Caso de uso principal:
///   - Broad Phase de colisiones: filtrar rápidamente qué objetos NO chocan.
///   - Bounds de entidades: saber el rectángulo que envuelve un sprite.
///
class AABB {
public:
    Vector2D min;  // Esquina superior-izquierda (valores menores)
    Vector2D max;  // Esquina inferior-derecha (valores mayores)

    // ── Constructores ──────────────────────────────────────────

    /// AABB vacía (min=0, max=0)
    AABB();

    /// AABB desde dos esquinas
    AABB(const Vector2D& min, const Vector2D& max);

    /// AABB desde centro y tamaño (half-extents)
    static AABB fromCenter(const Vector2D& center, const Vector2D& halfSize);

    /// AABB desde posición y dimensiones (x, y, ancho, alto)
    static AABB fromRect(float x, float y, float width, float height);

    // ── Propiedades ────────────────────────────────────────────

    /// Centro de la AABB
    Vector2D center() const;

    /// Tamaño completo (ancho, alto)
    Vector2D size() const;

    /// Mitad del tamaño (half-extents) — útil para cálculos de colisión
    Vector2D halfSize() const;

    /// Ancho
    float width() const;

    /// Alto
    float height() const;

    /// Área del rectángulo
    float area() const;

    // ── Tests de colisión ──────────────────────────────────────

    /// ¿Contiene este punto?
    bool contains(const Vector2D& point) const;

    /// ¿Se superpone con otra AABB? (Detección de colisión AABB-AABB)
    /// Esta es la operación fundamental de la Broad Phase.
    bool overlaps(const AABB& other) const;

    /// Devuelve la AABB de intersección entre dos cajas (zona de solapamiento).
    /// Si no se solapan, devuelve una AABB vacía.
    AABB intersection(const AABB& other) const;

    /// Devuelve la AABB más pequeña que contiene ambas cajas
    AABB merge(const AABB& other) const;

    // ── Información de colisión ────────────────────────────────

    /// Calcula la penetración (overlap) entre dos AABBs.
    /// Devuelve el vector de mínima penetración (MTV: Minimum Translation Vector).
    /// Si no hay penetración, devuelve Vector2D::Zero.
    Vector2D getOverlap(const AABB& other) const;

    /// Calcula en qué dirección se debe empujar para resolver la colisión.
    /// Devuelve la normal de la superficie de contacto.
    Vector2D getCollisionNormal(const AABB& other) const;

    // ── Raycast ─────────────────────────────────────────────────

    /// Resultado de un raycast contra la AABB
    struct RayHit {
        bool  hit;        // ¿Hubo intersección?
        float distance;   // Distancia desde el origen del rayo
        Vector2D point;   // Punto de impacto
        Vector2D normal;  // Normal de la superficie impactada
    };

    /// Lanza un rayo y comprueba si intersecta esta AABB.
    /// Usa el algoritmo de slabs (Kay-Kajiya).
    /// @param origin Origen del rayo
    /// @param direction Dirección del rayo (debe estar normalizada)
    /// @param maxDistance Distancia máxima del rayo
    RayHit raycast(const Vector2D& origin, const Vector2D& direction,
                   float maxDistance = 999999.0f) const;

    /// Alias estándar: ¿se superpone con otra AABB?
    bool intersects(const AABB& other) const { return overlaps(other); }

    // ── Utilidades ─────────────────────────────────────────────

    /// Expande la AABB en todas las direcciones por 'amount'
    AABB expanded(float amount) const;

    /// Mueve la AABB por un desplazamiento
    AABB translated(const Vector2D& offset) const;

    /// ¿Es una AABB válida? (min < max en ambos ejes)
    bool isValid() const;

    // ── Comparación ────────────────────────────────────────────
    bool operator==(const AABB& other) const;
    bool operator!=(const AABB& other) const;

    // ── Debug ──────────────────────────────────────────────────
    friend std::ostream& operator<<(std::ostream& os, const AABB& aabb);
};

} // namespace math
} // namespace engine
