#pragma once

#include <glad/gl.h>
#include "../math/Matrix4x4.h"
#include <vector>

namespace engine {
namespace core {

/// DebugDraw3D — GPU-accelerated 3D debug drawing (lines, boxes, spheres, rays).
///
/// Usage:
///   DebugDraw3D::drawLine(a, b, color);
///   DebugDraw3D::drawAABB(min, max, green);
///   DebugDraw3D::drawSphere(center, radius, blue);
///   DebugDraw3D::drawRay(origin, dir, 10.0f, yellow);
///   // At end of frame:
///   DebugDraw3D::flush(viewProj);
///
class DebugDraw3D {
public:
    struct Color3 { float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f; };

    static constexpr Color3 RED     = {1.0f, 0.2f, 0.2f, 1.0f};
    static constexpr Color3 GREEN   = {0.2f, 1.0f, 0.2f, 1.0f};
    static constexpr Color3 BLUE    = {0.2f, 0.4f, 1.0f, 1.0f};
    static constexpr Color3 YELLOW  = {1.0f, 1.0f, 0.2f, 1.0f};
    static constexpr Color3 CYAN    = {0.2f, 1.0f, 1.0f, 1.0f};
    static constexpr Color3 MAGENTA = {1.0f, 0.2f, 1.0f, 1.0f};
    static constexpr Color3 WHITE   = {1.0f, 1.0f, 1.0f, 1.0f};
    static constexpr Color3 GRAY    = {0.5f, 0.5f, 0.5f, 0.7f};

    // ── Init / Shutdown ────────────────────────────────────────
    static void init() {
        // Compile minimal line shader
        const char* vs = R"(#version 330 core
            layout(location=0) in vec3 aPos;
            layout(location=1) in vec4 aColor;
            uniform mat4 uVP;
            out vec4 vColor;
            void main() {
                gl_Position = uVP * vec4(aPos, 1.0);
                vColor = aColor;
            })";

        const char* fs = R"(#version 330 core
            in vec4 vColor;
            out vec4 FragColor;
            void main() { FragColor = vColor; })";

        GLuint v = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(v, 1, &vs, nullptr);
        glCompileShader(v);

        GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(f, 1, &fs, nullptr);
        glCompileShader(f);

        s_shader = glCreateProgram();
        glAttachShader(s_shader, v);
        glAttachShader(s_shader, f);
        glLinkProgram(s_shader);
        glDeleteShader(v);
        glDeleteShader(f);

        s_vpLoc = glGetUniformLocation(s_shader, "uVP");

        glGenVertexArrays(1, &s_vao);
        glGenBuffers(1, &s_vbo);

        glBindVertexArray(s_vao);
        glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
        // pos (3f) + color (4f) = 7 floats per vertex
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);

