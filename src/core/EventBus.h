#pragma once

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
    static EventTypeId counter = 0;
    return counter++;
}

template<typename T>
EventTypeId getEventTypeId() {
    static EventTypeId id = nextEventTypeId();
    return id;
}

// ── Eventos predefinidos ───────────────────────────────────────

/// Se emite cuando dos entidades colisionan
struct CollisionEvent {
    uint32_t entityA;
    uint32_t entityB;
    float    impulse;    // Fuerza del impacto
    float    normalX;
    float    normalY;
};

/// Se emite cuando una entidad entra en un trigger/sensor
struct TriggerEvent {
    uint32_t entityA;    // Una de las entidades
    uint32_t entityB;    // La otra entidad
};

/// Se emite cuando una entidad es destruida
struct EntityDestroyedEvent {
    uint32_t entity;
};

/// Se emite cuando cambia el estado del jugador
struct PlayerStateEvent {
    bool onGround;
    bool jumped;
    float speed;
};

/// Evento genérico con string (para debugging)
struct DebugEvent {
    std::string message;
    float value;
};

// ── EventBus ───────────────────────────────────────────────────

class IEventHandler {
public:
    virtual ~IEventHandler() = default;
};

template<typename T>
class EventHandler : public IEventHandler {
public:
    using Callback = std::function<void(const T&)>;

    void add(Callback cb) {
        m_callbacks.push_back(std::move(cb));
    }

    void invoke(const T& event) {
        for (auto& cb : m_callbacks) {
            cb(event);
        }
    }

private:
    std::vector<Callback> m_callbacks;
};

class EventBus {
public:
    /// Suscribirse a un tipo de evento
    template<typename T>
    void subscribe(std::function<void(const T&)> callback) {
        auto typeId = getEventTypeId<T>();
        auto it = m_handlers.find(typeId);
        if (it == m_handlers.end()) {
            m_handlers[typeId] = std::make_unique<EventHandler<T>>();
        }
        static_cast<EventHandler<T>*>(m_handlers[typeId].get())->add(std::move(callback));
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
    uint32_t m_totalEmitted = 0;
};

} // namespace core
} // namespace engine
