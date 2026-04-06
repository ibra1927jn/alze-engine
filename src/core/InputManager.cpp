#include "InputManager.h"
#include "Logger.h"

namespace engine {
namespace core {

InputManager::~InputManager() {
    if (m_gamepad) {
        SDL_GameControllerClose(m_gamepad);
        m_gamepad = nullptr;
    }
}

void InputManager::prepare() {
    m_pressedKeys.clear();
    m_releasedKeys.clear();
    m_pressedButtons.clear();
    m_releasedButtons.clear();
    m_scrollDelta = 0;
    m_mouseDelta = math::Vector2D::Zero;
}

void InputManager::update(const SDL_Event& event) {
    switch (event.type) {
        case SDL_QUIT:
            m_quitRequested = true;
            break;

        // ── Teclado ────────────────────────────────────────────
        case SDL_KEYDOWN:
            if (!event.key.repeat) {
                m_pressedKeys[event.key.keysym.scancode] = true;
            }
            m_currentKeys[event.key.keysym.scancode] = true;
            break;

        case SDL_KEYUP:
            m_releasedKeys[event.key.keysym.scancode] = true;
            m_currentKeys[event.key.keysym.scancode] = false;
            break;

        // ── Ratón ──────────────────────────────────────────────
        case SDL_MOUSEMOTION:
            m_mousePos.x = static_cast<float>(event.motion.x);
            m_mousePos.y = static_cast<float>(event.motion.y);
            m_mouseDelta.x += static_cast<float>(event.motion.xrel);
            m_mouseDelta.y += static_cast<float>(event.motion.yrel);
            break;

        case SDL_MOUSEBUTTONDOWN:
            m_mouseButtons |= SDL_BUTTON(event.button.button);
            break;

        case SDL_MOUSEBUTTONUP:
            m_mouseButtons &= ~SDL_BUTTON(event.button.button);
            break;

        case SDL_MOUSEWHEEL:
            m_scrollDelta += event.wheel.y;
            break;

        // ── Gamepad ────────────────────────────────────────────
        case SDL_CONTROLLERDEVICEADDED:
            if (!m_gamepad) {
                tryOpenGamepad();
            }
            break;

        case SDL_CONTROLLERDEVICEREMOVED:
            if (m_gamepad) {
                SDL_GameControllerClose(m_gamepad);
                m_gamepad = nullptr;
                Logger::info("Input", "Gamepad desconectado");
            }
            break;

        case SDL_CONTROLLERBUTTONDOWN:
            m_pressedButtons[event.cbutton.button] = true;
            m_currentButtons[event.cbutton.button] = true;
            break;

        case SDL_CONTROLLERBUTTONUP:
            m_releasedButtons[event.cbutton.button] = true;
            m_currentButtons[event.cbutton.button] = false;
            break;

        default:
            break;
    }
}

// ── Teclado ────────────────────────────────────────────────────

bool InputManager::isKeyDown(SDL_Scancode key) const {
    auto it = m_currentKeys.find(key);
    return it != m_currentKeys.end() && it->second;
}

bool InputManager::isKeyPressed(SDL_Scancode key) const {
    auto it = m_pressedKeys.find(key);
    return it != m_pressedKeys.end() && it->second;
}

bool InputManager::isKeyReleased(SDL_Scancode key) const {
    auto it = m_releasedKeys.find(key);
    return it != m_releasedKeys.end() && it->second;
}

// ── Ratón ──────────────────────────────────────────────────────

math::Vector2D InputManager::getMousePosition() const {
    return m_mousePos;
}

bool InputManager::isMouseButtonDown(int button) const {
    return (m_mouseButtons & SDL_BUTTON(button)) != 0;
}

// ── Gamepad ────────────────────────────────────────────────────

bool InputManager::isGamepadConnected() const {
    return m_gamepad != nullptr;
}

bool InputManager::isButtonDown(SDL_GameControllerButton button) const {
    auto it = m_currentButtons.find(static_cast<int>(button));
    return it != m_currentButtons.end() && it->second;
}

bool InputManager::isButtonPressed(SDL_GameControllerButton button) const {
    auto it = m_pressedButtons.find(static_cast<int>(button));
    return it != m_pressedButtons.end() && it->second;
}

bool InputManager::isButtonReleased(SDL_GameControllerButton button) const {
    auto it = m_releasedButtons.find(static_cast<int>(button));
    return it != m_releasedButtons.end() && it->second;
}

math::Vector2D InputManager::getLeftStick() const {
    if (!m_gamepad) return math::Vector2D::Zero;

    float x = static_cast<float>(SDL_GameControllerGetAxis(m_gamepad, SDL_CONTROLLER_AXIS_LEFTX)) / 32767.0f;
    float y = static_cast<float>(SDL_GameControllerGetAxis(m_gamepad, SDL_CONTROLLER_AXIS_LEFTY)) / 32767.0f;
    return math::Vector2D(applyDeadzone(x), applyDeadzone(y));
}

math::Vector2D InputManager::getRightStick() const {
    if (!m_gamepad) return math::Vector2D::Zero;

    float x = static_cast<float>(SDL_GameControllerGetAxis(m_gamepad, SDL_CONTROLLER_AXIS_RIGHTX)) / 32767.0f;
    float y = static_cast<float>(SDL_GameControllerGetAxis(m_gamepad, SDL_CONTROLLER_AXIS_RIGHTY)) / 32767.0f;
    return math::Vector2D(applyDeadzone(x), applyDeadzone(y));
}

float InputManager::getLeftTrigger() const {
    if (!m_gamepad) return 0.0f;
    return static_cast<float>(SDL_GameControllerGetAxis(m_gamepad, SDL_CONTROLLER_AXIS_TRIGGERLEFT)) / 32767.0f;
}

float InputManager::getRightTrigger() const {
    if (!m_gamepad) return 0.0f;
    return static_cast<float>(SDL_GameControllerGetAxis(m_gamepad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT)) / 32767.0f;
}

// ── Mouse Capture ──────────────────────────────────────────────

void InputManager::setMouseCaptured(bool captured) {
    m_mouseCaptured = captured;
    SDL_SetRelativeMouseMode(captured ? SDL_TRUE : SDL_FALSE);
}

// ── Sistema ────────────────────────────────────────────────────

bool InputManager::isQuitRequested() const {
    return m_quitRequested;
}

// ── Helpers ────────────────────────────────────────────────────

float InputManager::applyDeadzone(float value) const {
    if (value > -m_deadzone && value < m_deadzone) return 0.0f;
    // Remap del rango [deadzone, 1] a [0, 1]
    float sign = value > 0.0f ? 1.0f : -1.0f;
    float absVal = value > 0.0f ? value : -value;
    float range = 1.0f - m_deadzone;
    if (range <= 0.0f) return sign;
    return sign * (absVal - m_deadzone) / range;
}

void InputManager::tryOpenGamepad() {
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            m_gamepad = SDL_GameControllerOpen(i);
            if (m_gamepad) {
                Logger::info("Input", std::string("Gamepad conectado: ") +
                    SDL_GameControllerName(m_gamepad));
                return;
            }
        }
    }
}

} // namespace core
} // namespace engine
