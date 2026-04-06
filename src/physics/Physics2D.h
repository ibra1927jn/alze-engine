#pragma once

/// Sistema de fisica 2D completo — header-only.
///
/// Incluye:
///   - AABB2D, Circle2D con deteccion de colisiones
///   - RigidBody2D con integracion semi-implicit Euler
///   - Broadphase via spatial hash grid
///   - Resolucion de colisiones con restitution y friccion
///
/// Uso tipico:
///   PhysicsWorld2D world;
///   world.setGravity({0, 980});
///   auto id = world.addBody({...});
///   world.step(dt);
///

#include "math/Vector2D.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace engine {
namespace physics {

// ════════════════════════════════════════════════════════════════
// Shapes 2D
// ════════════════════════════════════════════════════════════════

/// Axis-Aligned Bounding Box 2D
struct AABB2D {
    math::Vector2D min;
    math::Vector2D max;

    AABB2D() = default;
    AABB2D(const math::Vector2D& mn, const math::Vector2D& mx)
        : min(mn), max(mx) {}

    /// Construir desde centro y half-extents
    static AABB2D fromCenter(const math::Vector2D& center, const math::Vector2D& half) {
        return { center - half, center + half };
    }

    math::Vector2D center() const { return (min + max) * 0.5f; }
    math::Vector2D halfExtents() const { return (max - min) * 0.5f; }
    float width()  const { return max.x - min.x; }
    float height() const { return max.y - min.y; }

    bool contains(const math::Vector2D& p) const {
        return p.x >= min.x && p.x <= max.x &&
               p.y >= min.y && p.y <= max.y;
    }

    /// Test de interseccion AABB vs AABB
    static bool intersects(const AABB2D& a, const AABB2D& b) {
        if (a.max.x < b.min.x || a.min.x > b.max.x) return false;
        if (a.max.y < b.min.y || a.min.y > b.max.y) return false;
        return true;
    }
};

/// Circulo 2D
struct Circle2D {
    math::Vector2D center;
    float radius = 0.5f;

    Circle2D() = default;
    Circle2D(const math::Vector2D& c, float r) : center(c), radius(r) {}

    /// AABB que encierra este circulo
    AABB2D bounds() const {
        math::Vector2D half(radius, radius);
        return { center - half, center + half };
    }

    /// Test de interseccion Circle vs Circle
    static bool intersects(const Circle2D& a, const Circle2D& b) {
        math::Vector2D delta = b.center - a.center;
        float distSq = delta.sqrMagnitude();
        float sumR = a.radius + b.radius;
        return distSq <= sumR * sumR;
    }

    /// Test de interseccion Circle vs AABB
    static bool intersectsAABB(const Circle2D& c, const AABB2D& box) {
        // Punto mas cercano del AABB al centro del circulo
        float closestX = std::max(box.min.x, std::min(c.center.x, box.max.x));
        float closestY = std::max(box.min.y, std::min(c.center.y, box.max.y));
        float dx = c.center.x - closestX;
        float dy = c.center.y - closestY;
        return (dx * dx + dy * dy) <= (c.radius * c.radius);
    }
};

// ════════════════════════════════════════════════════════════════
// Resultado de colision 2D
// ════════════════════════════════════════════════════════════════

struct Collision2D {
    uint32_t bodyA = 0;
    uint32_t bodyB = 0;
    math::Vector2D normal;     // Apunta de A hacia B
    float penetration = 0.0f;
    math::Vector2D contactPoint;
};

// ════════════════════════════════════════════════════════════════
// RigidBody2D
// ════════════════════════════════════════════════════════════════

struct RigidBody2D {
    enum class Shape : uint8_t { CIRCLE, AABB };

    // Estado cinematico
    math::Vector2D position;
    math::Vector2D velocity;
    math::Vector2D acceleration; // Se limpia cada frame

    // Propiedades fisicas
    float mass        = 1.0f;
    float invMass     = 1.0f;
    float restitution = 0.5f;
    float friction    = 0.3f;

    // Shape
    Shape shapeType = Shape::CIRCLE;
    float radius    = 0.5f;                         // Para CIRCLE
    math::Vector2D halfExtents = {0.5f, 0.5f};     // Para AABB

    bool isStatic = false;
    bool active   = true;

    RigidBody2D() = default;

