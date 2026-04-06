#pragma once

#include "math/Vector2D.h"
#include "math/Color.h"
#include <vector>
#include <algorithm>

namespace engine {
namespace core {

/// RenderCommand — Instrucción de dibujo desacoplada del backend.
///
/// El RenderSystem emite estos comandos en vez de llamar SDL directamente.
/// El RenderBackend los ordena y ejecuta. Al pasar a OpenGL, solo
/// cambia el backend — el ECS sigue emitiendo los mismos comandos.
///
struct RenderCommand {
    enum class Type : uint8_t {
        RECT_FILL,
        RECT_OUTLINE,
        LINE
    };

    Type type = Type::RECT_FILL;
    math::Vector2D pos;     // Centro (para rects) o punto A (para líneas)
    math::Vector2D posB;    // Punto B (solo para líneas)
    float width = 0, height = 0;
    math::Color color;
    int16_t zOrder = 0;     // Para ordenar: menor = dibuja primero (fondo)
};

/// RenderQueue — Cola de comandos de dibujo, ordenada por z-order.
///
/// Uso:
///   queue.clear();
///   queue.push({ ... });
///   queue.sort();
///   for (auto& cmd : queue) backend.execute(cmd);
///
class RenderQueue {
public:
    void clear() { m_commands.clear(); }

    void push(const RenderCommand& cmd) {
        m_commands.push_back(cmd);
    }

    /// Rect fill centered at pos
    void pushRect(math::Vector2D pos, float w, float h, math::Color c, int16_t z = 0) {
        m_commands.push_back({RenderCommand::Type::RECT_FILL, pos, {}, w, h, c, z});
    }

    /// Rect outline centered at pos
    void pushRectOutline(math::Vector2D pos, float w, float h, math::Color c, int16_t z = 0) {
        m_commands.push_back({RenderCommand::Type::RECT_OUTLINE, pos, {}, w, h, c, z});
    }

    /// Line from A to B
    void pushLine(math::Vector2D a, math::Vector2D b, math::Color c, int16_t z = 0) {
        m_commands.push_back({RenderCommand::Type::LINE, a, b, 0, 0, c, z});
    }

    /// Ordenar por z-order, luego por tipo (minimiza cambios de estado)
    void sort() {
        std::sort(m_commands.begin(), m_commands.end(),
            [](const RenderCommand& a, const RenderCommand& b) {
                if (a.zOrder != b.zOrder) return a.zOrder < b.zOrder;
                return static_cast<uint8_t>(a.type) < static_cast<uint8_t>(b.type);
            });
    }

    size_t size() const { return m_commands.size(); }
    const std::vector<RenderCommand>& commands() const { return m_commands; }

    // Iteradores para for-range
    auto begin() const { return m_commands.begin(); }
    auto end()   const { return m_commands.end(); }

private:
    std::vector<RenderCommand> m_commands;
};

} // namespace core
} // namespace engine
