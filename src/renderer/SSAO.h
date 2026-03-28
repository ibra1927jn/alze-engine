#pragma once

#include <glad/gl.h>
#include "ShaderProgram.h"
#include <cstdlib>
#include <cmath>
#include <iostream>

namespace engine {
namespace renderer {

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// SSAO Shaders
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

namespace SSAOShaders {

inline const char* SSAO_VERT = R"GLSL(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aUV;
out vec2 vUV;
void main() {
    vUV = aUV;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)GLSL";

/// SSAO â€” Screen-space ambient occlusion using depth-only reconstruction
/// Reconstructs view-space position from depth buffer + inverse projection
inline const char* SSAO_FRAG = R"GLSL(
#version 330 core
in vec2 vUV;
out float FragColor;

uniform sampler2D uDepthTex;
uniform sampler2D uNoiseTex;

uniform vec3  uSamples[32];     // Random hemisphere samples
uniform mat4  uProjection;
uniform mat4  uInvProjection;
uniform vec2  uNoiseScale;      // screen / noise texture size
uniform float uRadius;
uniform float uBias;
uniform float uPower;

// Reconstruct view-space position from depth
vec3 viewPosFromDepth(vec2 uv) {
    float depth = texture(uDepthTex, uv).r;
    // NDC
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = uInvProjection * ndc;
    return viewPos.xyz / viewPos.w;
}

void main() {
    vec3 fragPos = viewPosFromDepth(vUV);

    // Early out for sky (far plane)
    float rawDepth = texture(uDepthTex, vUV).r;
    if (rawDepth >= 0.9999) { FragColor = 1.0; return; }

    // Approximate normal from depth derivatives
    vec3 dFdxPos = viewPosFromDepth(vUV + vec2(1.0/textureSize(uDepthTex, 0).x, 0)) - fragPos;
    vec3 dFdyPos = viewPosFromDepth(vUV + vec2(0, 1.0/textureSize(uDepthTex, 0).y)) - fragPos;
    vec3 normal = normalize(cross(dFdxPos, dFdyPos));

    // Random rotation from noise texture
    vec3 randomVec = texture(uNoiseTex, vUV * uNoiseScale).xyz;

    // Gram-Schmidt: create TBN that orients hemisphere along normal
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    // Sample hemisphere
    float occlusion = 0.0;
    int numSamples = 32;
    for (int i = 0; i < numSamples; i++) {
        // Orient sample in view space
        vec3 samplePos = fragPos + TBN * uSamples[i] * uRadius;

        // Project sample to screen
        vec4 offset = uProjection * vec4(samplePos, 1.0);
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5 + 0.5;

        // Compare depth
        float sampleDepth = viewPosFromDepth(offset.xy).z;

        // Range check (avoid darkening from distant geometry)
        float rangeCheck = smoothstep(0.0, 1.0, uRadius / abs(fragPos.z - sampleDepth));
        occlusion += (sampleDepth >= samplePos.z + uBias ? 1.0 : 0.0) * rangeCheck;
    }

    occlusion = 1.0 - (occlusion / float(numSamples));
    FragColor = pow(occlusion, uPower);
}
)GLSL";

/// Depth-aware bilateral blur for SSAO (prevents edge bleeding)
inline const char* BLUR_FRAG = R"GLSL(
#version 330 core
in vec2 vUV;
out float FragColor;
uniform sampler2D uSSAOTex;
uniform sampler2D uDepthTex;

void main() {
    vec2 texelSize = 1.0 / textureSize(uSSAOTex, 0);
    float centerAO = texture(uSSAOTex, vUV).r;
    float centerDepth = texture(uDepthTex, vUV).r;

    float totalAO = 0.0;
    float totalWeight = 0.0;

    // 7x7 bilateral blur with Gaussian weights
    for (int x = -3; x <= 3; x++) {
        for (int y = -3; y <= 3; y++) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            float sampleAO = texture(uSSAOTex, vUV + offset).r;
            float sampleDepth = texture(uDepthTex, vUV + offset).r;

            // Spatial weight (Gaussian-ish)
            float spatialW = exp(-0.5 * float(x*x + y*y) / 4.0);

            // Depth weight: reject across depth discontinuities
            float depthDiff = abs(centerDepth - sampleDepth);
            float depthW = exp(-depthDiff * depthDiff * 10000.0);

            float w = spatialW * depthW;
            totalAO += sampleAO * w;
            totalWeight += w;
        }
    }
    FragColor = totalAO / max(totalWeight, 0.001);
}
)GLSL";

} // namespace SSAOShaders

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// SSAO System
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

/// SSAO â€” Screen-Space Ambient Occlusion.
///
/// Adds contact darkening where surfaces meet, dramatically improving
/// perceived depth and realism. Uses depth buffer reconstruction
/// (no separate geometry pass needed).
///
/// Usage:
///   SSAO ssao;
///   ssao.init(1280, 720);
///
///   // After scene rendering:
///   ssao.generate(depthTexture, projectionMatrix);
///   GLuint aoTex = ssao.getResult();  // Bind in your lighting pass
///
class SSAO {
public:
    struct Settings {
        float radius = 0.5f;    // Sample radius in view space
        float bias   = 0.025f;  // Depth comparison bias
        float power  = 2.0f;    // AO intensity curve
        bool  enabled = true;
    };

    SSAO() = default;
    ~SSAO() { destroy(); }

    SSAO(const SSAO&) = delete;
    SSAO& operator=(const SSAO&) = delete;

    bool init(int width, int height);
    void resize(int width, int height);
    void generate(GLuint depthTexture, const math::Matrix4x4& projection);

    GLuint getResult() const { return m_blurTex; }
    bool isValid() const { return m_ssaoFBO != 0; }
    Settings& getSettings() { return m_settings; }

private:
    ShaderProgram m_ssaoShader, m_blurShader;
    GLuint m_ssaoFBO = 0, m_ssaoTex = 0;
    GLuint m_blurFBO = 0, m_blurTex = 0;
    GLuint m_noiseTex = 0;
    GLuint m_quadVAO = 0, m_quadVBO = 0;
    int m_width = 0, m_height = 0;
    float m_samples[32 * 3];
    Settings m_settings;

    GLint m_locDepthTex = -1, m_locNoiseTex = -1;
    GLint m_locProjection = -1, m_locInvProjection = -1;
    GLint m_locNoiseScale = -1, m_locRadius = -1, m_locBias = -1, m_locPower = -1;

    void generateSamples();
    void generateNoiseTexture();
    void createFBO(GLuint& fbo, GLuint& tex, int w, int h);
    void createQuad();
    void drawQuad();
    void destroy();
};

} // namespace renderer
} // namespace engine
