#pragma once

#include <glad/gl.h>
#include "MeshPrimitives.h"
#include "ShaderProgram.h"
#include "math/Matrix4x4.h"
#include <vector>

namespace engine {
namespace renderer {

class InstancedRenderer {
public:
    static constexpr int MAX_INSTANCES = 50000;

    InstancedRenderer() = default;
    ~InstancedRenderer() { shutdown(); }
    InstancedRenderer(const InstancedRenderer&) = delete;
    InstancedRenderer& operator=(const InstancedRenderer&) = delete;

    bool init();
    void shutdown();
    void begin();
    void addInstance(const math::Matrix4x4& model,
                     const math::Vector3D& color = math::Vector3D(1, 1, 1),
                     float alpha = 1.0f);
    void render(const Mesh3D& mesh, ShaderProgram& shader);

    int  getDrawCalls() const { return m_drawCalls; }
    int  getTotalInstances() const { return m_totalInstances; }
    int  getPendingCount() const { return static_cast<int>(m_instanceData.size()); }
    void resetStats() { m_drawCalls = 0; m_totalInstances = 0; }

    static constexpr const char* INSTANCED_VS = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in vec3 aNormal;
        layout(location = 2) in vec2 aTexCoord;
        layout(location = 3) in mat4 aInstanceModel;
        layout(location = 7) in vec4 aInstanceColor;
        uniform mat4 uView;
        uniform mat4 uProjection;
        out vec3 vWorldPos;
        out vec3 vNormal;
        out vec2 vTexCoord;
        out vec4 vInstanceColor;
        void main() {
            vec4 worldPos = aInstanceModel * vec4(aPos, 1.0);
            gl_Position = uProjection * uView * worldPos;
            vWorldPos = worldPos.xyz;
            vNormal = mat3(aInstanceModel) * aNormal;
            vTexCoord = aTexCoord;
            vInstanceColor = aInstanceColor;
        }
    )";

    static constexpr const char* INSTANCED_FS = R"(
        #version 330 core
        in vec3 vWorldPos;
        in vec3 vNormal;
        in vec2 vTexCoord;
        in vec4 vInstanceColor;
        uniform vec3 uViewPos;
        uniform vec3 uLightDir;
        uniform vec3 uLightColor;
        out vec4 FragColor;
        void main() {
            vec3 N = normalize(vNormal);
            vec3 L = normalize(-uLightDir);
            float diff = max(dot(N, L), 0.0);
            vec3 ambient = 0.15 * uLightColor;
            vec3 diffuse = diff * uLightColor;
            vec3 color = (ambient + diffuse) * vInstanceColor.rgb;
            FragColor = vec4(color, vInstanceColor.a);
        }
    )";

private:
    struct InstanceData {
        float modelMatrix[16];
        float color[4];
    };

    bool   m_initialized  = false;
    GLuint m_instanceVBO  = 0;
    std::vector<InstanceData> m_instanceData;
    int m_drawCalls      = 0;
    int m_totalInstances = 0;
};

} // namespace renderer
} // namespace engine
