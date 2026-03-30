#pragma once

#include <glad/gl.h>
#include "ShaderProgram.h"
#include "MeshPrimitives.h"
#include "math/Matrix4x4.h"
#include <vector>

namespace engine {
namespace renderer {

/// DeferredRenderer â€” G-Buffer based deferred rendering pipeline.
///
/// Architecture (3-pass):
///   Pass 1 (Geometry): Render all objects once â†’ G-Buffer (position, normal, albedo, MR)
///   Pass 2 (Lighting): Full-screen quad, read G-Buffer, apply all lights â†’ lit scene
///   Pass 3 (Forward):  Transparent objects rendered forward on top
///
/// Advantages over Forward:
///   - O(objects + lights) instead of O(objects Ã— lights)
///   - Dozens/hundreds of lights at constant cost
///   - Screen-space effects (SSR, SSAO) have all geometry data ready
///
/// G-Buffer layout:
///   RT0: Position (RGB16F) + Metallic (A)
///   RT1: Normal   (RGB16F) + Roughness (A)
///   RT2: Albedo   (RGBA8)
///   Depth: GL_DEPTH_COMPONENT24
///
class DeferredRenderer {
public:
    DeferredRenderer() = default;
    ~DeferredRenderer() { shutdown(); }

    // Non-copyable
    DeferredRenderer(const DeferredRenderer&) = delete;
    DeferredRenderer& operator=(const DeferredRenderer&) = delete;

    /// Initialize G-Buffer FBO and shaders
    bool init(int width, int height);  // impl in DeferredRenderer.cpp

    void shutdown();  // impl in DeferredRenderer.cpp
    void resize(int width, int height);  // impl in DeferredRenderer.cpp

    // â”€â”€ Pass 1: Geometry â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

    /// Begin geometry pass (binds G-Buffer FBO, clears it)
    void beginGeometryPass(const math::Matrix4x4& view, const math::Matrix4x4& projection);  // impl in DeferredRenderer.cpp

    void submitGeometry(const Mesh3D& mesh, const math::Matrix4x4& model,
                        const math::Vector3D& albedo = math::Vector3D(0.8f, 0.8f, 0.8f),
                        float metallic = 0.0f, float roughness = 0.5f);  // impl in DeferredRenderer.cpp

    void endGeometryPass();  // impl in DeferredRenderer.cpp

    void lightingPass(const math::Vector3D& viewPos,
                      const math::Vector3D& lightDir,
                      const math::Vector3D& lightColor,
                      float ambientIntensity = 0.15f);  // impl in DeferredRenderer.cpp


    // â”€â”€ Accessors â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

    GLuint getPositionTexture() const { return m_gPosition; }
    GLuint getNormalTexture() const { return m_gNormal; }
    GLuint getAlbedoTexture() const { return m_gAlbedo; }
    GLuint getVelocityTexture() const { return m_gVelocity; }
    GLuint getDepthBuffer() const { return m_depthRBO; }
    GLuint getGBuffer() const { return m_gBuffer; }
    ShaderProgram& getGeometryShader() { return m_geometryShader; }

    bool isInitialized() const { return m_initialized; }

private:
    bool m_initialized = false;
    int m_width = 0, m_height = 0;

    // G-Buffer
    GLuint m_gBuffer = 0;
    GLuint m_gPosition = 0;  // RT0: Position.xyz + Metallic
    GLuint m_gNormal   = 0;  // RT1: Normal.xyz + Roughness
    GLuint m_gAlbedo   = 0;  // RT2: Albedo.rgba
    GLuint m_gVelocity = 0;  // RT3: Velocity.rg (for motion blur)
    GLuint m_depthRBO  = 0;

    // Shaders
    ShaderProgram m_geometryShader;
    ShaderProgram m_lightingShader;

    // View/Projection
    math::Matrix4x4 m_view, m_projection;

    // Fullscreen quad
    GLuint m_quadVAO = 0, m_quadVBO = 0;

    void createScreenQuad();  // impl in DeferredRenderer.cpp
    void cacheUniformLocations();  // impl in DeferredRenderer.cpp

    // Cached uniform locations — evita glGetUniformLocation por frame
    struct {
        // Geometry pass
        GLint geoView = -1, geoProjection = -1, geoModel = -1;
        GLint geoAlbedo = -1, geoMetallic = -1, geoRoughness = -1;
        // Lighting pass
        GLint litGPosition = -1, litGNormal = -1, litGAlbedo = -1;
        GLint litViewPos = -1, litLightDir = -1, litLightColor = -1;
        GLint litAmbientIntensity = -1;
    } m_uniforms;

    // â”€â”€ Shaders â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

