#pragma once

#include <glad/gl.h>
#include "ShaderProgram.h"
#include "math/Matrix4x4.h"
#include "math/Vector3D.h"
#include "Skybox.h"
#include <cmath>
#include <vector>
#include "ImageDecoder.h"

namespace engine {
namespace renderer {

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// IBL Shaders
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

namespace IBLShaders {

/// Vertex shader for rendering to cubemap faces
inline const char* CUBEMAP_VERT = R"GLSL(
#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 uView;
uniform mat4 uProjection;
out vec3 vWorldPos;
void main() {
    vWorldPos = aPos;
    gl_Position = uProjection * uView * vec4(aPos, 1.0);
}
)GLSL";

/// Capture procedural sky to cubemap
inline const char* SKY_CAPTURE_FRAG = R"GLSL(
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

    float sunDot = max(dot(dir, normalize(uSunDir)), 0.0);
    sky += uSunColor * pow(sunDot, uSunSize);
    sky += uSunColor * pow(sunDot, uSunSize * 0.1) * 0.15;

    FragColor = vec4(sky, 1.0);
}
)GLSL";

/// Irradiance convolution (diffuse IBL)
inline const char* IRRADIANCE_FRAG = R"GLSL(
#version 330 core
in vec3 vWorldPos;
out vec4 FragColor;
uniform samplerCube uEnvironmentMap;

const float PI = 3.14159265359;

void main() {
    vec3 N = normalize(vWorldPos);
    vec3 irradiance = vec3(0.0);

    // Tangent frame
    vec3 up    = vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(up, N));
    up         = normalize(cross(N, right));

    float sampleDelta = 0.025;
    float nrSamples = 0.0;

    for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta) {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta) {
            // Spherical to cartesian (tangent space)
            vec3 tangentSample = vec3(
                sin(theta) * cos(phi),
                sin(theta) * sin(phi),
                cos(theta)
            );
            // Tangent space to world
            vec3 sampleVec = tangentSample.x * right +
                             tangentSample.y * up +
                             tangentSample.z * N;

            irradiance += texture(uEnvironmentMap, sampleVec).rgb *
                          cos(theta) * sin(theta);
            nrSamples++;
        }
    }
    irradiance = PI * irradiance * (1.0 / nrSamples);
    FragColor = vec4(irradiance, 1.0);
}
)GLSL";

/// Pre-filter environment map (specular IBL)
inline const char* PREFILTER_FRAG = R"GLSL(
#version 330 core
in vec3 vWorldPos;
out vec4 FragColor;
uniform samplerCube uEnvironmentMap;
uniform float uRoughness;

const float PI = 3.14159265359;
const uint SAMPLE_COUNT = 1024u;

// Van der Corput radical inverse
float RadicalInverse_VdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

vec2 Hammersley(uint i, uint N) {
    return vec2(float(i) / float(N), RadicalInverse_VdC(i));
}

vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness) {
    float a = roughness * roughness;
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    vec3 H = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);

    vec3 up = abs(N.z) < 0.999 ? vec3(0, 0, 1) : vec3(1, 0, 0);
    vec3 tangent   = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);

    return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

void main() {
    vec3 N = normalize(vWorldPos);
    vec3 R = N;
    vec3 V = R;

    float totalWeight = 0.0;
    vec3 prefilteredColor = vec3(0.0);

    for (uint i = 0u; i < SAMPLE_COUNT; i++) {
        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        vec3 H  = ImportanceSampleGGX(Xi, N, uRoughness);
        vec3 L  = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0) {
            prefilteredColor += texture(uEnvironmentMap, L).rgb * NdotL;
            totalWeight += NdotL;
        }
    }
    prefilteredColor /= totalWeight;
    FragColor = vec4(prefilteredColor, 1.0);
}
)GLSL";

/// BRDF integration LUT (split-sum)
inline const char* BRDF_VERT = R"GLSL(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aUV;
out vec2 vUV;
void main() {
    vUV = aUV;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)GLSL";

inline const char* BRDF_FRAG = R"GLSL(
#version 330 core
in vec2 vUV;
out vec2 FragColor;

const float PI = 3.14159265359;
const uint SAMPLE_COUNT = 1024u;

