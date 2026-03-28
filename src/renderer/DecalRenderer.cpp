#include "DecalRenderer.h"
#include <glad/gl.h>
#include <cmath>
#include <cstdint>

namespace engine {
namespace renderer {

void DecalRenderer::init() {
    float quadVerts[] = { -1,-1,0,0, 1,-1,1,0, 1,1,1,1, -1,1,0,1 };
    uint32_t quadIdx[] = {0,1,2, 0,2,3};

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quadIdx), quadIdx, GL_STATIC_DRAW);
    glBindVertexArray(0);

    m_shader.compile(DECAL_VERT_SRC, DECAL_FRAG_SRC);
}

int DecalRenderer::addDecal(const math::Vector3D& pos, const math::Vector3D& normal,
                             float size, float lifetime, float rotation)
{
    for (int i = 0; i < MAX_DECALS; i++) {
        if (!m_decals[i].active) {
            m_decals[i].position  = pos;
            m_decals[i].normal    = normal.normalized();
            m_decals[i].size      = size;
            m_decals[i].rotation  = rotation;
            m_decals[i].lifetime  = lifetime;
            m_decals[i].fadeStart = lifetime * 0.8f;
            m_decals[i].age       = 0;
            m_decals[i].active    = true;
            m_activeCount++;
            return i;
        }
    }
    return -1;
}

void DecalRenderer::setDecalUV(int id, float u0, float v0, float u1, float v1) {
    if (id >= 0 && id < MAX_DECALS) {
        m_decals[id].u0 = u0; m_decals[id].v0 = v0;
        m_decals[id].u1 = u1; m_decals[id].v1 = v1;
    }
}

void DecalRenderer::update(float dt) {
    for (int i = 0; i < MAX_DECALS; i++) {
        if (!m_decals[i].active) continue;
        m_decals[i].age += dt;
        if (m_decals[i].age >= m_decals[i].lifetime) {
            m_decals[i].active = false;
            m_activeCount--;
        }
    }
}

void DecalRenderer::render(const math::Matrix4x4& view, const math::Matrix4x4& proj,
                            GLuint depthTexture, GLuint decalTexture)
{
    if (m_activeCount == 0) return;

    m_shader.use();
    m_shader.setMat4("uView", view);
    m_shader.setMat4("uProj", proj);

    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, depthTexture);
    m_shader.setInt("uDepthTex", 0);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, decalTexture);
    m_shader.setInt("uDecalTex", 1);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glBindVertexArray(m_vao);

    for (int i = 0; i < MAX_DECALS; i++) {
        if (!m_decals[i].active) continue;
        const Decal& d = m_decals[i];
        float alpha = 1.0f;
        if (d.age > d.fadeStart)
            alpha = 1.0f - (d.age - d.fadeStart) / (d.lifetime - d.fadeStart);
        math::Matrix4x4 model = buildDecalMatrix(d.position, d.normal, d.size, d.rotation);
        m_shader.setMat4("uModel", model);
        m_shader.setFloat("uAlpha", alpha);
        m_shader.setVec4("uUVRegion", d.u0, d.v0, d.u1, d.v1);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }

    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
}

void DecalRenderer::clearAll() {
    for (auto& d : m_decals) d.active = false;
    m_activeCount = 0;
}

void DecalRenderer::destroy() {
    if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
    if (m_vbo) { glDeleteBuffers(1, &m_vbo);       m_vbo = 0; }
    if (m_ebo) { glDeleteBuffers(1, &m_ebo);       m_ebo = 0; }
    m_shader = ShaderProgram{};
}

math::Matrix4x4 DecalRenderer::buildDecalMatrix(const math::Vector3D& pos,
                                                  const math::Vector3D& normal,
                                                  float size, float rotation) const
{
    math::Vector3D up = std::abs(normal.y) < 0.99f ? math::Vector3D::Up : math::Vector3D::Right;
    math::Vector3D tangent   = up.cross(normal).normalized();
    math::Vector3D bitangent = normal.cross(tangent).normalized();

    if (rotation != 0.0f) {
        float c = std::cos(rotation), s = std::sin(rotation);
        math::Vector3D newT = tangent * c + bitangent * s;
        math::Vector3D newB = bitangent * c - tangent * s;
        tangent = newT; bitangent = newB;
    }

    math::Matrix4x4 m = math::Matrix4x4::identity();
    m.set(0,0,tangent.x*size);   m.set(1,0,tangent.y*size);   m.set(2,0,tangent.z*size);
    m.set(0,1,normal.x*size);    m.set(1,1,normal.y*size);    m.set(2,1,normal.z*size);
    m.set(0,2,bitangent.x*size); m.set(1,2,bitangent.y*size); m.set(2,2,bitangent.z*size);
    m.set(0,3,pos.x); m.set(1,3,pos.y); m.set(2,3,pos.z);
    return m;
}

} // namespace renderer
} // namespace engine
