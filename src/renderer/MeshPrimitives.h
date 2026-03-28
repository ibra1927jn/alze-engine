#pragma once

#include <glad/gl.h>
#include <vector>
#include <cmath>
#include "math/MathUtils.h"
#include "math/Vector3D.h"

namespace engine {
namespace renderer {

/// Mesh3D — VAO + VBO + EBO para geometry 3D.
///
/// Vertex layout 3D: {posX, posY, posZ, normX, normY, normZ, u, v}
///                   = 8 floats × 32 bytes por vértice
///
class Mesh3D {
public:
    Mesh3D() = default;
    ~Mesh3D() { destroy(); }

    // No copiable
    Mesh3D(const Mesh3D&) = delete;
    Mesh3D& operator=(const Mesh3D&) = delete;

    // Movible
    Mesh3D(Mesh3D&& other) noexcept
        : m_vao(other.m_vao), m_vbo(other.m_vbo), m_ebo(other.m_ebo),
          m_indexCount(other.m_indexCount) {
        other.m_vao = other.m_vbo = other.m_ebo = 0;
        other.m_indexCount = 0;
    }

    Mesh3D& operator=(Mesh3D&& other) noexcept {
        if (this != &other) {
            destroy();
            m_vao = other.m_vao; m_vbo = other.m_vbo; m_ebo = other.m_ebo;
            m_instanceVBO = other.m_instanceVBO;
            m_indexCount = other.m_indexCount;
            other.m_vao = other.m_vbo = other.m_ebo = other.m_instanceVBO = 0;
            other.m_indexCount = 0;
        }
        return *this;
    }

    /// Crear desde arrays de datos
    void create(const float* vertices, size_t vertexFloatCount,
                const uint32_t* indices, size_t indexCount) {
        destroy();
        m_indexCount = static_cast<int>(indexCount);

        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);
        glGenBuffers(1, &m_ebo);

        glBindVertexArray(m_vao);

        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(vertexFloatCount * sizeof(float)),
                     vertices, GL_STATIC_DRAW);

        // Position: layout(location = 0) = vec3
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        // Normal: layout(location = 1) = vec3
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        // TexCoord: layout(location = 2) = vec2
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(indexCount * sizeof(uint32_t)),
                     indices, GL_STATIC_DRAW);

        glBindVertexArray(0);
    }

    void draw() const {
        if (m_indexCount == 0) return;
        glBindVertexArray(m_vao);
        glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, 0);
        // Note: no unbind — next draw rebinds anyway (perf optimization)
    }

    /// Draw N instances using the instance buffer
    void drawInstanced(int instanceCount) const {
        if (m_indexCount == 0 || instanceCount <= 0) return;
        glBindVertexArray(m_vao);
        glDrawElementsInstanced(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, 0, instanceCount);
    }

    /// Upload instance model matrices (mat4 per instance, layout 3-6)
    /// Call once to create the buffer, then updateInstanceData to update
    void setupInstanceBuffer(const float* matrices, int count) {
        glBindVertexArray(m_vao);

        if (!m_instanceVBO) {
            glGenBuffers(1, &m_instanceVBO);
        }

        glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(count * 16 * sizeof(float)),
                     matrices, GL_DYNAMIC_DRAW);

        // mat4 takes 4 attrib slots (vec4 each): locations 3, 4, 5, 6
        for (int i = 0; i < 4; i++) {
            GLuint loc = 3 + i;
            glEnableVertexAttribArray(loc);
            glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE,
                                  16 * sizeof(float),
                                  (void*)(i * 4 * sizeof(float)));
            glVertexAttribDivisor(loc, 1); // 1 = per instance
        }

        glBindVertexArray(0);
    }

    /// Update existing instance buffer data (faster than setup)
    void updateInstanceData(const float* matrices, int count) {
        if (!m_instanceVBO) { setupInstanceBuffer(matrices, count); return; }
        glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0,
                        static_cast<GLsizeiptr>(count * 16 * sizeof(float)),
                        matrices);
    }

    bool isValid() const { return m_vao != 0; }
    GLuint getVAO() const { return m_vao; }
    int getIndexCount() const { return m_indexCount; }

private:
    GLuint m_vao = 0, m_vbo = 0, m_ebo = 0;
    GLuint m_instanceVBO = 0;
    int m_indexCount = 0;

    void destroy() {
        if (m_instanceVBO) glDeleteBuffers(1, &m_instanceVBO);
        if (m_ebo) glDeleteBuffers(1, &m_ebo);
        if (m_vbo) glDeleteBuffers(1, &m_vbo);
        if (m_vao) glDeleteVertexArrays(1, &m_vao);
        m_vao = m_vbo = m_ebo = m_instanceVBO = 0;
        m_indexCount = 0;
    }
};

