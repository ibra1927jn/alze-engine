#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <functional>
#include <cassert>
#include <vector>
#include <future>
#include <mutex>

namespace engine {
namespace core {

/// ResourceManager — Asset cache with ref-counting, async loading, and GC.
///
/// Features:
///   - Flyweight pattern (no duplicate loads)
///   - weak_ptr cache (auto-release when unused)
///   - Async loading via std::future
///   - Batch preload
///   - Thread-safe with mutex
///   - Stats (hit rate, alive count)
///
template<typename T>
class ResourceManager {
public:
    using Loader = std::function<std::shared_ptr<T>(const std::string&)>;

    void setLoader(Loader loader) { m_loader = loader; }

    /// Get a resource (loads if not cached)
    std::shared_ptr<T> get(const std::string& id) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return getInternal(id);
    }

    void preload(const std::string& id) { get(id); }

    /// Batch preload multiple resources
    void preloadBatch(const std::vector<std::string>& ids) {
        for (auto& id : ids) get(id);
    }

    /// Async load — returns a future
    std::future<std::shared_ptr<T>> getAsync(const std::string& id) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_cache.find(id);
            if (it != m_cache.end()) {
                auto locked = it->second.lock();
                if (locked) {
                    m_cacheHits++;
                    std::promise<std::shared_ptr<T>> p;
                    p.set_value(locked);
                    return p.get_future();
                }
            }
        }
        return std::async(std::launch::async, [this, id]() {
            assert(m_loader && "ResourceManager: loader not set!");
            auto resource = m_loader(id);
            if (resource) {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_cache[id] = resource;
                m_loads++;
            }
            return resource;
        });
    }

    void insert(const std::string& id, std::shared_ptr<T> resource) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_cache[id] = resource;
    }

    bool has(const std::string& id) const {
        std::lock_guard<std::mutex> lock(m_mutex); // Proteger contra data race
        auto it = m_cache.find(id);
        return it != m_cache.end() && !it->second.expired();
    }

    /// Release unused resources
    int collectGarbage() {
        std::lock_guard<std::mutex> lock(m_mutex);
        int freed = 0;
        for (auto it = m_cache.begin(); it != m_cache.end();) {
            if (it->second.expired()) { it = m_cache.erase(it); freed++; }
            else ++it;
        }
        return freed;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_cache.clear();
    }

    // ── Stats ──────────────────────────────────────────────────
    int getCacheSize() const { return static_cast<int>(m_cache.size()); }
    int getCacheHits() const { return m_cacheHits; }
    int getTotalLoads() const { return m_loads; }
    float getHitRate() const {
        int total = m_cacheHits + m_loads;
        return total > 0 ? static_cast<float>(m_cacheHits) / total : 0.0f;
    }

    int getAliveCount() const {
        std::lock_guard<std::mutex> lock(m_mutex); // Proteger contra data race
        int count = 0;
        for (auto& [id, wp] : m_cache)
            if (!wp.expired()) count++;
        return count;
    }

    template<typename Func>
    void forEachAlive(Func&& fn) {
        for (auto& [id, wp] : m_cache) {
            auto sp = wp.lock();
            if (sp) fn(id, sp);
        }
    }

private:
    std::shared_ptr<T> getInternal(const std::string& id) {
        auto it = m_cache.find(id);
        if (it != m_cache.end()) {
            auto locked = it->second.lock();
            if (locked) { m_cacheHits++; return locked; }
            m_cache.erase(it);
        }
        assert(m_loader && "ResourceManager: loader not set!");
        auto resource = m_loader(id);
        if (resource) { m_cache[id] = resource; m_loads++; }
        return resource;
    }

    std::unordered_map<std::string, std::weak_ptr<T>> m_cache;
    Loader m_loader;
    mutable std::mutex m_mutex;
    int m_cacheHits = 0;
    int m_loads     = 0;
};

} // namespace core
} // namespace engine
