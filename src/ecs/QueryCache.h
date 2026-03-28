#pragma once

#include "EntityManager.h"
#include <vector>
#include <unordered_map>
#include <functional>

namespace engine {
namespace ecs {

// ═══════════════════════════════════════════════════════════════
// QueryCache — Memoizes entity lists per component signature
//
// Problem: forEach<T1, T2>() iterates all alive entities every
// call, checking component masks. For hot loops this is wasteful.
//
// Solution: Cache the matching entity list per signature.
// Invalidate when entities are created/destroyed or components
// change. Gives O(matched) iteration instead of O(alive).
// ═══════════════════════════════════════════════════════════════

class QueryCache {
public:
    using EntityList = std::vector<Entity>;

    /// Get or build a cached entity list for a signature
    const EntityList& getMatching(const ComponentMask& sig, const EntityManager& em) {
        uint64_t key = hashMask(sig);
        auto it = m_cache.find(key);

        if (it != m_cache.end() && it->second.generation == m_generation) {
            return it->second.entities;
        }

        // Cache miss — build the list
        CachedQuery& cq = m_cache[key];
        cq.entities.clear();
        cq.signature = sig;
        cq.generation = m_generation;

        const auto& aliveList = em.m_aliveList;
        for (uint32_t i = 0; i < aliveList.size(); i++) {
            uint32_t idx = aliveList[i];
            if ((em.m_componentMasks[idx] & sig) == sig) {
                cq.entities.push_back(makeEntity(idx, em.m_generations[idx]));
            }
        }

        return cq.entities;
    }

    /// Invalidate all caches (call when entities/components change)
    void invalidate() {
        m_generation++;
    }

    /// Clear all cached data
    void clear() {
        m_cache.clear();
        m_generation++;
    }

    /// Number of cached queries
    size_t getCachedQueryCount() const { return m_cache.size(); }

    /// Check if a specific signature is cached and valid
    bool isCached(const ComponentMask& sig) const {
        auto it = m_cache.find(hashMask(sig));
        return it != m_cache.end() && it->second.generation == m_generation;
    }

private:
    struct CachedQuery {
        EntityList entities;
        ComponentMask signature;
        uint64_t generation = 0;
    };

    std::unordered_map<uint64_t, CachedQuery> m_cache;
    uint64_t m_generation = 0;

    /// Hash a ComponentMask to a uint64_t key
    static uint64_t hashMask(const ComponentMask& mask) {
        // ComponentMask is a std::bitset<64>, convert to uint64
        // Use FNV-1a on the bitset
        uint64_t hash = 14695981039346656037ULL;
        // Iterate bits in groups of 8
        for (size_t i = 0; i < 64; i += 8) {
            uint8_t byte = 0;
            for (size_t b = 0; b < 8 && (i + b) < 64; b++) {
                if (mask.test(i + b)) byte |= (1u << b);
            }
            hash ^= byte;
            hash *= 1099511628211ULL;
        }
        return hash;
    }
};

} // namespace ecs
} // namespace engine
