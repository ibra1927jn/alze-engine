#include "Engine.h"
#include "Logger.h"
#include "Timer.h"
#include <sstream>
#include <cstdio>

namespace engine {
namespace core {

Engine::Engine(const std::string& title, int width, int height)
    : m_window(title, width, height)
{
    // Crear contexto gráfico vinculado a la ventana
    if (m_window.isOpen()) {
        if (!m_graphics.init(m_window.getSDLWindow())) {
            Logger::error("Engine", "GraphicsContext init failed: " + std::string(SDL_GetError()));
        } else {
            Logger::info("Engine", "GraphicsContext created (SDL2 Renderer)");
        }
    }

    // Frame allocator — 2MB para datos temporales por frame
    FrameAllocator::init(2 * 1024 * 1024);
    Logger::info("Engine", "FrameAllocator: 2MB initialized");
}

void Engine::setUpdateCallback(UpdateCallback callback) {
    m_updateCallback = std::move(callback);
}

void Engine::setRenderCallback(RenderCallback callback) {
    m_renderCallback = std::move(callback);
}

void Engine::setInputCallback(InputCallback callback) {
    m_inputCallback = std::move(callback);
}

Window& Engine::getWindow() {
    return m_window;
}

GraphicsContext& Engine::getGraphics() {
    return m_graphics;
}

InputManager& Engine::getInput() {
    return m_input;
}

float Engine::getFPS() const {
    return m_fps;
}

void Engine::run() {
    if (!m_window.isOpen()) {
        Logger::error("Engine", "No se puede iniciar: la ventana no esta abierta");
        return;
    }

    std::ostringstream oss;
    oss << "Game Loop iniciado (Fixed Timestep: " << FIXED_DT * 1000.0f << " ms)";
    Logger::info("Engine", oss.str());

    // ── Timer de alta precisión ────────────────────────────────
    Timer frameTimer;
    float accumulator = 0.0f;

    // Para medir FPS
    float fpsTimer   = 0.0f;
    int   frameCount = 0;

    // ── BUCLE PRINCIPAL ────────────────────────────────────────
    while (m_window.isOpen()) {

        // 0. Reset frame allocator
        FrameAllocator::reset();

        // 1. Delta time con Timer
        float deltaTime = frameTimer.lap();

        // Limitar deltaTime para evitar saltos grandes
        if (deltaTime > 0.25f) {
            Logger::warn("Engine", "deltaTime capped (posible lag o ventana movida)");
            deltaTime = 0.25f;
        }

        // 2. Procesar Input (eventos SDL)
        m_input.prepare();
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            m_input.update(event);
            m_window.handleResize(event);
        }

        // Si se pidió cerrar la ventana
        if (m_input.isQuitRequested()) {
            m_window.close();
            break;
        }

        // Callback de input personalizado
        if (m_inputCallback) {
            m_inputCallback(m_input, deltaTime);
        }

        // 3. Update con Fixed Timestep
        accumulator += deltaTime;
        int steps = 0;
        while (accumulator >= FIXED_DT && steps < MAX_STEPS_PER_FRAME) {
            if (m_updateCallback) {
                m_updateCallback(FIXED_DT);
            }
            accumulator -= FIXED_DT;
            steps++;
        }

        // 4. Calcular alpha para interpolación visual
        float alpha = accumulator / FIXED_DT;

        // 5. Render
        // In MODE_3D, PostProcess manages its own HDR FBO clear+present.
        // Engine only clears/presents in MODE_2D to avoid double-swap flicker.
        bool is3D = (m_graphics.getMode() == GraphicsContext::MODE_3D);
        if (!is3D) {
            m_graphics.clear(12, 12, 22);
        }
        if (m_renderCallback) {
            m_renderCallback(m_window, alpha);
        }
        if (!is3D) {
            m_graphics.present();
        }

        // 6. Medir FPS
        frameCount++;
        fpsTimer += deltaTime;
        if (fpsTimer >= 1.0f) {
            m_fps = static_cast<float>(frameCount) / fpsTimer;

            // Evitar ostringstream cada segundo — snprintf sin heap alloc
            char titleBuf[64];
            std::snprintf(titleBuf, sizeof(titleBuf), "ALZE Engine | FPS: %d", static_cast<int>(m_fps));
            m_window.setTitle(titleBuf);

            frameCount = 0;
            fpsTimer = 0.0f;
        }
    }

    // Cleanup
    m_graphics.shutdown();
    FrameAllocator::shutdown();
    Logger::info("Engine", "Game Loop terminado");
}

} // namespace core
} // namespace engine