    static constexpr const char* GEOMETRY_VS = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in vec3 aNormal;
        layout(location = 2) in vec2 aTexCoord;

        uniform mat4 uModel;
        uniform mat4 uView;
        uniform mat4 uProjection;

        out vec3 vWorldPos;
        out vec3 vNormal;
        out vec2 vTexCoord;

        void main() {
            vec4 worldPos = uModel * vec4(aPos, 1.0);
            vWorldPos = worldPos.xyz;
            vNormal = mat3(transpose(inverse(uModel))) * aNormal;
            vTexCoord = aTexCoord;
            gl_Position = uProjection * uView * worldPos;
        }
    )";

    static constexpr const char* GEOMETRY_FS = R"(
        #version 330 core
        layout(location = 0) out vec4 gPosition;
        layout(location = 1) out vec4 gNormal;
        layout(location = 2) out vec4 gAlbedo;

        in vec3 vWorldPos;
        in vec3 vNormal;
        in vec2 vTexCoord;

        uniform vec3 uAlbedo;
        uniform float uMetallic;
        uniform float uRoughness;

        void main() {
            gPosition = vec4(vWorldPos, uMetallic);
            gNormal   = vec4(normalize(vNormal), uRoughness);
            gAlbedo   = vec4(uAlbedo, 1.0);
        }
    )";

    static constexpr const char* LIGHTING_VS = R"(
        #version 330 core
        layout(location = 0) in vec2 aPos;
        layout(location = 1) in vec2 aTexCoord;

        out vec2 vTexCoord;

        void main() {
            vTexCoord = aTexCoord;
            gl_Position = vec4(aPos, 0.0, 1.0);
        }
    )";

    // PBR-style deferred lighting with Cook-Torrance BRDF
    static constexpr const char* LIGHTING_FS = R"(
        #version 330 core
        out vec4 FragColor;
        in vec2 vTexCoord;

        uniform sampler2D gPosition;
        uniform sampler2D gNormal;
        uniform sampler2D gAlbedo;

        uniform vec3 uViewPos;
        uniform vec3 uLightDir;
        uniform vec3 uLightColor;
        uniform float uAmbientIntensity;

        const float PI = 3.14159265359;

        // GGX Normal Distribution Function
        float DistributionGGX(vec3 N, vec3 H, float roughness) {
            float a = roughness * roughness;
            float a2 = a * a;
            float NdotH = max(dot(N, H), 0.0);
            float NdotH2 = NdotH * NdotH;
            float denom = NdotH2 * (a2 - 1.0) + 1.0;
            return a2 / (PI * denom * denom);
        }

        // Schlick-GGX Geometry Function
        float GeometrySchlickGGX(float NdotV, float roughness) {
            float r = roughness + 1.0;
            float k = (r * r) / 8.0;
            return NdotV / (NdotV * (1.0 - k) + k);
        }

        float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
            return GeometrySchlickGGX(max(dot(N, V), 0.0), roughness) *
                   GeometrySchlickGGX(max(dot(N, L), 0.0), roughness);
        }

        // Fresnel-Schlick
        vec3 fresnelSchlick(float cosTheta, vec3 F0) {
            return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
        }

        void main() {
            vec4 posData = texture(gPosition, vTexCoord);
            vec4 normData = texture(gNormal, vTexCoord);
            vec3 albedo = texture(gAlbedo, vTexCoord).rgb;

            vec3 worldPos = posData.xyz;
            float metallic = posData.a;
            vec3 N = normalize(normData.xyz);
            float roughness = normData.a;

            // Skip empty pixels
            if (length(normData.xyz) < 0.01) discard;

            vec3 V = normalize(uViewPos - worldPos);
            vec3 L = normalize(-uLightDir);
            vec3 H = normalize(V + L);

            vec3 F0 = mix(vec3(0.04), albedo, metallic);

            // Cook-Torrance BRDF
            float NDF = DistributionGGX(N, H, roughness);
            float G = GeometrySmith(N, V, L, roughness);
            vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

            vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);

            float NdotL = max(dot(N, L), 0.0);
            vec3 numerator = NDF * G * F;
            float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001;
            vec3 specular = numerator / denominator;

            vec3 Lo = (kD * albedo / PI + specular) * uLightColor * NdotL;
            vec3 ambient = uAmbientIntensity * albedo;

            vec3 color = ambient + Lo;

            // Tone mapping (ACES)
            color = color / (color + vec3(1.0));
            // Gamma correction
            color = pow(color, vec3(1.0/2.2));

            FragColor = vec4(color, 1.0);
        }
    )";
};

} // namespace renderer
} // namespace engine
