#pragma once

#include <glad/gl.h>
#include "ShaderProgram.h"
#include "Mesh.h"
#include "core/RenderCommand.h"
#include "math/Matrix4x4.h"
#include <vector>

namespace engine {
namespace renderer {

/// GLRenderBackend — Ejecuta RenderCommands usando OpenGL 3.3.
///
/// Reemplazo del RenderBackend (SDL2). Acepta la misma RenderQueue
/// y la dibuja usando shaders + batching.
///
/// Estrategia de batching:
///   Acumula todos los rects/lines en un solo buffer de vértices,
///   los sube a la GPU una vez, y los dibuja con un solo draw call.
///   Esto es mucho más eficiente que N draw calls individuales.
///
class GLRenderBackend {
public:
    GLRenderBackend() = default;

    /// Inicializar después de que GLAD esté cargado
    bool init(int viewportWidth, int viewportHeight) {
        m_viewW = viewportWidth;
        m_viewH = viewportHeight;

        // Compilar shader por defecto
        if (!m_shader.compile(DEFAULT_VERTEX_SHADER, DEFAULT_FRAGMENT_SHADER)) {
            return false;
        }

        // Crear mesh para batch
        m_batchMesh.init();

        // Configurar OpenGL state
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        updateProjection();
        return true;
    }

    /// Actualizar dimensiones del viewport (llamar en resize)
    void resize(int width, int height) {
        m_viewW = width;
        m_viewH = height;
        glViewport(0, 0, width, height);
        updateProjection();
    }

    /// Ejecutar toda la cola de render (procesa y dibuja en batch)
    void execute(const core::RenderQueue& queue) {
        m_vertices.clear();
        m_indices.clear();
        m_drawCalls = 0;

        uint32_t baseVertex = 0;

        for (const auto& cmd : queue) {
            float r = cmd.color.r / 255.0f;
            float g = cmd.color.g / 255.0f;
            float b = cmd.color.b / 255.0f;
            float a = cmd.color.a / 255.0f;

            switch (cmd.type) {
                case core::RenderCommand::Type::RECT_FILL: {
                    float x1 = cmd.pos.x - cmd.width * 0.5f;
                    float y1 = cmd.pos.y - cmd.height * 0.5f;
                    float x2 = x1 + cmd.width;
                    float y2 = y1 + cmd.height;

                    // 4 vértices: TL, TR, BR, BL
                    pushVertex(x1, y1, r, g, b, a);
                    pushVertex(x2, y1, r, g, b, a);
                    pushVertex(x2, y2, r, g, b, a);
                    pushVertex(x1, y2, r, g, b, a);

                    // 2 triángulos
                    m_indices.push_back(baseVertex + 0);
                    m_indices.push_back(baseVertex + 1);
                    m_indices.push_back(baseVertex + 2);
                    m_indices.push_back(baseVertex + 0);
                    m_indices.push_back(baseVertex + 2);
                    m_indices.push_back(baseVertex + 3);

                    baseVertex += 4;
                    break;
                }
                case core::RenderCommand::Type::RECT_OUTLINE: {
                    float x1 = cmd.pos.x - cmd.width * 0.5f;
                    float y1 = cmd.pos.y - cmd.height * 0.5f;
                    float x2 = x1 + cmd.width;
                    float y2 = y1 + cmd.height;
                    float t = 1.0f;  // grosor de línea en pixels

                    // Outline = 4 rects finos (top, bottom, left, right)
                    pushQuad(x1, y1, x2, y1 + t, r, g, b, a, baseVertex);      // Top
                    pushQuad(x1, y2 - t, x2, y2, r, g, b, a, baseVertex);       // Bottom
                    pushQuad(x1, y1 + t, x1 + t, y2 - t, r, g, b, a, baseVertex); // Left
                    pushQuad(x2 - t, y1 + t, x2, y2 - t, r, g, b, a, baseVertex); // Right
                    break;
                }
                case core::RenderCommand::Type::LINE: {
                    // Line = rect delgado entre los dos puntos
                    float dx = cmd.posB.x - cmd.pos.x;
                    float dy = cmd.posB.y - cmd.pos.y;
                    float len = std::sqrt(dx * dx + dy * dy);
                    if (len < 0.001f) break;

                    float nx = -dy / len;  // normal perpendicular
                    float ny =  dx / len;
                    float hw = 0.5f;  // half width

                    pushVertex(cmd.pos.x + nx * hw, cmd.pos.y + ny * hw, r, g, b, a);
                    pushVertex(cmd.pos.x - nx * hw, cmd.pos.y - ny * hw, r, g, b, a);
                    pushVertex(cmd.posB.x - nx * hw, cmd.posB.y - ny * hw, r, g, b, a);
                    pushVertex(cmd.posB.x + nx * hw, cmd.posB.y + ny * hw, r, g, b, a);

                    m_indices.push_back(baseVertex + 0);
                    m_indices.push_back(baseVertex + 1);
                    m_indices.push_back(baseVertex + 2);
                    m_indices.push_back(baseVertex + 0);
                    m_indices.push_back(baseVertex + 2);
                    m_indices.push_back(baseVertex + 3);

                    baseVertex += 4;
                    break;
                }
            }
        }

        // Subir y dibujar todo en un solo batch
        if (!m_indices.empty()) {
            m_batchMesh.upload(m_vertices.data(), m_vertices.size(),
                               m_indices.data(), m_indices.size());
            m_shader.use();
            m_shader.setMat4("uProjection", m_projection);
            m_batchMesh.draw();
            m_drawCalls = 1;  // ¡Todo en un solo draw call!
        }
    }

    int getDrawCalls() const { return m_drawCalls; }

private:
    void updateProjection() {
        // Ortho: (0,0) top-left, (viewW, viewH) bottom-right (matching SDL2 coords)
        m_projection = math::Matrix4x4::orthographic(
            0.0f, static_cast<float>(m_viewW),
            static_cast<float>(m_viewH), 0.0f,  // flip Y: top = 0, bottom = viewH
            -1.0f, 1.0f
        );
    }

    void pushVertex(float x, float y, float r, float g, float b, float a) {
        m_vertices.push_back(x);
        m_vertices.push_back(y);
        m_vertices.push_back(r);
        m_vertices.push_back(g);
        m_vertices.push_back(b);
        m_vertices.push_back(a);
    }

    void pushQuad(float x1, float y1, float x2, float y2,
                  float r, float g, float b, float a,
                  uint32_t& baseVertex) {
        pushVertex(x1, y1, r, g, b, a);
        pushVertex(x2, y1, r, g, b, a);
        pushVertex(x2, y2, r, g, b, a);
        pushVertex(x1, y2, r, g, b, a);

        m_indices.push_back(baseVertex + 0);
        m_indices.push_back(baseVertex + 1);
        m_indices.push_back(baseVertex + 2);
        m_indices.push_back(baseVertex + 0);
        m_indices.push_back(baseVertex + 2);
        m_indices.push_back(baseVertex + 3);

        baseVertex += 4;
    }

    ShaderProgram m_shader;
    Mesh          m_batchMesh;
    math::Matrix4x4 m_projection;

    int m_viewW = 800;
    int m_viewH = 600;
    int m_drawCalls = 0;

    std::vector<float>    m_vertices;  // Batch buffer
    std::vector<uint32_t> m_indices;
};

} // namespace renderer
} // namespace engine
