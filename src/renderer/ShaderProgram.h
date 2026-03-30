#pragma once

#include <glad/gl.h>
#include <string>
#include "core/Logger.h"
#include "math/Matrix4x4.h"

namespace engine {
namespace renderer {

/// ShaderProgram — Compila y gestiona shaders GLSL.
///
/// Uso:
///   ShaderProgram shader;
///   shader.compile(vertSrc, fragSrc);
///   shader.use();
///   shader.setMat4("uMVP", mvpMatrix);
///
class ShaderProgram {
public:
    ShaderProgram() = default;
    ~ShaderProgram() { destroy(); }

    // No copiable
    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;

    // Movible
    ShaderProgram(ShaderProgram&& other) noexcept : m_program(other.m_program) {
        other.m_program = 0;
    }
    ShaderProgram& operator=(ShaderProgram&& other) noexcept {
        if (this != &other) {
            destroy();
            m_program = other.m_program;
            other.m_program = 0;
        }
        return *this;
    }

    /// Compilar vertex + fragment shaders y linkar
    bool compile(const char* vertexSrc, const char* fragmentSrc) {
        GLuint vert = compileShader(GL_VERTEX_SHADER, vertexSrc);
        GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragmentSrc);
        if (!vert || !frag) {
            if (vert) glDeleteShader(vert);
            if (frag) glDeleteShader(frag);
            return false;
        }

        m_program = glCreateProgram();
        glAttachShader(m_program, vert);
        glAttachShader(m_program, frag);
        glLinkProgram(m_program);

        GLint success;
        glGetProgramiv(m_program, GL_LINK_STATUS, &success);
        if (!success) {
            char log[512];
            glGetProgramInfoLog(m_program, 512, nullptr, log);
            core::Logger::error("ShaderProgram", std::string("Link error: ") + log);
            glDeleteProgram(m_program);
            m_program = 0;
        }

        glDeleteShader(vert);
        glDeleteShader(frag);
        return m_program != 0;
    }

    void use() const { glUseProgram(m_program); }
    void unbind() const { glUseProgram(0); }

    GLuint getHandle() const { return m_program; }
    bool isValid() const { return m_program != 0; }

    // ── Uniforms ───────────────────────────────────────────────

    void setFloat(const char* name, float v) const {
        glUniform1f(loc(name), v);
    }

    void setVec2(const char* name, float x, float y) const {
        glUniform2f(loc(name), x, y);
    }

    void setVec3(const char* name, const math::Vector3D& v) const {
        glUniform3f(loc(name), v.x, v.y, v.z);
    }

    void setVec4(const char* name, float x, float y, float z, float w) const {
        glUniform4f(loc(name), x, y, z, w);
    }

    void setMat4(const char* name, const math::Matrix4x4& mat) const {
        glUniformMatrix4fv(loc(name), 1, GL_FALSE, mat.data());
    }

    void setInt(const char* name, int v) const {
        glUniform1i(loc(name), v);
    }

private:
    GLuint m_program = 0;

    GLint loc(const char* name) const {
        return glGetUniformLocation(m_program, name);
    }

    GLuint compileShader(GLenum type, const char* src) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);

        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char log[512];
            glGetShaderInfoLog(shader, 512, nullptr, log);
            const char* kind = (type == GL_VERTEX_SHADER) ? "Vertex" : "Fragment";
            core::Logger::error("ShaderProgram", std::string(kind) + " compile error: " + log);
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }

    void destroy() {
        if (m_program) {
            glDeleteProgram(m_program);
            m_program = 0;
        }
    }
};

// ── Shaders por defecto (embebidos) ────────────────────────────

/// Vertex shader: transforma posición por MVP, pasa color al fragment
inline const char* DEFAULT_VERTEX_SHADER = R"GLSL(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec4 aColor;

uniform mat4 uProjection;

out vec4 vColor;

void main() {
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
    vColor = aColor;
}
)GLSL";

/// Fragment shader: toma el color interpolado
inline const char* DEFAULT_FRAGMENT_SHADER = R"GLSL(
#version 330 core
in vec4 vColor;
out vec4 FragColor;

void main() {
    FragColor = vColor;
}
)GLSL";

} // namespace renderer
} // namespace engine
