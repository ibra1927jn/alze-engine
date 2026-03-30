#pragma once

#include "EntityManager.h"
#include <vector>
#include <cassert>

namespace engine {
namespace ecs {

/// ComponentStorage<T> — Dense array + flat sparse array (O(1) lookup).
///
/// Uses a flat vector<uint32_t> for the sparse mapping instead of unordered_map.
/// Lookup is a single array access: sparse[entityIndex] → dense position.
/// This eliminates hash computation, bucket traversal, and cache misses.
///
///   Cost comparison (per lookup):
///     unordered_map: ~80ns (hash + compare + cache miss)
///     flat array:    ~1ns  (single indexed load)
///
///   dense[]     → datos T contiguos (lo que iteran los Systems)
///   sparse[]    → entityIndex → posición en dense (INVALID = not present)
///   entities[]  → posición en dense → Entity (para saber quién es)
///
/// Remove usa Swap-and-Pop:
///   1. Intercambia el componente a borrar con el último
///   2. Actualiza el sparse del movido
///   3. pop_back → O(1), sin huecos, 100% contiguo
///
template<typename T>
class ComponentStorage {
public:
    static constexpr uint32_t INVALID = 0xFFFFFFFF;

    ComponentStorage() {
        // Pre-allocate sparse for first 1024 entities
        m_sparse.resize(1024, INVALID);
        m_dense.reserve(64);
        m_entities.reserve(64);
    }

    /// Añade un componente a la entidad (O(1))
    void add(Entity entity, const T& component) {
        uint32_t index = getIndex(entity);
        ensureSparseSize(index);
        assert(m_sparse[index] == INVALID && "Entity ya tiene este componente!");

        uint32_t denseIndex = static_cast<uint32_t>(m_dense.size());
        m_sparse[index] = denseIndex;
        m_dense.push_back(component);
        m_entities.push_back(entity);
    }

    /// Añade un componente construido in-place
    template<typename... Args>
    T& emplace(Entity entity, Args&&... args) {
        uint32_t index = getIndex(entity);
        ensureSparseSize(index);
        assert(m_sparse[index] == INVALID && "Entity ya tiene este componente!");

        uint32_t denseIndex = static_cast<uint32_t>(m_dense.size());
        m_sparse[index] = denseIndex;
        m_dense.emplace_back(std::forward<Args>(args)...);
        m_entities.push_back(entity);
        return m_dense.back();
    }

    /// Elimina el componente de la entidad — Swap-and-Pop O(1)
    void remove(Entity entity) {
        uint32_t index = getIndex(entity);
        if (index >= m_sparse.size() || m_sparse[index] == INVALID) return;

        uint32_t denseIndex = m_sparse[index];
        uint32_t lastDense = static_cast<uint32_t>(m_dense.size()) - 1;

        if (denseIndex != lastDense) {
            // Swap: mover el último al hueco
            m_dense[denseIndex] = std::move(m_dense[lastDense]);
            m_entities[denseIndex] = m_entities[lastDense];

            // Actualizar sparse del elemento movido
            uint32_t movedIndex = getIndex(m_entities[denseIndex]);
            m_sparse[movedIndex] = denseIndex;
        }

        // Pop
        m_dense.pop_back();
        m_entities.pop_back();
        m_sparse[index] = INVALID;
    }

    /// Obtiene referencia al componente (O(1) — single array access!)
    T& get(Entity entity) {
        uint32_t index = getIndex(entity);
        assert(index < m_sparse.size() && m_sparse[index] != INVALID && "Entity no tiene este componente!");
        return m_dense[m_sparse[index]];
    }

    const T& get(Entity entity) const {
        uint32_t index = getIndex(entity);
        assert(index < m_sparse.size() && m_sparse[index] != INVALID && "Entity no tiene este componente!");
        return m_dense[m_sparse[index]];
    }

    /// ¿Tiene esta entidad este componente? — O(1)
    bool has(Entity entity) const {
        uint32_t index = getIndex(entity);
        return index < m_sparse.size() && m_sparse[index] != INVALID;
    }

    /// Entidad asociada a la posición i del dense array
    Entity getEntity(uint32_t denseIndex) const {
        return m_entities[denseIndex];
    }

    // ── Iteración sobre dense[] (para Systems) ─────────────────

    T* begin() { return m_dense.data(); }
    T* end()   { return m_dense.data() + m_dense.size(); }
    const T* begin() const { return m_dense.data(); }
    const T* end()   const { return m_dense.data() + m_dense.size(); }

    uint32_t size() const { return static_cast<uint32_t>(m_dense.size()); }
    bool empty() const { return m_dense.empty(); }

    // Acceso directo para iteration patterns
    T& getDense(uint32_t i) { return m_dense[i]; }
    const T& getDense(uint32_t i) const { return m_dense[i]; }

private:
    void ensureSparseSize(uint32_t index) {
        if (index >= m_sparse.size()) {
            // Grow by doubling (amortized O(1) growth)
            size_t newSize = m_sparse.size();
            while (newSize <= index) newSize *= 2;
            m_sparse.resize(newSize, INVALID);
        }
    }

    std::vector<T>        m_dense;       // Componentes contiguos (cache-friendly)
    std::vector<uint32_t> m_sparse;      // entityIndex → dense pos (INVALID = absent)
    std::vector<Entity>   m_entities;    // posición en dense → Entity
};

} // namespace ecs
} // namespace engine
