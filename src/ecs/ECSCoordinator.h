#pragma once

#include "EntityManager.h"
#include "ComponentStorage.h"
#include "SystemManager.h"
#include "QueryCache.h"
#include <array>
#include <atomic>
#include <memory>
#include <cassert>
#include <vector>
#include "core/JobSystem.h"

namespace engine {
namespace ecs {

// ── Compile-time type ID (no RTTI needed) ──────────────────────
// Cada tipo T obtiene un ID único usando un counter estático.
inline ComponentType nextTypeId() {
    static std::atomic<ComponentType> counter{0};
    return counter.fetch_add(1, std::memory_order_relaxed);
}

template<typename T>
ComponentType getTypeId() {
    static ComponentType id = nextTypeId();
    return id;
}

/// IComponentStorageBase — Interfaz no-templated para almacenar storages.
class IComponentStorageBase {
public:
    virtual ~IComponentStorageBase() = default;
    virtual void entityDestroyed(Entity entity) = 0;
    /// Copia el componente de src a dst (si src lo tiene)
    virtual void cloneComponent(Entity src, Entity dst) = 0;
};

/// IComponentStorageTyped — Wrapper con tipo.
template<typename T>
class IComponentStorageTyped : public IComponentStorageBase {
public:
    ComponentStorage<T> storage;

    void entityDestroyed(Entity entity) override {
        if (storage.has(entity)) {
            storage.remove(entity);
        }
    }

    void cloneComponent(Entity src, Entity dst) override {
        if (storage.has(src)) {
            storage.add(dst, storage.get(src));
        }
    }
};

/// ECSCoordinator — Fachada principal del ECS.
///
/// Punto de entrada único que coordina:
///   - EntityManager (entidades + generational IDs + masks)
///   - ComponentStorages (slotmaps por tipo)
///   - SystemManager (sistemas con prioridad)
///
/// Usa compile-time type IDs (sin RTTI/typeid).
///
class ECSCoordinator {
public:
    // ── Entidades ──────────────────────────────────────────────

    Entity createEntity() {
        m_queryCache.invalidate();
        return m_entityManager.createEntity();
    }

    void destroyEntity(Entity entity) {
        if (!m_entityManager.isAlive(entity)) return;
        m_queryCache.invalidate();

        // Notificar a todos los storages para que limpien
        for (auto& storage : m_storages) {
            if (storage) storage->entityDestroyed(entity);
        }
        m_entityManager.destroyEntity(entity);
    }

    bool isAlive(Entity entity) const {
        return m_entityManager.isAlive(entity);
    }

    uint32_t getActiveEntityCount() const {
        return m_entityManager.getActiveCount();
    }

    // ── Componentes ────────────────────────────────────────────

    /// Registra un tipo de componente (debe llamarse antes de usarlo)
    template<typename T>
    ComponentType registerComponent() {
        ComponentType type = getTypeId<T>();
        assert(type < MAX_COMPONENTS && "Too many component types!");
        assert(!m_storages[type] && "Componente ya registrado!");

        m_storages[type] = std::make_unique<IComponentStorageTyped<T>>();
        return type;
    }

    /// Añade un componente a una entidad
    template<typename T>
    void addComponent(Entity entity, const T& component) {
        getStorage<T>().add(entity, component);
        m_entityManager.setComponentBit(entity, getTypeId<T>(), true);
        m_queryCache.invalidate();
    }

    /// Añade un componente construido in-place
    template<typename T, typename... Args>
    T& emplaceComponent(Entity entity, Args&&... args) {
        T& ref = getStorage<T>().emplace(entity, std::forward<Args>(args)...);
        m_entityManager.setComponentBit(entity, getTypeId<T>(), true);
        return ref;
    }

    /// Elimina un componente de una entidad
    template<typename T>
    void removeComponent(Entity entity) {
        getStorage<T>().remove(entity);
        m_entityManager.setComponentBit(entity, getTypeId<T>(), false);
        m_queryCache.invalidate();
    }

    /// Obtiene referencia al componente
    template<typename T>
    T& getComponent(Entity entity) {
        return getStorage<T>().get(entity);
    }

    template<typename T>
    const T& getComponent(Entity entity) const {
        return getStorageConst<T>().get(entity);
    }

    /// ¿Tiene la entidad este componente?
    template<typename T>
    bool hasComponent(Entity entity) const {
        return getStorageConst<T>().has(entity);
    }

    template<typename T>
    ComponentStorage<T>& getStorage() {
        ComponentType type = getTypeId<T>();
        assert(m_storages[type] && "Component type not registered! Call registerComponent<T>() first.");
        return static_cast<IComponentStorageTyped<T>*>(m_storages[type].get())->storage;
    }

