#pragma once

#include <glad/gl.h>
#include "ShaderProgram.h"
#include "Texture2D.h"
#include "math/Vector3D.h"
#include "math/Matrix4x4.h"
#include <cstdint>

namespace engine {
namespace renderer {

/// DecalRenderer — Projected decal system for surface marks.
class DecalRenderer {
public:
    static constexpr int MAX_DECALS = 512;

    struct Decal {
        math::Vector3D position;
        math::Vector3D normal;
        float size      = 1.0f;
        float rotation  = 0.0f;
        float lifetime  = 10.0f;
        float age       = 0.0f;
        float fadeStart = 8.0f;
        float u0 = 0, v0 = 0, u1 = 1, v1 = 1;
        bool active = false;
    };

    void init();
    int addDecal(const math::Vector3D& pos, const math::Vector3D& normal,
                 float size, float lifetime = 10.0f, float rotation = 0.0f);
    void setDecalUV(int id, float u0, float v0, float u1, float v1);
    void update(float dt);
    void render(const math::Matrix4x4& view, const math::Matrix4x4& proj,
                GLuint depthTexture, GLuint decalTexture);
    int getActiveCount() const { return m_activeCount; }
    void clearAll();
    void destroy();

private:
    math::Matrix4x4 buildDecalMatrix(const math::Vector3D& pos,
                                     const math::Vector3D& normal,
                                     float size, float rotation) const;

    Decal m_decals[MAX_DECALS] = {};
    int m_activeCount = 0;
    GLuint m_vao = 0, m_vbo = 0, m_ebo = 0;
    ShaderProgram m_shader;

    static constexpr const char* DECAL_VERT_SRC = R"(
        #version 330 core
        layout(location=0) in vec2 aPos;
        layout(location=1) in vec2 aUV;
        uniform mat4 uModel;
        uniform mat4 uView;
        uniform mat4 uProj;
        uniform vec4 uUVRegion;
        out vec2 vUV;
        out vec4 vClipPos;
        void main() {
            vec4 worldPos = uModel * vec4(aPos, 0.0, 1.0);
            vClipPos = uProj * uView * worldPos;
            gl_Position = vClipPos;
            vUV = vec2(mix(uUVRegion.x, uUVRegion.z, aUV.x),
                       mix(uUVRegion.y, uUVRegion.w, aUV.y));
        }
    )";

    static constexpr const char* DECAL_FRAG_SRC = R"(
        #version 330 core
        in vec2 vUV;
        in vec4 vClipPos;
        uniform sampler2D uDecalTex;
        uniform float uAlpha;
        out vec4 FragColor;
        void main() {
            vec4 texColor = texture(uDecalTex, vUV);
            vec2 edgeDist = abs(vUV - 0.5) * 2.0;
            float edgeFade = 1.0 - smoothstep(0.8, 1.0, max(edgeDist.x, edgeDist.y));
            FragColor = vec4(texColor.rgb, texColor.a * uAlpha * edgeFade);
        }
    )";
};

} // namespace renderer
} // namespace engine
