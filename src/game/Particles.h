#pragma once

#include "math/Vector2D.h"
#include "math/Color.h"
#include <vector>
#include <cstdlib>

namespace engine {
namespace game {

// ── Config ─────────────────────────────────────────────────────
namespace Config {
    constexpr float GRAVITY          = 980.0f;
    constexpr float MOVE_FORCE       = 2000.0f;
    constexpr float JUMP_IMPULSE     = 420.0f;
    constexpr float GROUND_DRAG      = 6.0f;
    constexpr float AIR_DRAG         = 0.5f;
    constexpr float MAX_VELOCITY_X   = 500.0f;
    constexpr float MAX_VELOCITY_Y   = 800.0f;
    constexpr float COYOTE_TIME      = 0.12f;
    constexpr float JUMP_BUFFER      = 0.10f;
    constexpr float PLAYER_W         = 28.0f;
    constexpr float PLAYER_H         = 28.0f;
    constexpr float PLAYER_MASS      = 1.5f;
    constexpr float RESTITUTION      = 0.15f;
    constexpr int   MAX_PARTICLES    = 500;
    constexpr int   HIT_CACHE_SIZE   = 8;
}

// ── Particle (pool-friendly) ───────────────────────────────────
struct Particle {
    math::Vector2D pos, prevPos, vel;
    math::Color    color;
    float size = 0, life = 0, maxLife = 0;
    bool  active = false;

    void update(float dt, float gravity) {
        prevPos = pos;
        vel.y += gravity * dt;
        vel *= (1.0f - 1.5f * dt);
        pos += vel * dt;
        life -= dt;
        if (life <= 0.0f) active = false;
    }
};

// ── Particle Pool (fixed-size, free list, no alloc) ────────────
class ParticlePool {
public:
    ParticlePool() {
        m_pool.resize(Config::MAX_PARTICLES);
        m_firstFree = 0;
        for (int i = 0; i < Config::MAX_PARTICLES - 1; i++)
            m_nextFree.push_back(i + 1);
        m_nextFree.push_back(-1);
    }

    Particle* spawn() {
        if (m_firstFree < 0) return nullptr;
        int idx = m_firstFree;
        m_firstFree = m_nextFree[idx];
        return &m_pool[idx];
    }

    void update(float dt, float gravity) {
        m_activeCount = 0;
        for (int i = 0; i < static_cast<int>(m_pool.size()); i++) {
            auto& p = m_pool[i];
            if (p.active) {
                p.update(dt, gravity);
                if (!p.active) {
                    m_nextFree[i] = m_firstFree;
                    m_firstFree = i;
                } else {
                    m_activeCount++;
                }
            }
        }
    }

    void spawnBurst(math::Vector2D origin, int count, float force, math::Color base) {
        for (int i = 0; i < count; i++) {
            Particle* p = spawn();
            if (!p) break;
            float angle = randf(0, math::MathUtils::TWO_PI);
            float spd = randf(force * 0.3f, force);
            p->pos = origin;
            p->prevPos = origin;
            p->vel = {std::cos(angle) * spd, std::sin(angle) * spd};
            p->color = (rand() % 3 == 0) ? randColor() : base;
            p->size = randf(2, 6);
            p->life = randf(0.5f, 2.0f);
            p->maxLife = p->life;
            p->active = true;
        }
    }

    void clear() { for (auto& p : m_pool) p.active = false; m_activeCount = 0; }
    int activeCount() const { return m_activeCount; }
    const std::vector<Particle>& pool() const { return m_pool; }

private:
    static float randf(float lo, float hi) {
        return lo + static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * (hi - lo);
    }
    static math::Color randColor() {
        const math::Color p[] = {
            math::Color::green(), math::Color::red(),    math::Color::blue(),
            math::Color::yellow(),math::Color::purple(), math::Color::orange(),
            math::Color::cyan(),  math::Color::magenta()
        };
        return p[rand() % 8];
    }

    std::vector<Particle> m_pool;
    std::vector<int>      m_nextFree;
    int                   m_firstFree = 0;
    int m_activeCount = 0;
};

} // namespace game
} // namespace engine