    static RigidBody2D circle(const math::Vector2D& pos, float r, float m) {
        RigidBody2D b;
        b.position = pos;
        b.radius = r;
        b.mass = m;
        b.invMass = (m > 1e-6f) ? 1.0f / m : 0.0f;
        b.shapeType = Shape::CIRCLE;
        return b;
    }

    static RigidBody2D box(const math::Vector2D& pos, const math::Vector2D& half, float m) {
        RigidBody2D b;
        b.position = pos;
        b.halfExtents = half;
        b.mass = m;
        b.invMass = (m > 1e-6f) ? 1.0f / m : 0.0f;
        b.shapeType = Shape::AABB;
        return b;
    }

    static RigidBody2D staticCircle(const math::Vector2D& pos, float r) {
        RigidBody2D b;
        b.position = pos;
        b.radius = r;
        b.mass = 0.0f;
        b.invMass = 0.0f;
        b.isStatic = true;
        b.shapeType = Shape::CIRCLE;
        return b;
    }

    static RigidBody2D staticBox(const math::Vector2D& pos, const math::Vector2D& half) {
        RigidBody2D b;
        b.position = pos;
        b.halfExtents = half;
        b.mass = 0.0f;
        b.invMass = 0.0f;
        b.isStatic = true;
        b.shapeType = Shape::AABB;
        return b;
    }

    void applyForce(const math::Vector2D& force) {
        if (!isStatic) acceleration += force * invMass;
    }

    void applyImpulse(const math::Vector2D& impulse) {
        if (!isStatic) velocity += impulse * invMass;
    }

    AABB2D bounds() const {
        if (shapeType == Shape::CIRCLE) {
            math::Vector2D half(radius, radius);
            return { position - half, position + half };
        }
        return { position - halfExtents, position + halfExtents };
    }
};

// ════════════════════════════════════════════════════════════════
// Spatial Hash Grid — Broadphase 2D
// ════════════════════════════════════════════════════════════════

class SpatialHash2D {
public:
    SpatialHash2D(float cellSize = 64.0f, int gridDim = 128)
        : m_cellSize(cellSize)
        , m_invCellSize(1.0f / cellSize)
        , m_gridDim(gridDim)
    {
        m_cells.resize(gridDim * gridDim);
    }

    void clear() {
        for (auto& cell : m_usedCells) {
            m_cells[cell].clear();
        }
        m_usedCells.clear();
    }

    void insert(uint32_t id, const AABB2D& aabb) {
        int x0 = cellX(aabb.min.x);
        int y0 = cellY(aabb.min.y);
        int x1 = cellX(aabb.max.x);
        int y1 = cellY(aabb.max.y);

        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                int idx = cellIndex(x, y);
                if (idx < 0 || idx >= static_cast<int>(m_cells.size())) continue;
                if (m_cells[idx].empty()) m_usedCells.push_back(idx);
                m_cells[idx].push_back(id);
            }
        }
    }

    /// Devuelve pares candidatos (sin duplicados)
    std::vector<std::pair<uint32_t, uint32_t>> queryPairs() const {
        std::vector<std::pair<uint32_t, uint32_t>> pairs;
        for (int cellIdx : m_usedCells) {
            const auto& cell = m_cells[cellIdx];
            for (size_t i = 0; i < cell.size(); ++i) {
                for (size_t j = i + 1; j < cell.size(); ++j) {
                    uint32_t a = cell[i], b = cell[j];
                    if (a > b) std::swap(a, b);
                    pairs.push_back({a, b});
                }
            }
        }
        // Eliminar duplicados (pares que aparecen en multiples celdas)
        std::sort(pairs.begin(), pairs.end());
        pairs.erase(std::unique(pairs.begin(), pairs.end()), pairs.end());
        return pairs;
    }

private:
    int cellX(float x) const { return std::max(0, std::min(m_gridDim - 1, static_cast<int>(x * m_invCellSize))); }
    int cellY(float y) const { return std::max(0, std::min(m_gridDim - 1, static_cast<int>(y * m_invCellSize))); }
    int cellIndex(int x, int y) const { return y * m_gridDim + x; }

    float m_cellSize = 0.0f;
    float m_invCellSize = 0.0f;
    int   m_gridDim = 0;
    std::vector<std::vector<uint32_t>> m_cells;
    std::vector<int> m_usedCells;
};

// ════════════════════════════════════════════════════════════════
// PhysicsWorld2D — Mundo de fisica 2D
// ════════════════════════════════════════════════════════════════

