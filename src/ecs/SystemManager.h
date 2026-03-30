#pragma once

#include "EntityManager.h"
#include <vector>
#include <memory>
#include <algorithm>
#include <atomic>
#include <cstdint>

namespace engine {
namespace ecs {

// ── Compile-time System Type IDs (no RTTI needed) ─────────────
namespace detail {
    inline uint32_t nextSystemTypeId() {
        static std::atomic<uint32_t> counter{0};
        return counter.fetch_add(1, std::memory_order_relaxed);
    }

    template<typename T>
    uint32_t getSystemTypeId() {
        static uint32_t id = nextSystemTypeId();
        return id;
    }
}

/// System — Clase base para todos los sistemas del ECS.
///
/// Cada sistema define una signature (bitset) que indica qué componentes
/// necesita. Solo procesa entidades cuya CMP mask contiene todos los bits.
///
class System {
public:
    virtual ~System() = default;

    /// Ejecuta la lógica del sistema
    virtual void update(float dt) = 0;

    /// La signature define qué componentes requiere este sistema
    ComponentMask signature;

    /// Prioridad de ejecución (menor = se ejecuta antes)
    int priority = 0;

    /// Enabled flag (disabled systems are skipped during updateAll)
    bool enabled = true;
};

/// SystemManager — Registra y ejecuta sistemas en orden de prioridad.
///
/// Features:
///   - Priority ordering (lower = runs first)
///   - Enable/disable systems at runtime
///   - Get/remove systems by type (no RTTI, uses static type IDs)
///
class SystemManager {
public:
    /// Registra un sistema con su signature
    template<typename T, typename... Args>
    T* registerSystem(Args&&... args) {
        auto system = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = system.get();
        SystemEntry entry;
        entry.system = std::move(system);
        entry.typeId = detail::getSystemTypeId<T>();
        m_systems.push_back(std::move(entry));
        sortByPriority();
        return ptr;
    }

    /// Ejecuta todos los sistemas habilitados en orden de prioridad
    void updateAll(float dt) {
        for (auto& entry : m_systems) {
            if (entry.system->enabled) {
                entry.system->update(dt);
            }
        }
    }

    /// Get a system by type (returns nullptr if not found)
    template<typename T>
    T* getSystem() {
        uint32_t id = detail::getSystemTypeId<T>();
        for (auto& entry : m_systems) {
            if (entry.typeId == id) {
                return static_cast<T*>(entry.system.get());
            }
        }
        return nullptr;
    }

    /// Check if a system of type T is registered
    template<typename T>
    bool hasSystem() const {
        uint32_t id = detail::getSystemTypeId<T>();
        for (const auto& entry : m_systems) {
            if (entry.typeId == id) return true;
        }
        return false;
    }

    /// Remove a system by type
    template<typename T>
    bool removeSystem() {
        uint32_t id = detail::getSystemTypeId<T>();
        auto it = std::remove_if(m_systems.begin(), m_systems.end(),
            [id](const SystemEntry& e) { return e.typeId == id; });
        if (it != m_systems.end()) {
            m_systems.erase(it, m_systems.end());
            return true;
        }
        return false;
    }

    /// Enable a system by type
    template<typename T>
    void enableSystem(bool on = true) {
        uint32_t id = detail::getSystemTypeId<T>();
        for (auto& entry : m_systems) {
            if (entry.typeId == id) {
                entry.system->enabled = on;
                return;
            }
        }
    }

    /// Disable a system by type
    template<typename T>
    void disableSystem() { enableSystem<T>(false); }

    /// Get the number of registered systems
    int getSystemCount() const { return static_cast<int>(m_systems.size()); }

    /// Get the number of enabled systems
    int getEnabledCount() const {
        int count = 0;
        for (const auto& e : m_systems) { if (e.system->enabled) count++; }
        return count;
    }

private:
    struct SystemEntry {
        std::unique_ptr<System> system;
        uint32_t typeId = 0;
    };

    void sortByPriority() {
        std::sort(m_systems.begin(), m_systems.end(),
            [](const SystemEntry& a, const SystemEntry& b) {
                return a.system->priority < b.system->priority;
            });
    }

    std::vector<SystemEntry> m_systems;
};

} // namespace ecs
} // namespace engine
