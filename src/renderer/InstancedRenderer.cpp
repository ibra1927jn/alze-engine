#include "InstancedRenderer.h"
#include <glad/gl.h>

namespace engine {
namespace renderer {

bool InstancedRenderer::init() {
    glGenBuffers(1, &m_instanceVBO);
    m_instanceData.reserve(1024);
    m_initialized = true;
    return true;
}

void InstancedRenderer::shutdown() {
    if (m_instanceVBO) { glDeleteBuffers(1, &m_instanceVBO); m_instanceVBO = 0; }
    m_initialized = false;
}

void InstancedRenderer::begin() {
    m_instanceData.clear();
}

void InstancedRenderer::addInstance(const math::Matrix4x4& model,
                                    const math::Vector3D& color, float alpha)
{
    if (m_instanceData.size() >= MAX_INSTANCES) return;
    InstanceData inst;
    const float* d = model.data();
    for (int i = 0; i < 16; i++) inst.modelMatrix[i] = d[i];
    inst.color[0] = color.x; inst.color[1] = color.y;
    inst.color[2] = color.z; inst.color[3] = alpha;
    m_instanceData.push_back(inst);
}

void InstancedRenderer::render(const Mesh3D& mesh, ShaderProgram& /*shader*/) {
    if (m_instanceData.empty() || !m_initialized) return;
    if (!mesh.isValid()) return;

    int count = static_cast<int>(m_instanceData.size());

    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, count * sizeof(InstanceData),
                 m_instanceData.data(), GL_DYNAMIC_DRAW);

    glBindVertexArray(mesh.getVAO());
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);

    for (int i = 0; i < 4; i++) {
        GLuint loc = 3 + i;
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData),
                              (void*)(i * 4 * sizeof(float)));
        glVertexAttribDivisor(loc, 1);
    }

    glEnableVertexAttribArray(7);
    glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData),
                          (void*)(16 * sizeof(float)));
    glVertexAttribDivisor(7, 1);

    glDrawElementsInstanced(GL_TRIANGLES, mesh.getIndexCount(),
                            GL_UNSIGNED_INT, nullptr, count);

    for (int i = 0; i < 4; i++) {
        glVertexAttribDivisor(3 + i, 0);
        glDisableVertexAttribArray(3 + i);
    }
    glVertexAttribDivisor(7, 0);
    glDisableVertexAttribArray(7);

    glBindVertexArray(0);
    m_drawCalls++;
    m_totalInstances += count;
}

} // namespace renderer
} // namespace engine