class PhysicsWorld2D {
public:
    PhysicsWorld2D(float gravity = 980.0f, float cellSize = 64.0f)
        : m_gravity(0.0f, gravity)
        , m_broadphase(cellSize)
    {}

    // ── Configuracion ──────────────────────────────────────────
    void setGravity(const math::Vector2D& g) { m_gravity = g; }
    math::Vector2D getGravity() const { return m_gravity; }

    // ── Gestion de cuerpos ─────────────────────────────────────
    uint32_t addBody(const RigidBody2D& body) {
        uint32_t id = static_cast<uint32_t>(m_bodies.size());
        m_bodies.push_back(body);
        return id;
    }

    RigidBody2D& getBody(uint32_t id) { return m_bodies[id]; }
    const RigidBody2D& getBody(uint32_t id) const { return m_bodies[id]; }
    size_t bodyCount() const { return m_bodies.size(); }

    const std::vector<Collision2D>& getCollisions() const { return m_collisions; }

    // ── Simulacion ─────────────────────────────────────────────

    /// Avanzar un step de simulacion (semi-implicit Euler)
    void step(float dt) {
        // 1. Integracion: aplicar gravedad y mover
        for (auto& body : m_bodies) {
            if (!body.active || body.isStatic) continue;

            // Semi-implicit Euler: actualizar velocidad primero, luego posicion
            body.velocity += (body.acceleration + m_gravity) * dt;
            body.position += body.velocity * dt;

            // Limpiar fuerzas acumuladas
            body.acceleration = math::Vector2D(0, 0);
        }

        // 2. Broadphase
        m_broadphase.clear();
        for (uint32_t i = 0; i < static_cast<uint32_t>(m_bodies.size()); ++i) {
            if (!m_bodies[i].active) continue;
            m_broadphase.insert(i, m_bodies[i].bounds());
        }

        // 3. Narrowphase + resolucion
        m_collisions.clear();
        auto pairs = m_broadphase.queryPairs();

        for (const auto& pair : pairs) {
            RigidBody2D& bodyA = m_bodies[pair.first];
            RigidBody2D& bodyB = m_bodies[pair.second];
            if (!bodyA.active || !bodyB.active) continue;
            if (bodyA.isStatic && bodyB.isStatic) continue;

            Collision2D col;
            col.bodyA = pair.first;
            col.bodyB = pair.second;

            bool hit = narrowphase(bodyA, bodyB, col);
            if (hit) {
                resolveCollision(bodyA, bodyB, col);
                m_collisions.push_back(col);
            }
        }
    }

private:
    // ── Narrowphase: deteccion exacta ──────────────────────────

    bool narrowphase(const RigidBody2D& a, const RigidBody2D& b, Collision2D& col) {
        if (a.shapeType == RigidBody2D::Shape::CIRCLE && b.shapeType == RigidBody2D::Shape::CIRCLE) {
            return circleVsCircle(a, b, col);
        }
        if (a.shapeType == RigidBody2D::Shape::AABB && b.shapeType == RigidBody2D::Shape::AABB) {
            return aabbVsAabb(a, b, col);
        }
        // Circle vs AABB (normalizar orden: A=circle, B=aabb)
        if (a.shapeType == RigidBody2D::Shape::CIRCLE && b.shapeType == RigidBody2D::Shape::AABB) {
            return circleVsAabb(a, b, col, false);
        }
        if (a.shapeType == RigidBody2D::Shape::AABB && b.shapeType == RigidBody2D::Shape::CIRCLE) {
            return circleVsAabb(b, a, col, true);
        }
        return false;
    }

    bool circleVsCircle(const RigidBody2D& a, const RigidBody2D& b, Collision2D& col) {
        math::Vector2D delta = b.position - a.position;
        float distSq = delta.sqrMagnitude();
        float sumR = a.radius + b.radius;

        if (distSq > sumR * sumR) return false;
        if (distSq < 1e-10f) {
            // Circulos coincidentes — normal arbitraria
            col.normal = math::Vector2D(0, 1);
            col.penetration = sumR;
            col.contactPoint = a.position;
            return true;
        }

        float dist = std::sqrt(distSq);
        col.normal = delta * (1.0f / dist);
        col.penetration = sumR - dist;
        col.contactPoint = a.position + col.normal * a.radius;
        return true;
    }

