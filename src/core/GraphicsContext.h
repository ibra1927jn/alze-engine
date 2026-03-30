#pragma once

#include <SDL.h>
#include <glad/gl.h>
#include <string>
#include "Logger.h"

namespace engine {
namespace core {

/// GraphicsContext — Dual-mode: OpenGL 3.3 Core + SDL_Renderer.
///
/// El motor soporta dos modos de rendering:
///   - MODE_2D: Usa SDL_Renderer para sprites, shapes, text (legacy 2D pipeline)
///   - MODE_3D: Usa OpenGL directamente para PBR, shadows, post-process
///
/// Ambos usan el mismo SDL_Window con SDL_WINDOW_OPENGL.
/// En modo 2D: SDL_Renderer se crea con SDL_RENDERER_SOFTWARE para no
///             interferir con nuestro GL context manual.
/// En modo 3D: OpenGL hace clear/present directamente via SDL_GL_SwapWindow.
///
class GraphicsContext {
public:
    enum Mode { MODE_2D, MODE_3D };

    bool init(SDL_Window* window) {
        m_window = window;

        // ── 1. Crear contexto OpenGL 3.3 Core (nuestro, manual) ──
        m_glContext = SDL_GL_CreateContext(window);
        if (!m_glContext) {
            Logger::error("GraphicsContext", std::string("SDL_GL_CreateContext fallo: ") + SDL_GetError());
            return false;
        }

        // Inicializar GLAD
        int version = gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress);
        if (!version) {
            Logger::error("GraphicsContext", "GLAD fallo al cargar OpenGL");
            SDL_GL_DeleteContext(m_glContext);
            m_glContext = nullptr;
            return false;
        }

        Logger::info("GraphicsContext", "OpenGL " + std::to_string(GLAD_VERSION_MAJOR(version)) + "."
                  + std::to_string(GLAD_VERSION_MINOR(version)) + " Core inicializado");

        SDL_GL_SetSwapInterval(1);  // VSync

        // ── 2. Crear SDL_Renderer SOFTWARE para 2D (no roba nuestro GL context) ──
        m_renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        if (!m_renderer) {
            Logger::warn("GraphicsContext", std::string("SDL_Renderer (software) fallo: ") + SDL_GetError());
            // No es fatal — 3D sigue funcionando
        }

        // Default: modo 2D (PlayState actual)
        m_mode = MODE_2D;
        return true;
    }

    void shutdown() {
        if (m_renderer) {
            SDL_DestroyRenderer(m_renderer);
            m_renderer = nullptr;
        }
        if (m_glContext) {
            SDL_GL_DeleteContext(m_glContext);
            m_glContext = nullptr;
        }
    }

    // ── Mode switching ────────────────────────────────────────

    void setMode(Mode mode) {
        m_mode = mode;
        if (mode == MODE_3D) {
            SDL_GL_MakeCurrent(m_window, m_glContext);
            glEnable(GL_DEPTH_TEST);
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            glEnable(GL_MULTISAMPLE);
            glViewport(0, 0, getWidth(), getHeight());
        }
    }

    Mode getMode() const { return m_mode; }

    // ── Clear / Present ───────────────────────────────────────

    void clear(uint8_t r = 12, uint8_t g = 12, uint8_t b = 22, uint8_t a = 255) {
        if (m_mode == MODE_3D) {
            SDL_GL_MakeCurrent(m_window, m_glContext);
            glClearColor(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        } else {
            if (m_renderer) {
                SDL_SetRenderDrawColor(m_renderer, r, g, b, a);
                SDL_RenderClear(m_renderer);
            }
        }
    }

    void present() {
        if (m_mode == MODE_3D) {
            SDL_GL_SwapWindow(m_window);
        } else {
            if (m_renderer) {
                SDL_RenderPresent(m_renderer);
            }
        }
    }

    // ── Accessors ─────────────────────────────────────────────

    SDL_GLContext getGLContext() const { return m_glContext; }
    SDL_Renderer* getRenderer() const { return m_renderer; }
    SDL_Window* getWindow() const { return m_window; }
    bool hasGL() const { return m_glContext != nullptr; }

    int getWidth() const {
        int w, h;
        SDL_GetWindowSize(m_window, &w, &h);
        return w;
    }

    int getHeight() const {
        int w, h;
        SDL_GetWindowSize(m_window, &w, &h);
        return h;
    }

private:
    SDL_Window*   m_window    = nullptr;
    SDL_Renderer* m_renderer  = nullptr;
    SDL_GLContext  m_glContext = nullptr;
    Mode          m_mode      = MODE_2D;
};

} // namespace core
} // namespace engine