        s_initialized = true;
    }

    static void shutdown() {
        if (!s_initialized) return;
        glDeleteProgram(s_shader);
        glDeleteVertexArrays(1, &s_vao);
        glDeleteBuffers(1, &s_vbo);
        s_initialized = false;
    }

    // ── Drawing primitives ─────────────────────────────────────

    static void setEnabled(bool e) { s_enabled = e; }
    static bool isEnabled() { return s_enabled; }
    static void toggle() { s_enabled = !s_enabled; }

    static void drawLine(const math::Vector3D& a, const math::Vector3D& b, Color3 c) {
        if (!s_enabled) return;
        pushVertex(a, c);
        pushVertex(b, c);
    }

    static void drawRay(const math::Vector3D& origin, const math::Vector3D& dir,
                         float length, Color3 c) {
        drawLine(origin, origin + dir.normalized() * length, c);
    }

    static void drawAABB(const math::Vector3D& mn, const math::Vector3D& mx, Color3 c) {
        if (!s_enabled) return;
        // 12 edges of a box
        math::Vector3D v[8] = {
            {mn.x, mn.y, mn.z}, {mx.x, mn.y, mn.z},
            {mx.x, mn.y, mx.z}, {mn.x, mn.y, mx.z},
            {mn.x, mx.y, mn.z}, {mx.x, mx.y, mn.z},
            {mx.x, mx.y, mx.z}, {mn.x, mx.y, mx.z}
        };
        // Bottom
        drawLine(v[0], v[1], c); drawLine(v[1], v[2], c);
        drawLine(v[2], v[3], c); drawLine(v[3], v[0], c);
        // Top
        drawLine(v[4], v[5], c); drawLine(v[5], v[6], c);
        drawLine(v[6], v[7], c); drawLine(v[7], v[4], c);
        // Verticals
        drawLine(v[0], v[4], c); drawLine(v[1], v[5], c);
        drawLine(v[2], v[6], c); drawLine(v[3], v[7], c);
    }

    static void drawBox(const math::Vector3D& center, const math::Vector3D& halfExt, Color3 c) {
        drawAABB(center - halfExt, center + halfExt, c);
    }

    static void drawSphere(const math::Vector3D& center, float radius, Color3 c, int segments = 16) {
        if (!s_enabled) return;
        float step = 2.0f * 3.14159265f / segments;
        // XY ring
        for (int i = 0; i < segments; i++) {
            float a1 = step * i, a2 = step * (i + 1);
            drawLine(center + math::Vector3D(cosf(a1), sinf(a1), 0) * radius,
                     center + math::Vector3D(cosf(a2), sinf(a2), 0) * radius, c);
        }
        // XZ ring
        for (int i = 0; i < segments; i++) {
            float a1 = step * i, a2 = step * (i + 1);
            drawLine(center + math::Vector3D(cosf(a1), 0, sinf(a1)) * radius,
                     center + math::Vector3D(cosf(a2), 0, sinf(a2)) * radius, c);
        }
        // YZ ring
        for (int i = 0; i < segments; i++) {
            float a1 = step * i, a2 = step * (i + 1);
            drawLine(center + math::Vector3D(0, cosf(a1), sinf(a1)) * radius,
                     center + math::Vector3D(0, cosf(a2), sinf(a2)) * radius, c);
        }
    }

    static void drawFrustum(const math::Matrix4x4& invVP, Color3 c) {
        if (!s_enabled) return;
        // NDC corners
        math::Vector3D ndc[8] = {
            {-1,-1,-1}, { 1,-1,-1}, { 1, 1,-1}, {-1, 1,-1},  // Near
            {-1,-1, 1}, { 1,-1, 1}, { 1, 1, 1}, {-1, 1, 1}   // Far
        };
        math::Vector3D world[8];
        for (int i = 0; i < 8; i++) {
            world[i] = invVP.transformPoint(ndc[i]);
        }
        // Near
        drawLine(world[0], world[1], c); drawLine(world[1], world[2], c);
        drawLine(world[2], world[3], c); drawLine(world[3], world[0], c);
        // Far
        drawLine(world[4], world[5], c); drawLine(world[5], world[6], c);
        drawLine(world[6], world[7], c); drawLine(world[7], world[4], c);
        // Connecting
        drawLine(world[0], world[4], c); drawLine(world[1], world[5], c);
        drawLine(world[2], world[6], c); drawLine(world[3], world[7], c);
    }

    static void drawGrid(float size, float step, Color3 c) {
        if (!s_enabled) return;
        for (float x = -size; x <= size; x += step) {
            drawLine({x, 0, -size}, {x, 0, size}, c);
            drawLine({-size, 0, x}, {size, 0, x}, c);
        }
    }

    static void drawAxes(const math::Vector3D& origin, float length) {
        drawLine(origin, origin + math::Vector3D(length, 0, 0), RED);
        drawLine(origin, origin + math::Vector3D(0, length, 0), GREEN);
        drawLine(origin, origin + math::Vector3D(0, 0, length), BLUE);
    }

    // ── Flush (renders all batched lines) ──────────────────────

    static void flush(const math::Matrix4x4& viewProj) {
        if (!s_initialized || s_vertices.empty()) {
            s_vertices.clear();
            return;
        }

        glUseProgram(s_shader);
        glUniformMatrix4fv(s_vpLoc, 1, GL_FALSE, viewProj.data());

        glBindVertexArray(s_vao);
        glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     s_vertices.size() * sizeof(float),
                     s_vertices.data(), GL_STREAM_DRAW);

        glDisable(GL_DEPTH_TEST);  // Draw on top
        glLineWidth(1.5f);
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(s_vertices.size() / 7));
        glEnable(GL_DEPTH_TEST);

        glBindVertexArray(0);
        glUseProgram(0);

        s_vertices.clear();
    }

    /// Flush with depth test ON (for 3D debug lines that respect depth)
    static void flushWithDepth(const math::Matrix4x4& viewProj) {
        if (!s_initialized || s_vertices.empty()) {
            s_vertices.clear();
            return;
        }

        glUseProgram(s_shader);
        glUniformMatrix4fv(s_vpLoc, 1, GL_FALSE, viewProj.data());

        glBindVertexArray(s_vao);
        glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     s_vertices.size() * sizeof(float),
                     s_vertices.data(), GL_STREAM_DRAW);

        glLineWidth(1.5f);
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(s_vertices.size() / 7));

        glBindVertexArray(0);
        glUseProgram(0);

        s_vertices.clear();
    }

    static int getLineCount() { return static_cast<int>(s_vertices.size() / 14); }

private:
    static void pushVertex(const math::Vector3D& p, Color3 c) {
        s_vertices.push_back(p.x);
        s_vertices.push_back(p.y);
        s_vertices.push_back(p.z);
        s_vertices.push_back(c.r);
        s_vertices.push_back(c.g);
        s_vertices.push_back(c.b);
        s_vertices.push_back(c.a);
    }

    inline static bool s_enabled = true;
    inline static bool s_initialized = false;
    inline static GLuint s_shader = 0, s_vao = 0, s_vbo = 0;
    inline static GLint s_vpLoc = -1;
    inline static std::vector<float> s_vertices;
};

} // namespace core
} // namespace engine
