#pragma once

#include <SDL.h>
#include "Vector2D.h"
#include <unordered_map>

namespace engine {
namespace core {

/// Gestor de entradas — Lee teclado, ratón y gamepad vía SDL2.
/// Soporta detección de tecla mantenida, recién pulsada y recién soltada.
/// Soporta gamepad con sticks analógicos, triggers y botones.
class InputManager {
public:
    InputManager() = default;
    ~InputManager();

    /// Llamar al inicio de cada frame para actualizar el estado
    void update(const SDL_Event& event);

    /// Resetear los estados "just pressed" / "just released" antes de procesar eventos
    void prepare();

    // ── Teclado ────────────────────────────────────────────────

    bool isKeyDown(SDL_Scancode key) const;
    bool isKeyPressed(SDL_Scancode key) const;
    bool isKeyReleased(SDL_Scancode key) const;

    // ── Ratón ──────────────────────────────────────────────────

    math::Vector2D getMousePosition() const;
    math::Vector2D getMouseDelta() const { return m_mouseDelta; }
    bool isMouseButtonDown(int button) const;
    void setMouseCaptured(bool captured);
    bool isMouseCaptured() const { return m_mouseCaptured; }

    // ── Gamepad ────────────────────────────────────────────────

    /// ¿Hay un gamepad conectado?
    bool isGamepadConnected() const;

    /// Botones del gamepad (A, B, X, Y, etc.)
    bool isButtonDown(SDL_GameControllerButton button) const;
    bool isButtonPressed(SDL_GameControllerButton button) const;
    bool isButtonReleased(SDL_GameControllerButton button) const;

    /// Stick izquierdo (valores normalizados: -1 a 1)
    math::Vector2D getLeftStick() const;

    /// Stick derecho (valores normalizados: -1 a 1)
    math::Vector2D getRightStick() const;

    /// Trigger izquierdo (0 a 1)
    float getLeftTrigger() const;

    /// Trigger derecho (0 a 1)
    float getRightTrigger() const;

    /// Deadzone para sticks analógicos (por defecto 0.15)
    void setDeadzone(float deadzone) { m_deadzone = deadzone; }
    float getDeadzone() const { return m_deadzone; }

    // ── Scroll ──────────────────────────────────────────────────
    int getScrollDelta() const { return m_scrollDelta; }

    // ── Sistema ────────────────────────────────────────────────
    bool isQuitRequested() const;

private:
    // Teclado
    std::unordered_map<SDL_Scancode, bool> m_currentKeys;
    std::unordered_map<SDL_Scancode, bool> m_pressedKeys;
    std::unordered_map<SDL_Scancode, bool> m_releasedKeys;

    // Ratón
    math::Vector2D m_mousePos;
    math::Vector2D m_mouseDelta;
    uint32_t       m_mouseButtons = 0;
    bool           m_mouseCaptured = false;

    // Gamepad
    SDL_GameController* m_gamepad = nullptr;
    std::unordered_map<int, bool> m_currentButtons;
    std::unordered_map<int, bool> m_pressedButtons;
    std::unordered_map<int, bool> m_releasedButtons;
    float m_deadzone = 0.15f;
    int   m_scrollDelta = 0;  // Rueda del ratón Y delta

    bool m_quitRequested = false;

    // ── Helpers ────────────────────────────────────────────────
    float applyDeadzone(float value) const;
    void tryOpenGamepad();
};

} // namespace core
} // namespace engine
