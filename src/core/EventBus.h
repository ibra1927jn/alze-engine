#pragma once

#include <atomic>
#include <functional>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>
#include <string>

namespace engine {
namespace core {

/// EventBus — Sistema de mensajería desacoplado (Pub/Sub).
///
/// Los sistemas se suscriben a tipos de eventos y reaccionan
/// cuando otro sistema los emite. Zero acoplamiento.
///
/// Uso:
///   EventBus bus;
///   bus.subscribe<CollisionEvent>([](const CollisionEvent& e) {
///       // Reproducir sonido, generar partículas, etc.
///   });
///   bus.emit(CollisionEvent{entityA, entityB, normal, impulse});
///
/// Internals:
///   - Cada tipo de evento tiene un ID único (compile-time counter)
///   - Los listeners se almacenan en vectores por tipo
///   - emit() llama a todos los listeners del tipo registrado
///   - Handlers almacenados con unique_ptr (RAII automático)
///

// IDs de tipo compiletime (sin RTTI)
using EventTypeId = uint32_t;

inline EventTypeId nextEventTypeId() {
    static std::atomic<EventTypeId> counter{0};
    return counter.fetch_add(1, std::memory_order_relaxed);
}

template<typename T>
EventTypeId getEventTypeId() {
    static EventTypeId id = nextEventTypeId();
    return id;
}

// ── Eventos predefinidos ───────────────────────────────────────

/// Se emite cuando dos entidades colisionan
struct CollisionEvent {
    uint32_t entityA = 0;
    uint32_t entityB = 0;
    float    impulse  = 0.0f;    // Fuerza del impacto
    float    normalX  = 0.0f;
    float    normalY  = 0.0f;
};

/// Se emite cuando una entidad entra en un trigger/sensor
struct TriggerEvent {
    uint32_t entityA = 0;    // Una de las entidades
    uint32_t entityB = 0;    // La otra entidad
};

/// Se emite cuando una entidad es destruida
struct EntityDestroyedEvent {
    uint32_t entity = 0;
};

/// Se emite cuando cambia el estado del jugador
struct PlayerStateEvent {
    bool onGround = false;
    bool jumped   = false;
    float speed   = 0.0f;
};

/// Evento genérico con string (para debugging)
struct DebugEvent {
    std::string message;
    float value = 0.0f;
};

// ── EventBus ───────────────────────────────────────────────────

/// ID opaco para desuscribirse de un evento
using SubscriptionId = uint32_t;

class IEventHandler {
public:
    virtual ~IEventHandler() = default;
    /// Remover un listener por su ID de suscripcion
    virtual bool remove(SubscriptionId id) = 0;
};

template<typename T>
class EventHandler : public IEventHandler {
public:
    using Callback = std::function<void(const T&)>;

    /// Agregar callback y devolver ID para desuscripcion
    SubscriptionId add(Callback cb) {
        SubscriptionId id = m_nextId++;
        m_callbacks.push_back({id, std::move(cb)});
        return id;
    }

    void invoke(const T& event) {
        for (auto& entry : m_callbacks) {
            entry.callback(event);
        }
    }

    /// Remover listener por ID — retorna true si se encontro
    bool remove(SubscriptionId id) override {
        for (auto it = m_callbacks.begin(); it != m_callbacks.end(); ++it) {
            if (it->id == id) {
                m_callbacks.erase(it);
                return true;
            }
        }
        return false;
    }

private:
    struct Entry {
        SubscriptionId id;
        Callback callback;
    };
    std::vector<Entry> m_callbacks;
    SubscriptionId m_nextId = 0;
};

class EventBus {
public:
    /// Suscribirse a un tipo de evento — retorna ID para desuscripcion
    template<typename T>
    SubscriptionId subscribe(std::function<void(const T&)> callback) {
        auto typeId = getEventTypeId<T>();
        auto it = m_handlers.find(typeId);
        if (it == m_handlers.end()) {
            m_handlers[typeId] = std::make_unique<EventHandler<T>>();
        }
        SubscriptionId subId = static_cast<EventHandler<T>*>(m_handlers[typeId].get())->add(std::move(callback));
        // Codificar typeId + subId local para busqueda rapida al desuscribir
        SubscriptionId globalId = m_nextGlobalId++;
        m_subscriptionMap[globalId] = typeId;
        m_localIds[globalId] = subId;
        return globalId;
    }

    /// Desuscribirse por ID — evita dangling pointers al destruir states
    bool unsubscribe(SubscriptionId globalId) {
        auto mapIt = m_subscriptionMap.find(globalId);
        if (mapIt == m_subscriptionMap.end()) return false;

        EventTypeId typeId = mapIt->second;
        SubscriptionId localId = m_localIds[globalId];

        auto handlerIt = m_handlers.find(typeId);
        if (handlerIt != m_handlers.end()) {
            handlerIt->second->remove(localId);
        }

        m_subscriptionMap.erase(mapIt);
        m_localIds.erase(globalId);
        return true;
    }

    /// Remover todos los listeners de un tipo de evento
    template<typename T>
    void unsubscribeAll() {
        auto typeId = getEventTypeId<T>();
        m_handlers.erase(typeId);
        // Limpiar mapeos correspondientes a ese tipo
        for (auto it = m_subscriptionMap.begin(); it != m_subscriptionMap.end();) {
            if (it->second == typeId) {
                m_localIds.erase(it->first);
                it = m_subscriptionMap.erase(it);
            } else {
                ++it;
            }
        }
    }

    /// Emitir un evento (notifica a todos los suscriptores)
    template<typename T>
    void emit(const T& event) {
        auto typeId = getEventTypeId<T>();
        auto it = m_handlers.find(typeId);
        if (it != m_handlers.end()) {
            static_cast<EventHandler<T>*>(it->second.get())->invoke(event);
        }
        m_totalEmitted++;
    }

    uint32_t getTotalEmitted() const { return m_totalEmitted; }

    // Destructor default — unique_ptr limpia automáticamente (RAII)
    ~EventBus() = default;

private:
    std::unordered_map<EventTypeId, std::unique_ptr<IEventHandler>> m_handlers;
    // Mapeo global ID → tipo de evento y ID local dentro del handler
    std::unordered_map<SubscriptionId, EventTypeId> m_subscriptionMap;
    std::unordered_map<SubscriptionId, SubscriptionId> m_localIds;
    SubscriptionId m_nextGlobalId = 0;
    uint32_t m_totalEmitted = 0;
};

} // namespace core
} // namespace engine
