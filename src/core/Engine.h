#pragma once

#include "Window.h"
#include "GraphicsContext.h"
#include "InputManager.h"
#include "FrameAllocator.h"
#include <string>
#include <functional>

namespace engine {
namespace core {

/// Motor principal — Ejecuta el Game Loop con Fixed Timestep.
///
/// El bucle principal desacopla la lógica (update) del renderizado (render):
/// - update() se ejecuta a un ritmo fijo (ej. 60 Hz) para físicas estables
/// - render() se ejecuta tan rápido como el hardware permita, interpolando
///
/// Posee:
///   - Window (OS window, events)
///   - GraphicsContext (SDL_Renderer / OpenGL)
///   - FrameAllocator (2MB, reset per frame)
///
class Engine {
public:
    Engine(const std::string& title, int width, int height);
    ~Engine() = default;

    // No copiable
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    /// Ejecutar el game loop (bloquea hasta que se cierra la ventana)
    void run();

    // ── Callbacks del juego ────────────────────────────────────

    using UpdateCallback = std::function<void(float dt)>;
    void setUpdateCallback(UpdateCallback callback);

    using RenderCallback = std::function<void(Window& window, float alpha)>;
    void setRenderCallback(RenderCallback callback);

    using InputCallback = std::function<void(const InputManager& input, float dt)>;
    void setInputCallback(InputCallback callback);

    // ── Getters ────────────────────────────────────────────────
    Window&          getWindow();
    GraphicsContext& getGraphics();
    InputManager&    getInput();
    float            getFPS() const;

    /// Paso de tiempo fijo en segundos (por defecto 1/60)
    static constexpr float FIXED_DT = 1.0f / 60.0f;
    static constexpr int   MAX_STEPS_PER_FRAME = 5;

private:
    Window          m_window;
    GraphicsContext m_graphics;
    InputManager    m_input;

    UpdateCallback m_updateCallback;
    RenderCallback m_renderCallback;
    InputCallback  m_inputCallback;

    float m_fps = 0.0f;
};

} // namespace core
} // namespace engine