    bool aabbVsAabb(const RigidBody2D& a, const RigidBody2D& b, Collision2D& col) {
        math::Vector2D delta = b.position - a.position;
        float overlapX = (a.halfExtents.x + b.halfExtents.x) - std::abs(delta.x);
        if (overlapX <= 0) return false;
        float overlapY = (a.halfExtents.y + b.halfExtents.y) - std::abs(delta.y);
        if (overlapY <= 0) return false;

        // Eje de menor penetracion
        if (overlapX < overlapY) {
            col.normal = math::Vector2D(delta.x > 0 ? 1.0f : -1.0f, 0.0f);
            col.penetration = overlapX;
        } else {
            col.normal = math::Vector2D(0.0f, delta.y > 0 ? 1.0f : -1.0f);
            col.penetration = overlapY;
        }
        col.contactPoint = (a.position + b.position) * 0.5f;
        return true;
    }

    bool circleVsAabb(const RigidBody2D& circle, const RigidBody2D& box,
                       Collision2D& col, bool flipped) {
        // Punto mas cercano del AABB al centro del circulo
        float closestX = std::max(box.position.x - box.halfExtents.x,
                         std::min(circle.position.x, box.position.x + box.halfExtents.x));
        float closestY = std::max(box.position.y - box.halfExtents.y,
                         std::min(circle.position.y, box.position.y + box.halfExtents.y));

        math::Vector2D closest(closestX, closestY);
        math::Vector2D delta = circle.position - closest;
        float distSq = delta.sqrMagnitude();

        if (distSq > circle.radius * circle.radius) return false;

        if (distSq < 1e-10f) {
            // Centro dentro del AABB — buscar eje de menor penetracion
            float dx = (box.position.x + box.halfExtents.x) - circle.position.x;
            float dy = (box.position.y + box.halfExtents.y) - circle.position.y;
            if (dx < dy) {
                col.normal = math::Vector2D(1, 0);
                col.penetration = dx + circle.radius;
            } else {
                col.normal = math::Vector2D(0, 1);
                col.penetration = dy + circle.radius;
            }
        } else {
            float dist = std::sqrt(distSq);
            col.normal = delta * (1.0f / dist);
            col.penetration = circle.radius - dist;
        }

        // Si se invirtio el orden, invertir la normal
        if (flipped) col.normal = -col.normal;

        col.contactPoint = closest;
        return true;
    }

    // ── Resolucion de colisiones ───────────────────────────────

    void resolveCollision(RigidBody2D& a, RigidBody2D& b, const Collision2D& col) {
        float invMassSum = a.invMass + b.invMass;
        if (invMassSum < 1e-10f) return;

        // Separar cuerpos (correccion de posicion)
        math::Vector2D correction = col.normal * (col.penetration / invMassSum) * 0.8f;
        a.position -= correction * a.invMass;
        b.position += correction * b.invMass;

        // Velocidad relativa a lo largo de la normal
        math::Vector2D relVel = b.velocity - a.velocity;
        float velAlongNormal = relVel.dot(col.normal);

        // Solo resolver si los cuerpos se acercan
        if (velAlongNormal > 0) return;

        // Restitution: coeficiente minimo
        float e = std::min(a.restitution, b.restitution);

        // Impulso normal
        float j = -(1.0f + e) * velAlongNormal / invMassSum;
        math::Vector2D impulse = col.normal * j;

        a.velocity -= impulse * a.invMass;
        b.velocity += impulse * b.invMass;

        // Friccion tangencial
        relVel = b.velocity - a.velocity;
        math::Vector2D tangent = relVel - col.normal * relVel.dot(col.normal);
        float tangentLen = tangent.magnitude();
        if (tangentLen > 1e-6f) {
            tangent = tangent * (1.0f / tangentLen);
            float jt = -relVel.dot(tangent) / invMassSum;

            // Coulomb friction: clamp al cono de friccion
            float mu = (a.friction + b.friction) * 0.5f;
            math::Vector2D frictionImpulse;
            if (std::abs(jt) < j * mu) {
                frictionImpulse = tangent * jt;
            } else {
                frictionImpulse = tangent * (-j * mu);
            }

            a.velocity -= frictionImpulse * a.invMass;
            b.velocity += frictionImpulse * b.invMass;
        }
    }

    // ── Datos ──────────────────────────────────────────────────

    math::Vector2D m_gravity;
    std::vector<RigidBody2D> m_bodies;
    std::vector<Collision2D> m_collisions;
    SpatialHash2D m_broadphase;
};

} // namespace physics
} // namespace engine
