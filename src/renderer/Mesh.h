#pragma once

#include <glad/gl.h>

namespace engine {
namespace renderer {

/// Mesh — Wrapper de VAO + VBO + EBO para enviar geometría a la GPU.
///
/// Soporta geometry dinámica (recrear cada frame para batching)
/// y geometry estática (crear una vez, dibujar muchas veces).
///
/// Vertex layout: x, y, r, g, b, a (6 floats = 24 bytes por vértice)
///
class Mesh {
public:
    Mesh() = default;
    ~Mesh() { destroy(); }

    // No copiable
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    // Movible
    Mesh(Mesh&& other) noexcept
        : m_vao(other.m_vao), m_vbo(other.m_vbo), m_ebo(other.m_ebo),
          m_indexCount(other.m_indexCount) {
        other.m_vao = other.m_vbo = other.m_ebo = 0;
        other.m_indexCount = 0;
    }

    /// Inicializar buffers OpenGL (llamar una vez después de GLAD init)
    void init() {
        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);
        glGenBuffers(1, &m_ebo);

        glBindVertexArray(m_vao);

        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        // Position: layout(location = 0) = vec2
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        // Color: layout(location = 1) = vec4
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);

        glBindVertexArray(0);
    }

    /// Subir geometry dinámica (llamar cada frame para batch)
    /// @param vertices Array de floats: [x, y, r, g, b, a, ...]
    /// @param vertexCount Total de floats (numVerts * 6)
    /// @param indices Array de uint32_t
    /// @param indexCount Total de indices
    void upload(const float* vertices, size_t vertexFloatCount,
                const uint32_t* indices, size_t indexCount) {
        m_indexCount = static_cast<int>(indexCount);

        glBindVertexArray(m_vao);

        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(vertexFloatCount * sizeof(float)),
                     vertices, GL_DYNAMIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(indexCount * sizeof(uint32_t)),
                     indices, GL_DYNAMIC_DRAW);

        glBindVertexArray(0);
    }

    /// Dibujar la geometry subida
    void draw() const {
        if (m_indexCount == 0) return;
        glBindVertexArray(m_vao);
        glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    bool isValid() const { return m_vao != 0; }
    GLuint getVAO() const { return m_vao; }
    int getIndexCount() const { return m_indexCount; }

private:
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_ebo = 0;
    int    m_indexCount = 0;

    void destroy() {
        if (m_ebo) glDeleteBuffers(1, &m_ebo);
        if (m_vbo) glDeleteBuffers(1, &m_vbo);
        if (m_vao) glDeleteVertexArrays(1, &m_vao);
        m_vao = m_vbo = m_ebo = 0;
    }
};

} // namespace renderer
} // namespace engine
