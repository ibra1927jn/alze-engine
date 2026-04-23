#pragma once

#include "SystemManager.h"
#include "ECSCoordinator.h"
#include "Components.h"
#include "InputManager.h"

namespace engine {
namespace ecs {

/// InputSystem — Thin wrapper que lee el InputManager.
///
/// La lógica de input del jugador está en PlayState::handleInput()
/// (movimiento, salto, coyote time, jump buffer, variable jump, etc.)
///
/// Este sistema solo expone el estado grounded para otros sistemas.
/// En el futuro puede manejar input de entidades NPC/AI.
///
class InputSystem : public System {
public:
    InputSystem(ECSCoordinator&, core::InputManager& input)
        : m_input(input) {
        priority = -1;  // Se ejecuta antes que la física
    }

    // ── Configuración (usada por PlayState) ────────────────────
    float moveSpeed     = 500.0f;
    float jumpImpulse   = -450.0f;
    float coyoteTime    = 0.08f;
    float jumpBuffer    = 0.1f;
    float airControl    = 0.6f;

    void update(float) override {
        // Input del jugador se maneja en PlayState::handleInput()
        // Este sistema está disponible para NPC/AI input en el futuro
    }

    /// Llamar cuando el jugador toca el suelo
    void setGrounded(bool grounded) { m_isGrounded = grounded; }
    bool isGrounded() const { return m_isGrounded; }

    core::InputManager& getInput() { return m_input; }

private:
    core::InputManager& m_input;
    bool  m_isGrounded = false;
};

} // namespace ecs
} // namespace engine