    template<typename T>
    const ComponentStorage<T>& getStorageConst() const {
        ComponentType type = getTypeId<T>();
        return static_cast<const IComponentStorageTyped<T>*>(m_storages[type].get())->storage;
    }

    // ── Masks ──────────────────────────────────────────────────

    ComponentMask getComponentMask(Entity entity) const {
        return m_entityManager.getComponentMask(entity);
    }

    bool matchesSignature(Entity entity, const ComponentMask& sig) const {
        return m_entityManager.matchesSignature(entity, sig);
    }

    /// Build a signature from component types
    template<typename... Ts>
    ComponentMask buildSignature() const {
        ComponentMask mask;
        (mask.set(getTypeId<Ts>()), ...);
        return mask;
    }

    // ── Tags ───────────────────────────────────────────────────

    void setTag(Entity entity, uint8_t tag, bool on = true) {
        m_entityManager.setTag(entity, tag, on);
    }

    bool hasTag(Entity entity, uint8_t tag) const {
        return m_entityManager.hasTag(entity, tag);
    }

    TagMask getTagMask(Entity entity) const {
        return m_entityManager.getTagMask(entity);
    }

    /// Find all alive entities with a specific tag
    std::vector<Entity> findByTag(uint8_t tag) const {
        std::vector<Entity> result;
        const auto& em = m_entityManager;
        const auto& aliveList = em.m_aliveList;
        for (uint32_t i = 0; i < aliveList.size(); i++) {
            uint32_t idx = aliveList[i];
            if (em.m_tagMasks[idx].test(tag)) {
                result.push_back(makeEntity(idx, em.m_generations[idx]));
            }
        }
        return result;
    }

    /// Iterate over all entities with a specific tag
    template<typename Func>
    void forEachWithTag(uint8_t tag, Func&& func) {
        auto& em = m_entityManager;
        const auto& aliveList = em.m_aliveList;
        for (uint32_t i = 0; i < aliveList.size(); i++) {
            uint32_t idx = aliveList[i];
            if (em.m_tagMasks[idx].test(tag)) {
                func(makeEntity(idx, em.m_generations[idx]));
            }
        }
    }

    /// Find the first entity with a specific tag (or NULL_ENTITY if not found)
    Entity findFirstByTag(uint8_t tag) const {
        const auto& em = m_entityManager;
        const auto& aliveList = em.m_aliveList;
        for (uint32_t i = 0; i < aliveList.size(); i++) {
            uint32_t idx = aliveList[i];
            if (em.m_tagMasks[idx].test(tag)) {
                return makeEntity(idx, em.m_generations[idx]);
            }
        }
        return NULL_ENTITY;
    }

    // ── Sistemas ───────────────────────────────────────────────

    template<typename T, typename... Args>
    T* registerSystem(Args&&... args) {
        return m_systemManager.registerSystem<T>(std::forward<Args>(args)...);
    }

    void updateSystems(float dt) {
        m_systemManager.updateAll(dt);
    }

    // ── Iteration ───────────────────────────────────────────────

    /// Iterate over all alive entities that have ALL specified component types.
    /// Callback signature: void(Entity, T1&, T2&, ...)
    /// Uses dense alive-list: O(alive) not O(MAX_ENTITIES)
    template<typename... Ts, typename Func>
    void forEach(Func&& func) {
        ComponentMask sig = buildSignature<Ts...>();
        auto& em = m_entityManager;
        const auto& aliveList = em.m_aliveList;

        for (uint32_t i = 0; i < aliveList.size(); i++) {
            uint32_t idx = aliveList[i];
            if ((em.m_componentMasks[idx] & sig) != sig) continue;
            Entity entity = makeEntity(idx, em.m_generations[idx]);
            func(entity, getStorage<Ts>().get(entity)...);
        }
    }

    /// Cached forEach — uses QueryCache for O(matched) iteration.
    /// Same as forEach but avoids re-scanning alive list on repeated calls.
    /// Cache auto-invalidates when entities/components change.
    template<typename... Ts, typename Func>
    void cachedForEach(Func&& func) {
        ComponentMask sig = buildSignature<Ts...>();
        const auto& entities = m_queryCache.getMatching(sig, m_entityManager);
        for (const auto& entity : entities) {
            func(entity, getStorage<Ts>().get(entity)...);
        }
    }

    /// Get QueryCache stats
    size_t getCachedQueryCount() const { return m_queryCache.getCachedQueryCount(); }

    /// Iterate over all alive entities matching a signature.
    /// Callback: void(Entity)
    /// Uses dense alive-list: O(alive) not O(MAX_ENTITIES)
    template<typename Func>
    void forEachEntity(const ComponentMask& sig, Func&& func) {
        auto& em = m_entityManager;
        const auto& aliveList = em.m_aliveList;

        for (uint32_t i = 0; i < aliveList.size(); i++) {
            uint32_t idx = aliveList[i];
            if ((em.m_componentMasks[idx] & sig) != sig) continue;
            func(makeEntity(idx, em.m_generations[idx]));
        }
    }

