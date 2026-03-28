#pragma once

#include <glad/gl.h>
#include "ShaderProgram.h"
#include "math/Matrix4x4.h"
#include "math/Vector2D.h"
#include <vector>
#include <cmath>

namespace engine {
namespace renderer {

/// ShapeRenderer2D — GPU-accelerated 2D shape drawing.
///
/// Draws circles, polygons (filled/outlined), lines, arcs, and rounded rects.
/// All geometry is batched into a single VBO and drawn with minimal draw calls.
///
/// Usage:
///   shapes.begin(vpMatrix);
///   shapes.circle(100, 100, 50, {1,0,0,1});
///   shapes.rect(200, 100, 80, 40, {0,1,0,1});
///   shapes.polygon(points, 6, {0,0,1,1});
///   shapes.end();
///
class ShapeRenderer2D {
public:
    static constexpr int MAX_VERTICES = 65536;
    static constexpr int CIRCLE_SEGMENTS = 32;

    struct Color { float r, g, b, a; };

    ShapeRenderer2D() = default;
    ~ShapeRenderer2D() { shutdown(); }

    bool init() {
        if (!m_shader.compile(SHAPE_VS, SHAPE_FS)) return false;

        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);
        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, MAX_VERTICES * sizeof(Vertex), nullptr, GL_DYNAMIC_DRAW);

        // Position (vec2)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        // Color (vec4)
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(2 * sizeof(float)));

