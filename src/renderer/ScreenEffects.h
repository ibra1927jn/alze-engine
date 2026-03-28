#pragma once

#include <glad/gl.h>
#include "ShaderProgram.h"
#include <cstdlib>

namespace engine {
namespace renderer {

/// ScreenSpaceReflections
class ScreenSpaceReflections {
public:
    ScreenSpaceReflections() = default;
    ~ScreenSpaceReflections() { shutdown(); }

    struct Settings {
        float maxDistance = 50.0f;
        float resolution  = 0.5f;
        int   maxSteps    = 64;
        float thickness   = 0.5f;
        float fadeEdge    = 0.1f;
        float intensity   = 0.8f;
    };

    Settings settings;

    bool init(int width, int height);
    void shutdown();
    void apply(GLuint gPosition, GLuint gNormal, GLuint sceneColor,
               GLuint depthTexture,
               const float* viewData, const float* projData);

    GLuint getSSRTexture() const { return m_ssrTexture; }

private:
    bool m_initialized = false;
    int m_width = 0, m_height = 0;
    GLuint m_fbo = 0, m_ssrTexture = 0;
    GLuint m_quadVAO = 0, m_quadVBO = 0;
    ShaderProgram m_shader;

    void createQuad();

    static constexpr const char* SSR_VS = R"(
        #version 330 core
        layout(location = 0) in vec2 aPos;
        layout(location = 1) in vec2 aUV;
        out vec2 vUV;
        void main() { vUV = aUV; gl_Position = vec4(aPos, 0, 1); }
    )";

    static constexpr const char* SSR_FS = R"(
        #version 330 core
        out vec4 FragColor;
        in vec2 vUV;
        uniform sampler2D gPosition;
        uniform sampler2D gNormal;
        uniform sampler2D uSceneColor;
        uniform sampler2D uDepth;
        uniform mat4 uView;
        uniform mat4 uProjection;
        uniform float uMaxDistance;
        uniform int   uMaxSteps;
        uniform float uThickness;
        uniform float uIntensity;
        uniform vec2  uScreenSize;
        vec3 worldToScreen(vec3 worldPos) {
            vec4 clipPos = uProjection * uView * vec4(worldPos, 1.0);
            vec3 ndc = clipPos.xyz / clipPos.w;
            return vec3(ndc.xy * 0.5 + 0.5, ndc.z * 0.5 + 0.5);
        }
        void main() {
            vec4 posData  = texture(gPosition, vUV);
            vec4 normData = texture(gNormal,   vUV);
            vec3 sceneColor = texture(uSceneColor, vUV).rgb;
            vec3 worldPos = posData.xyz;
            float metallic = posData.a;
            vec3 N = normalize(normData.xyz);
            float roughness = normData.a;
            if (length(normData.xyz) < 0.01 || metallic < 0.01) {
                FragColor = vec4(sceneColor, 1.0); return;
            }
            vec3 viewDir = normalize(worldPos - inverse(uView)[3].xyz);
            vec3 reflDir = reflect(viewDir, N);
            vec3 startPos = worldToScreen(worldPos);
            vec3 endPos   = worldToScreen(worldPos + reflDir * uMaxDistance);
            vec3 rayDir   = endPos - startPos;
            float stepSize = 1.0 / float(uMaxSteps);
            vec3 currentPos = startPos;
            vec3 step = rayDir * stepSize;
            vec3 reflColor = vec3(0.0);
            float hitMask = 0.0;
            for (int i = 0; i < uMaxSteps; i++) {
                currentPos += step;
                if (currentPos.x < 0.0 || currentPos.x > 1.0 || currentPos.y < 0.0 || currentPos.y > 1.0) break;
                float sampledDepth = texture(uDepth, currentPos.xy).r;
                float rayDepth = currentPos.z;
                if (rayDepth > sampledDepth && rayDepth - sampledDepth < uThickness) {
                    reflColor = texture(uSceneColor, currentPos.xy).rgb;
                    vec2 edgeDist = abs(currentPos.xy - 0.5) * 2.0;
                    float edgeFade = clamp((1.0 - max(edgeDist.x, edgeDist.y)) * 4.0, 0.0, 1.0);
                    hitMask = edgeFade * (1.0 - roughness) * metallic;
                    break;
                }
            }
            FragColor = vec4(mix(sceneColor, reflColor, hitMask * uIntensity), 1.0);
        }
    )";
};

/// MotionBlur
class MotionBlur {
public:
    MotionBlur() = default;
    ~MotionBlur() { shutdown(); }

    struct Settings {
        float intensity = 0.5f;
        int   samples   = 8;
        float maxBlur   = 0.05f;
    };

    Settings settings;

    bool init(int width, int height);
    void shutdown();
    void apply(GLuint sceneColor, GLuint depthTexture,
               const float* currentVPInverse, const float* prevVP);

    GLuint getOutputTexture() const { return m_outputTex; }

private:
    bool m_initialized = false;
    int m_width = 0, m_height = 0;
    GLuint m_fbo = 0, m_outputTex = 0;
    GLuint m_quadVAO = 0, m_quadVBO = 0;
    ShaderProgram m_shader;

    void createQuad();

    static constexpr const char* MB_VS = R"(
        #version 330 core
        layout(location = 0) in vec2 aPos;
        layout(location = 1) in vec2 aUV;
        out vec2 vUV;
        void main() { vUV = aUV; gl_Position = vec4(aPos, 0, 1); }
    )";

    static constexpr const char* MB_FS = R"(
        #version 330 core
        out vec4 FragColor;
        in vec2 vUV;
        uniform sampler2D uSceneColor;
        uniform sampler2D uDepth;
        uniform mat4 uCurrentVPInverse;
        uniform mat4 uPrevVP;
        uniform float uIntensity;
        uniform int   uSamples;
        uniform float uMaxBlur;
        void main() {
            float depth = texture(uDepth, vUV).r;
            vec4 clipPos = vec4(vUV * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
            vec4 worldPos = uCurrentVPInverse * clipPos;
            worldPos /= worldPos.w;
            vec4 prevClip = uPrevVP * worldPos;
            vec2 prevUV = (prevClip.xy / prevClip.w) * 0.5 + 0.5;
            vec2 velocity = (vUV - prevUV) * uIntensity;
            float speed = length(velocity);
            if (speed > uMaxBlur) velocity = velocity / speed * uMaxBlur;
            vec3 result = vec3(0.0);
            for (int i = 0; i < uSamples; i++) {
                float t = float(i) / float(uSamples - 1) - 0.5;
                vec2 sampleUV = clamp(vUV + velocity * t, 0.0, 1.0);
                result += texture(uSceneColor, sampleUV).rgb;
            }
            FragColor = vec4(result / float(uSamples), 1.0);
        }
    )";
};

} // namespace renderer
} // namespace engine