    /// Parallel forEach — usa JobSystem para reutilizar thread pool existente.
    /// func signature: void(Entity, Ts&...)
    template<typename... Ts, typename Func>
    void parallelForEach(Func&& func, int chunkSize = 64) {
        ComponentMask sig = buildSignature<Ts...>();
        auto& em = m_entityManager;
        const auto& aliveList = em.m_aliveList;

        // Recolectar entidades que cumplen la firma
        std::vector<Entity> matched;
        matched.reserve(aliveList.size());
        for (uint32_t i = 0; i < aliveList.size(); i++) {
            uint32_t idx = aliveList[i];
            if ((em.m_componentMasks[idx] & sig) != sig) continue;
            matched.push_back(makeEntity(idx, em.m_generations[idx]));
        }

        if (matched.empty()) return;

        int count = static_cast<int>(matched.size());

        // Usar JobSystem si esta disponible y corriendo
        if (m_jobSystem && m_jobSystem->isRunning()) {
            m_jobSystem->parallel_for(0, count, chunkSize,
                [&](int start, int end) {
                    for (int i = start; i < end; i++) {
                        func(matched[i], getStorage<Ts>().get(matched[i])...);
                    }
                });
        } else {
            // Fallback: ejecucion secuencial en thread principal
            for (int i = 0; i < count; i++) {
                func(matched[i], getStorage<Ts>().get(matched[i])...);
            }
        }
    }

    // ── Entity Cloning (Prefab instantiation) ─────────────────

    /// Clone an entity — creates a new entity with copies of all registered components.
    /// Note: only clones components that are added to the source entity.
    /// Returns the new entity ID.
    Entity cloneEntity(Entity source) {
        if (!isAlive(source)) return Entity{0};

        Entity clone = createEntity();
        ComponentMask srcMask = getComponentMask(source);

        // Copiar datos reales de componentes via storages
        for (size_t i = 0; i < MAX_COMPONENTS; i++) {
            if (srcMask.test(i) && m_storages[i]) {
                m_storages[i]->cloneComponent(source, clone);
            }
        }

        // Copiar mask y tags
        uint32_t cloneIdx = clone & 0xFFFFF;  // INDEX_MASK
        uint32_t srcIdx = source & 0xFFFFF;
        m_entityManager.m_componentMasks[cloneIdx] = srcMask;
        m_entityManager.m_tagMasks[cloneIdx] = m_entityManager.m_tagMasks[srcIdx];

        return clone;
    }

    // ── Batch Operations ──────────────────────────────────────

    /// Count entities matching a signature
    template<typename... Ts>
    uint32_t countEntities() const {
        ComponentMask sig;
        (sig.set(getTypeId<Ts>()), ...);
        uint32_t count = 0;
        const auto& aliveList = m_entityManager.m_aliveList;
        for (uint32_t i = 0; i < aliveList.size(); i++) {
            if ((m_entityManager.m_componentMasks[aliveList[i]] & sig) == sig) count++;
        }
        return count;
    }

    /// Destroy all entities matching a signature
    template<typename... Ts>
    void destroyMatching() {
        ComponentMask sig = buildSignature<Ts...>();
        auto& em = m_entityManager;
        std::vector<Entity> toDestroy;
        for (uint32_t i = 0; i < em.m_aliveList.size(); i++) {
            uint32_t idx = em.m_aliveList[i];
            if ((em.m_componentMasks[idx] & sig) == sig)
                toDestroy.push_back(makeEntity(idx, em.m_generations[idx]));
        }
        for (auto e : toDestroy) destroyEntity(e);
    }

    /// Destroy ALL entities
    void destroyAll() {
        auto& em = m_entityManager;
        std::vector<Entity> all;
        for (uint32_t i = 0; i < em.m_aliveList.size(); i++) {
            uint32_t idx = em.m_aliveList[i];
            all.push_back(makeEntity(idx, em.m_generations[idx]));
        }
        for (auto e : all) destroyEntity(e);
    }

    EntityManager& getEntityManager() { return m_entityManager; }

    /// Asignar JobSystem para reutilizar thread pool en parallelForEach
    void setJobSystem(core::JobSystem* js) { m_jobSystem = js; }

private:
    EntityManager  m_entityManager;
    SystemManager  m_systemManager;
    QueryCache     m_queryCache;
    core::JobSystem* m_jobSystem = nullptr;

    std::array<std::unique_ptr<IComponentStorageBase>, MAX_COMPONENTS> m_storages;
};

} // namespace ecs
} // namespace engine

