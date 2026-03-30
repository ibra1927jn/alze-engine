#pragma once

#include <vector>
#include <memory>

namespace engine {
namespace core {

/// IGameState — Interfaz para estados del juego.
///
/// Cada estado controla qué sistemas se actualizan y cómo se dibuja.
/// Ejemplos: MenuState, PlayState, PauseState.
///
class IGameState {
public:
    virtual ~IGameState() = default;

    /// Nombre del estado (para debug)
    virtual const char* getName() const = 0;

    /// Se llama al entrar en el estado
    virtual void onEnter() {}

    /// Se llama al salir del estado
    virtual void onExit() {}

    /// Se llama al pausar (otro estado se apila encima)
    virtual void onPause() {}

    /// Se llama al reanudar (el estado de encima se retira)
    virtual void onResume() {}

    /// Actualizar lógica (puede no llamarse si estado pausado)
    virtual void update(float dt) = 0;

    /// Renderizar (siempre se llama para el estado activo)
    virtual void render(float alpha) = 0;

    /// Procesar input
    virtual void handleInput(float /*dt*/) {}

    /// ¿Debe actualizarse la física en este estado?
    virtual bool shouldUpdatePhysics() const { return true; }

    /// ¿Debe dibujarse el estado de debajo? (ej. pausa con fondo transparente)
    virtual bool isTransparent() const { return false; }
};

/// StateManager — Pila de estados con transiciones seguras.
///
/// Uso:
///   StateManager states;
///   states.push(std::make_unique<PlayState>());
///   // Game loop:
///   states.handleInput(dt);
///   states.update(dt);
///   states.render(alpha);
///   // Pausar:
///   states.push(std::make_unique<PauseState>());
///   // Despausar:
///   states.pop();
///
class StateManager {
public:
    /// Apilar un nuevo estado (el actual se pausa)
    void push(std::unique_ptr<IGameState> state) {
        if (!m_states.empty()) {
            m_states.back()->onPause();
        }
        state->onEnter();
        m_states.push_back(std::move(state));
    }

    /// Desapilar el estado actual (el de debajo se reanuda)
    void pop() {
        if (!m_states.empty()) {
            m_states.back()->onExit();
            m_states.pop_back();
            if (!m_states.empty()) {
                m_states.back()->onResume();
            }
        }
    }

    /// Reemplazar el estado actual por otro
    void change(std::unique_ptr<IGameState> state) {
        if (!m_states.empty()) {
            m_states.back()->onExit();
            m_states.pop_back();
        }
        state->onEnter();
        m_states.push_back(std::move(state));
    }

    /// Actualizar el estado activo (si shouldUpdatePhysics)
    void update(float dt) {
        if (!m_states.empty()) {
            m_states.back()->update(dt);
        }
    }

    /// Renderizar (soporta transparencia — dibuja estados de abajo si isTransparent)
    void render(float alpha) {
        if (m_states.empty()) return;
        // Encontrar el primer estado no-transparente desde arriba
        int firstVisible = static_cast<int>(m_states.size()) - 1;
        for (int i = firstVisible; i >= 1; i--) {
            if (!m_states[i]->isTransparent()) break;
            firstVisible = i - 1;
        }
        if (firstVisible < 0) firstVisible = 0;
        // Dibujar desde el primer visible hasta arriba
        for (int i = firstVisible; i < static_cast<int>(m_states.size()); i++) {
            m_states[i]->render(alpha);
        }
    }

    /// Procesar input del estado activo
    void handleInput(float dt) {
        if (!m_states.empty()) {
            m_states.back()->handleInput(dt);
        }
    }

    /// ¿Debe actualizarse la física?
    bool shouldUpdatePhysics() const {
        if (m_states.empty()) return false;
        return m_states.back()->shouldUpdatePhysics();
    }

    /// Estado actual
    IGameState* current() {
        return m_states.empty() ? nullptr : m_states.back().get();
    }

    const char* currentName() const {
        return m_states.empty() ? "None" : m_states.back()->getName();
    }

    int depth() const { return static_cast<int>(m_states.size()); }
    bool empty() const { return m_states.empty(); }

    /// Limpiar todo
    void clear() {
        while (!m_states.empty()) {
            m_states.back()->onExit();
            m_states.pop_back();
        }
    }

private:
    std::vector<std::unique_ptr<IGameState>> m_states;
};

} // namespace core
} // namespace engine
