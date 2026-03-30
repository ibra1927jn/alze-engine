#include "Window.h"
#include "Logger.h"

namespace engine {
namespace core {

Window::Window(const std::string& title, int width, int height)
    : m_width(width), m_height(height)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        Logger::error("Window", std::string("SDL_Init fallo: ") + SDL_GetError());
        return;
    }

    // ── Configurar OpenGL 3.3 Core Profile ───────────────────
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

    m_window = SDL_CreateWindow(
        title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width,
        height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL
    );

    if (!m_window) {
        Logger::error("Window", std::string("SDL_CreateWindow fallo: ") + SDL_GetError());
        return;
    }

    m_isOpen = true;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "Ventana creada: %dx%d", width, height);
    Logger::info("Window", buf);
}

Window::~Window() {
    if (m_window) {
        SDL_DestroyWindow(m_window);
    }
    SDL_Quit();
    Logger::info("Window", "Ventana destruida");
}

bool Window::isOpen() const {
    return m_isOpen;
}

void Window::close() {
    m_isOpen = false;
}

void Window::setTitle(const std::string& title) {
    SDL_SetWindowTitle(m_window, title.c_str());
}

void Window::handleResize(const SDL_Event& event) {
    if (event.type == SDL_WINDOWEVENT &&
        event.window.event == SDL_WINDOWEVENT_RESIZED) {
        m_width = event.window.data1;
        m_height = event.window.data2;

        char buf[64];
        std::snprintf(buf, sizeof(buf), "Ventana redimensionada: %dx%d", m_width, m_height);
        Logger::debug("Window", buf);

        if (m_resizeCallback) {
            m_resizeCallback(m_width, m_height);
        }
    }
}

int Window::getWidth() const {
    return m_width;
}

int Window::getHeight() const {
    return m_height;
}

SDL_Window* Window::getSDLWindow() const {
    return m_window;
}

} // namespace core
} // namespace engine
