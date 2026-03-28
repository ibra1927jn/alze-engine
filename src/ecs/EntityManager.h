#pragma once

#include <cstdint>
#include <bitset>
#include <vector>
#include <cassert>

namespace engine {
namespace ecs {

// ── Constantes ECS ─────────────────────────────────────────────
constexpr uint32_t MAX_ENTITIES    = 10000;
constexpr uint32_t MAX_COMPONENTS  = 64;
constexpr uint32_t MAX_TAGS        = 16;

// ── Tipos ──────────────────────────────────────────────────────
using Entity        = uint32_t;
using ComponentMask = std::bitset<MAX_COMPONENTS>;
using TagMask       = std::bitset<MAX_TAGS>;
using ComponentType = uint8_t;

constexpr Entity NULL_ENTITY = UINT32_MAX;

// ── Generational ID ────────────────────────────────────────────
// Entity = [12 bits generación][20 bits índice]
// Índice:     hasta 1,048,575 entidades
// Generación: hasta 4,095 versiones antes de wraparound

constexpr uint32_t INDEX_BITS = 20;
constexpr uint32_t GEN_BITS   = 12;
constexpr uint32_t INDEX_MASK = (1u << INDEX_BITS) - 1;  // 0xFFFFF
constexpr uint32_t GEN_MASK   = (1u << GEN_BITS) - 1;    // 0xFFF

inline uint32_t getIndex(Entity e)      { return e & INDEX_MASK; }
inline uint32_t getGeneration(Entity e) { return (e >> INDEX_BITS) & GEN_MASK; }
inline Entity   makeEntity(uint32_t idx, uint32_t gen) {
    return (gen << INDEX_BITS) | idx;
}

/// EntityManager — Gestiona la creación y destrucción de entidades.
///
/// Cada entidad tiene:
///   - Un ID generacional (20 bits índice + 12 bits versión)
///   - Una CMP Mask (qué componentes tiene)
///   - Una TAG Mask (tags para categorización)
///
/// Reutiliza IDs destruidos incrementando la generación,
/// lo que invalida automáticamente cualquier referencia stale.
///
class EntityManager {
public:
    EntityManager() {
        m_generations.resize(MAX_ENTITIES, 0);
        m_alive.resize(MAX_ENTITIES, false);
        m_componentMasks.resize(MAX_ENTITIES);
        m_tagMasks.resize(MAX_ENTITIES);
        m_alivePosByIndex.resize(MAX_ENTITIES, UINT32_MAX);

        m_freeIndices.reserve(MAX_ENTITIES);
        m_aliveList.reserve(1024);
        for (uint32_t i = MAX_ENTITIES; i > 0; i--) {
            m_freeIndices.push_back(i - 1);
        }
    }

    /// Crea una nueva entidad
    Entity createEntity() {
        assert(!m_freeIndices.empty() && "EntityManager: sin IDs libres!");
        uint32_t index = m_freeIndices.back();
        m_freeIndices.pop_back();
        m_alive[index] = true;
        m_componentMasks[index].reset();
        m_tagMasks[index].reset();
        m_activeCount++;

        // Add to dense alive list
        m_alivePosByIndex[index] = static_cast<uint32_t>(m_aliveList.size());
        m_aliveList.push_back(index);

        return makeEntity(index, m_generations[index]);
    }

    /// Destruye una entidad (invalida su generación)
    void destroyEntity(Entity entity) {
        uint32_t index = getIndex(entity);
        uint32_t gen   = getGeneration(entity);

        // Validar generación (protección contra stale)
        if (gen != m_generations[index] || !m_alive[index]) return;

        m_alive[index] = false;
        m_componentMasks[index].reset();
        m_tagMasks[index].reset();

        // Remove from dense alive list (swap-and-pop)
        uint32_t pos = m_alivePosByIndex[index];
        if (pos < m_aliveList.size()) {
            uint32_t lastIdx = m_aliveList.back();
            m_aliveList[pos] = lastIdx;
            m_alivePosByIndex[lastIdx] = pos;
            m_aliveList.pop_back();
            m_alivePosByIndex[index] = UINT32_MAX;
        }

        m_generations[index] = (m_generations[index] + 1) & GEN_MASK;
        m_freeIndices.push_back(index);
        m_activeCount--;
    }

    /// ¿Sigue viva esta entidad? (valida generación)
    bool isAlive(Entity entity) const {
        uint32_t index = getIndex(entity);
        uint32_t gen   = getGeneration(entity);
        return index < MAX_ENTITIES && m_alive[index] && m_generations[index] == gen;
    }

    // ── CMP Mask ───────────────────────────────────────────────

    void setComponentBit(Entity entity, ComponentType type, bool on) {
        uint32_t index = getIndex(entity);
        if (on) m_componentMasks[index].set(type);
        else    m_componentMasks[index].reset(type);
    }

    ComponentMask getComponentMask(Entity entity) const {
        return m_componentMasks[getIndex(entity)];
    }

    /// ¿Tiene esta entidad todos los bits de la signature?
    bool matchesSignature(Entity entity, const ComponentMask& signature) const {
        return (m_componentMasks[getIndex(entity)] & signature) == signature;
    }

    // ── TAG Mask ───────────────────────────────────────────────

    void setTag(Entity entity, uint8_t tag, bool on) {
        uint32_t index = getIndex(entity);
        if (on) m_tagMasks[index].set(tag);
        else    m_tagMasks[index].reset(tag);
    }

    bool hasTag(Entity entity, uint8_t tag) const {
        return m_tagMasks[getIndex(entity)].test(tag);
    }

    TagMask getTagMask(Entity entity) const {
        return m_tagMasks[getIndex(entity)];
    }

    // ── Info ───────────────────────────────────────────────────

    uint32_t getActiveCount() const { return m_activeCount; }

    friend class ECSCoordinator;
    friend class QueryCache;

private:
    std::vector<uint32_t>     m_generations;
    std::vector<bool>         m_alive;
    std::vector<ComponentMask> m_componentMasks;
    std::vector<TagMask>       m_tagMasks;
    std::vector<uint32_t>     m_freeIndices;
    std::vector<uint32_t>     m_aliveList;         // Dense list of alive indices
    std::vector<uint32_t>     m_alivePosByIndex;   // index → position in aliveList
    uint32_t                  m_activeCount = 0;
};

} // namespace ecs
} // namespace engine
