#pragma once

#include <vector>
#include <cstdint>
#include <cassert>
#include <type_traits>

namespace engine {
namespace core {

/// ObjectPool — Fixed-size, zero-alloc object recycler.
///
/// Features:
///   - O(1) acquire/release via freelist
///   - No heap allocations after init
///   - Stable indices (pointers stay valid)
///   - forEach iterates only alive objects
///   - Compile-time type safety
///
/// Usage:
///   ObjectPool<Bullet> pool;
///   pool.init(1024);
///   uint32_t id = pool.acquire();        // O(1)
///   pool.get(id).position = {10, 20};
///   pool.release(id);                    // O(1)
///
template<typename T>
class ObjectPool {
public:
    ObjectPool() = default;
    ~ObjectPool() = default;

    /// Initialize pool with capacity (call once)
    void init(uint32_t capacity) {
        m_data.resize(capacity);
        m_alive.resize(capacity, false);
        m_freeList.resize(capacity);
        m_capacity = capacity;
        m_aliveCount = 0;

        // Build freelist (LIFO stack)
        for (uint32_t i = 0; i < capacity; i++) {
            m_freeList[i] = capacity - 1 - i;
        }
        m_freeTop = capacity;
    }

    /// Acquire an object from the pool (O(1))
    /// Returns index, or UINT32_MAX if pool is full
    uint32_t acquire() {
        if (m_freeTop == 0) return UINT32_MAX; // Pool full
        uint32_t idx = m_freeList[--m_freeTop];
        m_alive[idx] = true;
        m_data[idx] = T{}; // Reset to default
        m_aliveCount++;
        return idx;
    }

    /// Acquire without resetting the object (caller will initialize)
    uint32_t acquireRaw() {
        if (m_freeTop == 0) return UINT32_MAX;
        uint32_t idx = m_freeList[--m_freeTop];
        m_alive[idx] = true;
        m_aliveCount++;
        return idx;
    }

    /// Release an object back to the pool (O(1))
    void release(uint32_t index) {
        assert(index < m_capacity && "Index out of range");
        if (!m_alive[index]) return; // Already released
        m_alive[index] = false;
        m_freeList[m_freeTop++] = index;
        m_aliveCount--;
    }

    /// Release all objects at once
    void releaseAll() {
        m_freeTop = 0;
        for (uint32_t i = 0; i < m_capacity; i++) {
            m_alive[i] = false;
            m_freeList[m_freeTop++] = m_capacity - 1 - i;
        }
        m_aliveCount = 0;
    }

    /// Access object by index
    T& get(uint32_t index) {
        assert(index < m_capacity && m_alive[index]);
        return m_data[index];
    }

    const T& get(uint32_t index) const {
        assert(index < m_capacity && m_alive[index]);
        return m_data[index];
    }

    /// Check if an index is alive
    bool isAlive(uint32_t index) const {
        return index < m_capacity && m_alive[index];
    }

    /// Iterate over all alive objects
    template<typename Func>
    void forEach(Func&& func) {
        for (uint32_t i = 0; i < m_capacity; i++) {
            if (m_alive[i]) func(i, m_data[i]);
        }
    }

    template<typename Func>
    void forEach(Func&& func) const {
        for (uint32_t i = 0; i < m_capacity; i++) {
            if (m_alive[i]) func(i, m_data[i]);
        }
    }

    // ── Queries ────────────────────────────────────────────────

    uint32_t capacity() const { return m_capacity; }
    uint32_t aliveCount() const { return m_aliveCount; }
    uint32_t freeCount() const { return m_freeTop; }
    bool isFull() const { return m_freeTop == 0; }
    bool isEmpty() const { return m_aliveCount == 0; }

    /// Raw data access (for serialization, GPU upload, etc.)
    T* data() { return m_data.data(); }
    const T* data() const { return m_data.data(); }
    const std::vector<bool>& aliveFlags() const { return m_alive; }

private:
    std::vector<T> m_data;
    std::vector<bool> m_alive;
    std::vector<uint32_t> m_freeList;
    uint32_t m_capacity = 0;
    uint32_t m_freeTop = 0;
    uint32_t m_aliveCount = 0;
};

} // namespace core
} // namespace engine