        glBindVertexArray(0);
        m_initialized = true;
        return true;
    }

    void shutdown() {
        if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
        if (m_vbo) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
        m_initialized = false;
    }

    void begin(const math::Matrix4x4& viewProjection) {
        m_vertices.clear();
        m_batches.clear();
        m_viewProjection = viewProjection;
    }

    // ── Filled Shapes ──────────────────────────────────────────

    /// Draw filled circle
    void circleFill(float cx, float cy, float radius, Color col, int segments = CIRCLE_SEGMENTS) {
        int start = static_cast<int>(m_vertices.size());
        float angleStep = 6.28318530718f / segments;

        // Center vertex
        m_vertices.push_back({cx, cy, col.r, col.g, col.b, col.a});

        for (int i = 0; i <= segments; i++) {
            float a = i * angleStep;
            m_vertices.push_back({
                cx + std::cos(a) * radius,
                cy + std::sin(a) * radius,
                col.r, col.g, col.b, col.a
            });
        }

        m_batches.push_back({GL_TRIANGLE_FAN, start, segments + 2});
    }

    /// Draw filled rectangle
    void rectFill(float x, float y, float w, float h, Color col) {
        int start = static_cast<int>(m_vertices.size());
        float x1 = x - w * 0.5f, y1 = y - h * 0.5f;
        float x2 = x + w * 0.5f, y2 = y + h * 0.5f;

        m_vertices.push_back({x1, y1, col.r, col.g, col.b, col.a});
        m_vertices.push_back({x2, y1, col.r, col.g, col.b, col.a});
        m_vertices.push_back({x2, y2, col.r, col.g, col.b, col.a});
        m_vertices.push_back({x1, y1, col.r, col.g, col.b, col.a});
        m_vertices.push_back({x2, y2, col.r, col.g, col.b, col.a});
        m_vertices.push_back({x1, y2, col.r, col.g, col.b, col.a});

        m_batches.push_back({GL_TRIANGLES, start, 6});
    }

    /// Draw filled rounded rectangle
    void roundedRectFill(float x, float y, float w, float h, float cornerRadius, Color col, int segments = 8) {
        // Body (3 rects)
        float cr = std::min(cornerRadius, std::min(w, h) * 0.5f);
        rectFill(x, y, w - 2 * cr, h, col);                     // Center
        rectFill(x - (w * 0.5f - cr), y, cr * 2, h - 2 * cr, col); // Left
        rectFill(x + (w * 0.5f - cr), y, cr * 2, h - 2 * cr, col); // Right

        // 4 corner arcs
        arcFill(x - w*0.5f + cr, y - h*0.5f + cr, cr, 3.14159f, 4.71239f, col, segments);  // TL
        arcFill(x + w*0.5f - cr, y - h*0.5f + cr, cr, 4.71239f, 6.28318f, col, segments);  // TR
        arcFill(x + w*0.5f - cr, y + h*0.5f - cr, cr, 0.0f, 1.5708f, col, segments);       // BR
        arcFill(x - w*0.5f + cr, y + h*0.5f - cr, cr, 1.5708f, 3.14159f, col, segments);   // BL
    }

    /// Draw filled arc (pie slice)
    void arcFill(float cx, float cy, float radius, float startAngle, float endAngle, Color col, int segments = 16) {
        int start = static_cast<int>(m_vertices.size());
        float angleStep = (endAngle - startAngle) / segments;

        m_vertices.push_back({cx, cy, col.r, col.g, col.b, col.a});
        for (int i = 0; i <= segments; i++) {
            float a = startAngle + i * angleStep;
            m_vertices.push_back({
                cx + std::cos(a) * radius,
                cy + std::sin(a) * radius,
                col.r, col.g, col.b, col.a
            });
        }
        m_batches.push_back({GL_TRIANGLE_FAN, start, segments + 2});
    }

    /// Draw filled convex polygon
    void polygonFill(const math::Vector2D* points, int count, Color col) {
        if (count < 3) return;
        int start = static_cast<int>(m_vertices.size());

        // Triangle fan from first vertex
        for (int i = 0; i < count; i++) {
            m_vertices.push_back({points[i].x, points[i].y, col.r, col.g, col.b, col.a});
        }
        m_batches.push_back({GL_TRIANGLE_FAN, start, count});
    }

    // ── Outlined Shapes ────────────────────────────────────────

    /// Draw circle outline
    void circleOutline(float cx, float cy, float radius, Color col, int segments = CIRCLE_SEGMENTS) {
        int start = static_cast<int>(m_vertices.size());
        float angleStep = 6.28318530718f / segments;

        for (int i = 0; i <= segments; i++) {
            float a = i * angleStep;
            m_vertices.push_back({
                cx + std::cos(a) * radius,
                cy + std::sin(a) * radius,
                col.r, col.g, col.b, col.a
            });
        }
        m_batches.push_back({GL_LINE_STRIP, start, segments + 1});
    }

    /// Draw rectangle outline
    void rectOutline(float x, float y, float w, float h, Color col) {
        int start = static_cast<int>(m_vertices.size());
        float x1 = x - w * 0.5f, y1 = y - h * 0.5f;
        float x2 = x + w * 0.5f, y2 = y + h * 0.5f;

        m_vertices.push_back({x1, y1, col.r, col.g, col.b, col.a});
        m_vertices.push_back({x2, y1, col.r, col.g, col.b, col.a});
        m_vertices.push_back({x2, y2, col.r, col.g, col.b, col.a});
        m_vertices.push_back({x1, y2, col.r, col.g, col.b, col.a});
        m_vertices.push_back({x1, y1, col.r, col.g, col.b, col.a}); // Close

        m_batches.push_back({GL_LINE_STRIP, start, 5});
    }

    /// Draw a line between two points
    void line(float x1, float y1, float x2, float y2, Color col) {
        int start = static_cast<int>(m_vertices.size());
        m_vertices.push_back({x1, y1, col.r, col.g, col.b, col.a});
        m_vertices.push_back({x2, y2, col.r, col.g, col.b, col.a});
        m_batches.push_back({GL_LINES, start, 2});
    }

    /// Draw polyline (connected line segments)
    void polyline(const math::Vector2D* points, int count, Color col, bool closed = false) {
        if (count < 2) return;
        int start = static_cast<int>(m_vertices.size());
        for (int i = 0; i < count; i++) {
            m_vertices.push_back({points[i].x, points[i].y, col.r, col.g, col.b, col.a});
        }
        if (closed) {
            m_vertices.push_back({points[0].x, points[0].y, col.r, col.g, col.b, col.a});
            m_batches.push_back({GL_LINE_STRIP, start, count + 1});
        } else {
            m_batches.push_back({GL_LINE_STRIP, start, count});
        }
    }

    // ── Flush ──────────────────────────────────────────────────

    void end() {
        if (m_vertices.empty() || !m_initialized) return;

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        m_shader.use();
        GLint vpLoc = glGetUniformLocation(m_shader.getHandle(), "uVP");
        glUniformMatrix4fv(vpLoc, 1, GL_FALSE, m_viewProjection.data());

        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0,
                        m_vertices.size() * sizeof(Vertex), m_vertices.data());

        for (const auto& batch : m_batches) {
            glDrawArrays(batch.mode, batch.first, batch.count);
        }

        glBindVertexArray(0);
        m_shader.unbind();
    }

    int getDrawCalls() const { return static_cast<int>(m_batches.size()); }

private:
    struct Vertex { float x, y, r, g, b, a; };
    struct Batch { GLenum mode; int first; int count; };

    bool m_initialized = false;
    GLuint m_vao = 0, m_vbo = 0;
    ShaderProgram m_shader;
    math::Matrix4x4 m_viewProjection;
    std::vector<Vertex> m_vertices;
    std::vector<Batch> m_batches;

    static constexpr const char* SHAPE_VS = R"(
        #version 330 core
        layout(location = 0) in vec2 aPos;
        layout(location = 1) in vec4 aColor;
        uniform mat4 uVP;
        out vec4 vColor;
        void main() {
            gl_Position = uVP * vec4(aPos, 0.0, 1.0);
            vColor = aColor;
        }
    )";

    static constexpr const char* SHAPE_FS = R"(
        #version 330 core
        in vec4 vColor;
        out vec4 FragColor;
        void main() { FragColor = vColor; }
    )";
};

} // namespace renderer
} // namespace engine
