#pragma once

#include <SDL.h>
#include "RenderCommand.h"

namespace engine {
namespace core {

/// RenderBackend — Ejecuta RenderCommands usando SDL2.
///
/// En el futuro, se creará un RenderBackend_OpenGL que reemplace
/// este archivo. El RenderSystem y ECS no cambian.
///
/// Uso:
///   RenderBackend backend(renderer);
///   backend.execute(queue);  // Dibuja todos los comandos
///
class RenderBackend {
public:
    explicit RenderBackend(SDL_Renderer* renderer) : m_renderer(renderer) {}

    /// Ejecutar toda la cola de render (ya debe estar sorted)
    void execute(const RenderQueue& queue) {
        m_drawCalls = 0;

        // Enable blending
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);

        for (const auto& cmd : queue) {
            setColor(cmd.color);

            switch (cmd.type) {
                case RenderCommand::Type::RECT_FILL: {
                    SDL_Rect rect = {
                        static_cast<int>(cmd.pos.x - cmd.width * 0.5f),
                        static_cast<int>(cmd.pos.y - cmd.height * 0.5f),
                        static_cast<int>(cmd.width),
                        static_cast<int>(cmd.height)
                    };
                    SDL_RenderFillRect(m_renderer, &rect);
                    m_drawCalls++;
                    break;
                }
                case RenderCommand::Type::RECT_OUTLINE: {
                    SDL_Rect rect = {
                        static_cast<int>(cmd.pos.x - cmd.width * 0.5f),
                        static_cast<int>(cmd.pos.y - cmd.height * 0.5f),
                        static_cast<int>(cmd.width),
                        static_cast<int>(cmd.height)
                    };
                    SDL_RenderDrawRect(m_renderer, &rect);
                    m_drawCalls++;
                    break;
                }
                case RenderCommand::Type::LINE: {
                    SDL_RenderDrawLine(m_renderer,
                        static_cast<int>(cmd.pos.x), static_cast<int>(cmd.pos.y),
                        static_cast<int>(cmd.posB.x), static_cast<int>(cmd.posB.y));
                    m_drawCalls++;
                    break;
                }

                default:
                    break;
            }
        }
    }

    int getDrawCalls() const { return m_drawCalls; }

private:
    void setColor(math::Color c) {
        SDL_SetRenderDrawColor(m_renderer, c.r, c.g, c.b, c.a);
    }

    SDL_Renderer* m_renderer;
    int m_drawCalls = 0;
};

} // namespace core
} // namespace engine