float RadicalInverse_VdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

vec2 Hammersley(uint i, uint N) {
    return vec2(float(i) / float(N), RadicalInverse_VdC(i));
}

vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness) {
    float a = roughness * roughness;
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    vec3 H = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
    vec3 up = abs(N.z) < 0.999 ? vec3(0, 0, 1) : vec3(1, 0, 0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);
    return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float a = roughness;
    float k = (a * a) / 2.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) *
           GeometrySchlickGGX(NdotL, roughness);
}

void main() {
    float NdotV = vUV.x;
    float roughness = vUV.y;

    vec3 V = vec3(sqrt(1.0 - NdotV * NdotV), 0.0, NdotV);
    vec3 N = vec3(0.0, 0.0, 1.0);

    float A = 0.0;  // scale
    float B = 0.0;  // bias

    for (uint i = 0u; i < SAMPLE_COUNT; i++) {
        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        vec3 H  = ImportanceSampleGGX(Xi, N, roughness);
        vec3 L  = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(L.z, 0.0);
        float NdotH = max(H.z, 0.0);
        float VdotH = max(dot(V, H), 0.0);

        if (NdotL > 0.0) {
            float G = GeometrySmith(N, V, L, roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotV);
            float Fc = pow(1.0 - VdotH, 5.0);

            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    A /= float(SAMPLE_COUNT);
    B /= float(SAMPLE_COUNT);
    FragColor = vec2(A, B);
}
)GLSL";

} // namespace IBLShaders

namespace HDRIShaders {

/// Convert equirectangular HDR map to cubemap
inline const char* EQUIRECT_TO_CUBE_FRAG = R"GLSL(
#version 330 core
in vec3 vWorldPos;
out vec4 FragColor;
uniform sampler2D uEquirectangularMap;

const vec2 invAtan = vec2(0.1591, 0.3183); // 1/(2*PI), 1/PI

vec2 sampleSphericalMap(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

void main() {
    vec2 uv = sampleSphericalMap(normalize(vWorldPos));
    vec3 color = texture(uEquirectangularMap, uv).rgb;
    FragColor = vec4(color, 1.0);
}
)GLSL";

} // namespace HDRIShaders

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// EnvironmentMap â€” Full IBL pipeline
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

class EnvironmentMap {
public:
    EnvironmentMap() = default;
    ~EnvironmentMap() { destroy(); }

    /// Generate all IBL resources from sky parameters
    bool generate(const SkyParams& sky, int envSize = 256, int irrSize = 32, int pfSize = 128);  // impl in EnvironmentMap.cpp
    bool generateFromHDRI(const char* hdrPath, int envSize = 512, int irrSize = 32, int pfSize = 128);  // impl in EnvironmentMap.cpp
    void bind(int irradianceSlot = 4, int prefilterSlot = 5, int brdfSlot = 6) const;  // impl in EnvironmentMap.cpp
    bool isValid() const { return m_envCubemap != 0; }
    GLuint getEnvCubemap() const { return m_envCubemap; }
    GLuint getIrradianceMap() const { return m_irradianceMap; }
    GLuint getPrefilterMap() const { return m_prefilterMap; }
    GLuint getBRDFLUT() const { return m_brdfLUT; }

private:
    GLuint m_envCubemap = 0;
    GLuint m_irradianceMap = 0;
    GLuint m_prefilterMap = 0;
    GLuint m_brdfLUT = 0;

    ShaderProgram m_captureShader, m_irradianceShader, m_prefilterShader, m_brdfShader;
    GLuint m_cubeVAO = 0, m_cubeVBO = 0;
    GLuint m_quadVAO = 0, m_quadVBO = 0, m_quadEBO = 0;
    int m_envSize = 256;

    // impl in EnvironmentMap.cpp
    math::Matrix4x4 buildView(const float raw[16]);
    void generateIBLFromCubemap(GLuint captureFBO, GLuint captureRBO,
                                float viewMatData[6][16],
                                const math::Matrix4x4& captureProj,
                                int irrSize, int pfSize);
    void computeCubemapViews(float out[6][16]);
    void createCube();
    void createQuad();
    void drawCube();
    void drawQuad();
    void destroy();
};

} // namespace renderer
} // namespace engine
