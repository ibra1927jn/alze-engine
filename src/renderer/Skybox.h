#pragma once

#include <glad/gl.h>
#include "ShaderProgram.h"
#include "math/Matrix4x4.h"
#include <cmath>

namespace engine {
namespace renderer {

// ── Skybox Shader (embebido) ───────────────────────────────────

namespace SkyboxShaders {

inline const char* VERT = R"GLSL(
#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vWorldPos;

void main() {
    vWorldPos = aPos;
    mat4 rotView = mat4(mat3(uView));
    vec4 pos = uProjection * rotView * vec4(aPos, 1.0);
    gl_Position = pos.xyww;
}
)GLSL";

inline const char* FRAG = R"GLSL(
#version 330 core
in vec3 vWorldPos;
out vec4 FragColor;

uniform vec3 uSkyColorTop;
uniform vec3 uSkyColorHorizon;
uniform vec3 uSkyColorBottom;
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform float uSunSize;

void main() {
    vec3 dir = normalize(vWorldPos);
    float t = dir.y * 0.5 + 0.5;
    vec3 sky;
    if (t < 0.5)
        sky = mix(uSkyColorBottom, uSkyColorHorizon, t * 2.0);
    else
        sky = mix(uSkyColorHorizon, uSkyColorTop, (t - 0.5) * 2.0);
    float sunDot = max(dot(dir, uSunDir), 0.0);
    float sunFactor = pow(sunDot, uSunSize);
    sky += uSunColor * sunFactor;
    float glowFactor = pow(sunDot, uSunSize * 0.1);
    sky += uSunColor * glowFactor * 0.15;
    FragColor = vec4(sky, 1.0);
}
)GLSL";

} // namespace SkyboxShaders

namespace SkyboxCubemapShaders {

inline const char* FRAG = R"GLSL(
#version 330 core
in vec3 vWorldPos;
out vec4 FragColor;
uniform samplerCube uEnvironmentMap;
uniform float uExposure;
void main() {
    vec3 color = texture(uEnvironmentMap, vWorldPos).rgb;
    // Optional exposure control for HDR cubemaps
    color *= uExposure;
    FragColor = vec4(color, 1.0);
}
)GLSL";

} // namespace SkyboxCubemapShaders

/// Sky parameters — defined outside Skybox to avoid GCC 15 ordering issues
struct SkyParams {
    math::Vector3D topColor     = math::Vector3D(0.15f, 0.25f, 0.55f);
    math::Vector3D horizonColor = math::Vector3D(0.55f, 0.65f, 0.85f);
    math::Vector3D bottomColor  = math::Vector3D(0.25f, 0.22f, 0.28f);
    math::Vector3D sunDir       = math::Vector3D(0.3f, 0.6f, -0.5f);
    math::Vector3D sunColor     = math::Vector3D(1.0f, 0.9f, 0.7f);
    float sunSize               = 256.0f;
};

/// Skybox — Cielo procedural con gradiente y sol.
class Skybox {
public:
    Skybox() = default;
    ~Skybox() { destroy(); }

    bool init() {
        if (!m_shader.compile(SkyboxShaders::VERT, SkyboxShaders::FRAG))
            return false;

        const float V[] = {
            -1, -1, -1,   1, -1, -1,   1,  1, -1,  -1,  1, -1,
            -1, -1,  1,   1, -1,  1,   1,  1,  1,  -1,  1,  1,
        };
        const uint32_t I[] = {
            0,1,2, 0,2,3,  5,4,7, 5,7,6,  4,0,3, 4,3,7,
            1,5,6, 1,6,2,  3,2,6, 3,6,7,  4,5,1, 4,1,0,
        };

        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);
        glGenBuffers(1, &m_ebo);

        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(V), V, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(I), I, GL_STATIC_DRAW);
        glBindVertexArray(0);

        return true;
    }

    void draw(const math::Matrix4x4& view, const math::Matrix4x4& projection,
              const SkyParams& params = SkyParams()) {
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE);

        m_shader.use();
        m_shader.setMat4("uView", view);
        m_shader.setMat4("uProjection", projection);
        m_shader.setVec3("uSkyColorTop", params.topColor);
        m_shader.setVec3("uSkyColorHorizon", params.horizonColor);
        m_shader.setVec3("uSkyColorBottom", params.bottomColor);
        m_shader.setVec3("uSunDir", params.sunDir.normalized());
        m_shader.setVec3("uSunColor", params.sunColor);
        m_shader.setFloat("uSunSize", params.sunSize);

        glBindVertexArray(m_vao);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        m_shader.unbind();
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
    }

    /// Draw skybox from an existing cubemap texture (e.g. HDRI environment)
    void drawCubemap(const math::Matrix4x4& view, const math::Matrix4x4& projection,
                     GLuint cubemapHandle, float exposure = 1.0f) {
        if (!m_cubeShader.isValid()) {
            m_cubeShader.compile(SkyboxShaders::VERT, SkyboxCubemapShaders::FRAG);
        }

        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE);

        m_cubeShader.use();
        m_cubeShader.setMat4("uView", view);
        m_cubeShader.setMat4("uProjection", projection);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapHandle);
        m_cubeShader.setInt("uEnvironmentMap", 0);
        m_cubeShader.setFloat("uExposure", exposure);

        glBindVertexArray(m_vao);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        m_cubeShader.unbind();
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
    }

private:
    ShaderProgram m_shader;
    ShaderProgram m_cubeShader;
    GLuint m_vao = 0, m_vbo = 0, m_ebo = 0;

    void destroy() {
        if (m_ebo) glDeleteBuffers(1, &m_ebo);
        if (m_vbo) glDeleteBuffers(1, &m_vbo);
        if (m_vao) glDeleteVertexArrays(1, &m_vao);
        m_vao = m_vbo = m_ebo = 0;
    }
};

} // namespace renderer
} // namespace engine