// ════════════════════════════════════════════════════════════════
// Mesh Primitives — Generadores de geometry 3D
// ════════════════════════════════════════════════════════════════
namespace MeshPrimitives {

/// Crear un cubo unitario [-0.5, 0.5]³ con normales y UVs
inline void createCube(Mesh3D& mesh) {
    // 24 vértices (4 por cara, normales separadas)
    // Format: posX, posY, posZ, normX, normY, normZ, u, v
    const float V[] = {
        // Front face (+Z)
        -0.5f, -0.5f,  0.5f,  0, 0, 1,  0, 0,
         0.5f, -0.5f,  0.5f,  0, 0, 1,  1, 0,
         0.5f,  0.5f,  0.5f,  0, 0, 1,  1, 1,
        -0.5f,  0.5f,  0.5f,  0, 0, 1,  0, 1,
        // Back face (-Z)
         0.5f, -0.5f, -0.5f,  0, 0,-1,  0, 0,
        -0.5f, -0.5f, -0.5f,  0, 0,-1,  1, 0,
        -0.5f,  0.5f, -0.5f,  0, 0,-1,  1, 1,
         0.5f,  0.5f, -0.5f,  0, 0,-1,  0, 1,
        // Right face (+X)
         0.5f, -0.5f,  0.5f,  1, 0, 0,  0, 0,
         0.5f, -0.5f, -0.5f,  1, 0, 0,  1, 0,
         0.5f,  0.5f, -0.5f,  1, 0, 0,  1, 1,
         0.5f,  0.5f,  0.5f,  1, 0, 0,  0, 1,
        // Left face (-X)
        -0.5f, -0.5f, -0.5f, -1, 0, 0,  0, 0,
        -0.5f, -0.5f,  0.5f, -1, 0, 0,  1, 0,
        -0.5f,  0.5f,  0.5f, -1, 0, 0,  1, 1,
        -0.5f,  0.5f, -0.5f, -1, 0, 0,  0, 1,
        // Top face (+Y)
        -0.5f,  0.5f,  0.5f,  0, 1, 0,  0, 0,
         0.5f,  0.5f,  0.5f,  0, 1, 0,  1, 0,
         0.5f,  0.5f, -0.5f,  0, 1, 0,  1, 1,
        -0.5f,  0.5f, -0.5f,  0, 1, 0,  0, 1,
        // Bottom face (-Y)
        -0.5f, -0.5f, -0.5f,  0,-1, 0,  0, 0,
         0.5f, -0.5f, -0.5f,  0,-1, 0,  1, 0,
         0.5f, -0.5f,  0.5f,  0,-1, 0,  1, 1,
        -0.5f, -0.5f,  0.5f,  0,-1, 0,  0, 1,
    };

    const uint32_t I[] = {
         0, 1, 2,  0, 2, 3,   // Front
         4, 5, 6,  4, 6, 7,   // Back
         8, 9,10,  8,10,11,   // Right
        12,13,14, 12,14,15,   // Left
        16,17,18, 16,18,19,   // Top
        20,21,22, 20,22,23,   // Bottom
    };

    mesh.create(V, sizeof(V) / sizeof(float), I, sizeof(I) / sizeof(uint32_t));
}

/// Crear un plano XZ unitario en Y=0 con subdivisiones
inline void createPlane(Mesh3D& mesh, int subdivisions = 1) {
    std::vector<float> verts;
    std::vector<uint32_t> indices;

    float step = 1.0f / subdivisions;
    for (int z = 0; z <= subdivisions; z++) {
        for (int x = 0; x <= subdivisions; x++) {
            float px = -0.5f + x * step;
            float pz = -0.5f + z * step;
            float u = static_cast<float>(x) / subdivisions;
            float v = static_cast<float>(z) / subdivisions;
            // pos3, normal3, uv2
            verts.insert(verts.end(), {px, 0.0f, pz,  0, 1, 0,  u, v});
        }
    }

    int w = subdivisions + 1;
    for (int z = 0; z < subdivisions; z++) {
        for (int x = 0; x < subdivisions; x++) {
            uint32_t tl = z * w + x;
            uint32_t tr = tl + 1;
            uint32_t bl = (z + 1) * w + x;
            uint32_t br = bl + 1;
            indices.insert(indices.end(), {tl, bl, tr,  tr, bl, br});
        }
    }

    mesh.create(verts.data(), verts.size(), indices.data(), indices.size());
}

/// Crear una UV sphere
inline void createSphere(Mesh3D& mesh, int segments = 32, int rings = 16) {
    std::vector<float> verts;
    std::vector<uint32_t> indices;

    for (int r = 0; r <= rings; r++) {
        float phi = math::MathUtils::PI * r / rings;
        float sinPhi = std::sin(phi);
        float cosPhi = std::cos(phi);

        for (int s = 0; s <= segments; s++) {
            float theta = 2.0f * math::MathUtils::PI * s / segments;
            float sinTheta = std::sin(theta);
            float cosTheta = std::cos(theta);

            float x = cosTheta * sinPhi;
            float y = cosPhi;
            float z = sinTheta * sinPhi;

            float u = static_cast<float>(s) / segments;
            float v = static_cast<float>(r) / rings;

            // pos=normal (unit sphere), uv
            verts.insert(verts.end(), {x * 0.5f, y * 0.5f, z * 0.5f,  x, y, z,  u, v});
        }
    }

    int w = segments + 1;
    for (int r = 0; r < rings; r++) {
        for (int s = 0; s < segments; s++) {
            uint32_t tl = r * w + s;
            uint32_t tr = tl + 1;
            uint32_t bl = (r + 1) * w + s;
            uint32_t br = bl + 1;
            indices.insert(indices.end(), {tl, bl, tr,  tr, bl, br});
        }
    }

    mesh.create(verts.data(), verts.size(), indices.data(), indices.size());
}

/// Crear un terreno (Heightmap/Heightfield) genérico
inline void createTerrainMesh(Mesh3D& mesh, const std::vector<float>& heights, int rows, int cols, float spX, float spZ) {
    if (heights.size() < static_cast<size_t>(rows * cols)) return;

    std::vector<float> verts;
    std::vector<uint32_t> indices;

    // Smooth normal calculation (Averaging adjacent triangle normals)
    std::vector<math::Vector3D> normals(rows * cols, math::Vector3D(0, 1, 0));
    for (int r = 0; r < rows - 1; r++) {
        for (int c = 0; c < cols - 1; c++) {
            math::Vector3D p0(c * spX, heights[r * cols + c], r * spZ);
            math::Vector3D p1((c+1) * spX, heights[r * cols + (c+1)], r * spZ);
            math::Vector3D p2(c * spX, heights[(r+1) * cols + c], (r+1) * spZ);
            math::Vector3D p3((c+1) * spX, heights[(r+1) * cols + (c+1)], (r+1) * spZ);
            
            math::Vector3D n1 = (p2 - p0).cross(p1 - p0).normalized();
            math::Vector3D n2 = (p1 - p3).cross(p2 - p3).normalized();
            
            normals[r * cols + c] += n1;
            normals[r * cols + c + 1] += n1 + n2;
            normals[(r+1) * cols + c] += n1 + n2;
            normals[(r+1) * cols + c + 1] += n2;
        }
    }

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            float px = c * spX;
            float py = heights[r * cols + c];
            float pz = r * spZ;
            
            math::Vector3D n = normals[r * cols + c].normalized();
            
            float u = static_cast<float>(c) / cols;
            float v = static_cast<float>(r) / rows;
            
            // Note: Multiply UVs by 10 or similar multiplier if tiled texture is desired, 
            // but we leave it [0,1] here so materials can control UV scale.
            verts.push_back(px);
            verts.push_back(py);
            verts.push_back(pz);
            verts.push_back(n.x);
            verts.push_back(n.y);
            verts.push_back(n.z);
            verts.push_back(u * 20.0f);
            verts.push_back(v * 20.0f);
        }
    }

    for (int r = 0; r < rows - 1; r++) {
        for (int c = 0; c < cols - 1; c++) {
            uint32_t tl = r * cols + c;
            uint32_t tr = tl + 1;
            uint32_t bl = (r + 1) * cols + c;
            uint32_t br = bl + 1;
            indices.push_back(tl);
            indices.push_back(bl);
            indices.push_back(tr);
            indices.push_back(tr);
            indices.push_back(bl);
            indices.push_back(br);
        }
    }

    mesh.create(verts.data(), verts.size(), indices.data(), indices.size());
}

} // namespace MeshPrimitives
} // namespace renderer
} // namespace engine
