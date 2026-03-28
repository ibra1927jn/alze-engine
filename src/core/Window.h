#pragma once

#include <string>
#include <functional>
#include <SDL.h>

namespace engine {
namespace core {

/// Window — Gestión de la ventana del sistema operativo.
///
/// Solo gestiona: SDL_Window, eventos, resize, open/close.
/// El contexto gráfico (renderer/OpenGL) vive en GraphicsContext.
///
class Window {
public:
    Window(const std::string& title, int width, int height);
    ~Window();

    // No permitimos copias (SDL resources)
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    /// ¿Está la ventana abierta?
    bool isOpen() const;

    /// Cerrar la ventana
    void close();

    /// Actualizar el título
    void setTitle(const std::string& title);

    /// Procesar evento de resize (llamar desde el event loop)
    void handleResize(const SDL_Event& event);

    // ── Callback de resize ─────────────────────────────────────

    /// Se llama cuando la ventana cambia de tamaño
    using ResizeCallback = std::function<void(int newWidth, int newHeight)>;
    void setResizeCallback(ResizeCallback callback) { m_resizeCallback = std::move(callback); }

    // ── Getters ────────────────────────────────────────────────
    int getWidth() const;
    int getHeight() const;
    SDL_Window* getSDLWindow() const;

private:
    SDL_Window*   m_window   = nullptr;
    bool          m_isOpen   = false;
    int           m_width;
    int           m_height;
    ResizeCallback m_resizeCallback;
};

} // namespace core
} // namespace engine
