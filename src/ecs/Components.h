#pragma once

#include "Vector2D.h"
#include "Transform2D.h"
#include "AABB.h"
#include "Color.h"

namespace engine {
namespace ecs {

// ── Tags comunes ───────────────────────────────────────────────
enum Tags : uint8_t {
    TAG_PLAYER = 0,
    TAG_ENEMY,
    TAG_PROJECTILE,
    TAG_PLATFORM,
    TAG_PARTICLE,
};

// ── Componentes ────────────────────────────────────────────────

/// Posición, rotación y escala
struct TransformComponent {
    math::Transform2D transform;

    TransformComponent() = default;
    TransformComponent(const math::Vector2D& pos)
        : transform(pos) {}
    TransformComponent(const math::Vector2D& pos, float rot)
        : transform(pos, rot) {}
    TransformComponent(const math::Transform2D& t)
        : transform(t) {}
};

/// Datos de física — velocidad, fuerzas, masa, fricción, sleep
struct PhysicsComponent {
    math::Vector2D velocity;
    math::Vector2D acceleration;
    math::Vector2D previousPosition;   // Para interpolación
    float mass      = 1.0f;
    float invMass   = 1.0f;            // 0 = estático
    float drag      = 0.01f;
    float restitution = 0.3f;
    float friction  = 0.3f;            // Coeficiente de fricción [0-1]
    bool  isStatic  = false;
    bool  isSleeping = false;          // Resting contact — no procesar física
    float sleepTimer = 0.0f;           // Tiempo con velocidad < umbral

    static constexpr float SLEEP_THRESHOLD = 2.0f;  // Vel² bajo la cual se considera reposo
    static constexpr float SLEEP_TIME = 0.5f;        // Segundos antes de dormir

    PhysicsComponent() = default;

    PhysicsComponent(float m, bool stat = false)
        : mass(m)
        , invMass(stat ? 0.0f : (m > 0.0001f ? 1.0f / m : 0.0f))
        , isStatic(stat)
    {}
};

// ── Free functions for physics operations (ECS-correct) ────────
namespace PhysicsOps {

inline void applyForce(PhysicsComponent& phys, const math::Vector2D& force) {
    if (phys.isStatic) return;
    phys.acceleration += force * phys.invMass;
    phys.isSleeping = false;
    phys.sleepTimer = 0.0f;
}

inline void applyImpulse(PhysicsComponent& phys, const math::Vector2D& impulse) {
    if (phys.isStatic) return;
    phys.velocity += impulse * phys.invMass;
    phys.isSleeping = false;
    phys.sleepTimer = 0.0f;
}

inline void wake(PhysicsComponent& phys) {
    phys.isSleeping = false;
    phys.sleepTimer = 0.0f;
}

} // namespace PhysicsOps

/// Colisionador AABB + offset + triggers + layers
struct ColliderComponent {
    math::AABB aabb;
    math::Vector2D offset;          // Offset respecto al TransformComponent
    bool isStatic = false;
    bool isTrigger = false;         // Trigger: detecta overlap sin resolver colisión
    uint32_t layer = 1;             // Layer de este collider (bitmask)
    uint32_t collisionMask = 0xFFFFFFFF;  // Con qué layers colisiona

    ColliderComponent() = default;
    ColliderComponent(const math::Vector2D& size, bool stat = false)
        : aabb(math::AABB::fromCenter(math::Vector2D::Zero, size * 0.5f))
        , isStatic(stat)
    {}
    ColliderComponent(const math::AABB& box, bool stat = false)
        : aabb(box), isStatic(stat) {}

    /// ¿Debe colisionar con otra entidad?
    bool shouldCollide(const ColliderComponent& other) const {
        return (layer & other.collisionMask) != 0 &&
               (other.layer & collisionMask) != 0;
    }
};

/// Renderizado simple — color + tamaño + z-order
struct SpriteComponent {
    math::Color color;
    math::Vector2D size;
    int16_t zOrder = 0;     // -1000 fondo, 0 normal, 1000 frente

    SpriteComponent() : color(math::Color::white()), size(32, 32) {}
    SpriteComponent(const math::Color& c, const math::Vector2D& s, int16_t z = 0)
        : color(c), size(s), zOrder(z) {}
    SpriteComponent(const math::Color& c, float w, float h, int16_t z = 0)
        : color(c), size(w, h), zOrder(z) {}
};

} // namespace ecs
} // namespace engine
