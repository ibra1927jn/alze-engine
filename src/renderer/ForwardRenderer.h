#pragma once

#include <glad/gl.h>
#include "ShaderProgram.h"
#include "ShaderLibrary.h"
#include "ShadowMap.h"
#include "EnvironmentMap.h"
#include "MeshPrimitives.h"
#include "Material.h"
#include "Texture2D.h"
#include "math/Matrix4x4.h"
#include <vector>
#include <cmath>

namespace engine {
namespace renderer {

// ── Light Types ────────────────────────────────────────────────

/// Directional light (sun)
struct DirectionalLight {
    math::Vector3D direction = math::Vector3D(0.3f, -1.0f, -0.5f).normalized();
    math::Vector3D color     = math::Vector3D(1.0f, 0.98f, 0.95f);
    math::Vector3D skyColor  = math::Vector3D(0.15f, 0.20f, 0.35f);   // Hemisphere sky
    math::Vector3D groundColor = math::Vector3D(0.07f, 0.05f, 0.04f); // Hemisphere ground
    float ambientIntensity   = 0.4f;
};

/// Point light (omni-directional)
struct PointLight {
    math::Vector3D position = math::Vector3D::Zero;
    math::Vector3D color    = math::Vector3D(1, 1, 1);
    float constant  = 1.0f;
    float linear    = 0.09f;
    float quadratic = 0.032f;
};

/// Spot light
struct SpotLight {
    math::Vector3D position  = math::Vector3D::Zero;
    math::Vector3D direction = math::Vector3D(0, -1, 0);
    math::Vector3D color     = math::Vector3D(1.0f, 1.0f, 1.0f);
    float cutOff             = std::cos(0.2182f);   // cos(12.5°)
    float outerCutOff        = std::cos(0.3054f);   // cos(17.5°)
    float constant           = 1.0f;
    float linear             = 0.09f;
    float quadratic          = 0.032f;
};

/// RenderItem3D — Un objeto a dibujar.
struct RenderItem3D {
    const Mesh3D*   mesh     = nullptr;
    Material        material;
    math::Matrix4x4 modelMatrix = math::Matrix4x4::identity();
};

// ── Render Settings ────────────────────────────────────────────
struct RenderSettings {
    bool frustumCulling   = true;
    bool shadows          = true;
    bool sortObjects      = true;
    int  shadowResolution = 2048;  // High-res shadows for PCSS
    float shadowDistance   = 50.0f;
    float iblIntensity    = 1.0f;
    float volumetricIntensity = 0.0f; // Strength of God Rays (0 = off)
    float fogDensity = 0.0f;          // 0 = off, 0.0005 = subtle, 0.002 = thick
    int   renderMode = 0;             // 0=PBR, 1=Toon, 2=Neon
};

// ── Render Statistics ──────────────────────────────────────────
struct RenderStats {
    int drawCalls          = 0;
    int culledObjects      = 0;
    int shadowCulledObjects = 0;
    int totalObjects       = 0;
    int totalTriangles     = 0;
    int materialBatches    = 0;   // Number of unique material groups
};

// ── Frustum Planes ─────────────────────────────────────────────
struct FrustumPlane {
    float a, b, c, d;  // ax + by + cz + d = 0

    float distanceToPoint(const math::Vector3D& p) const {
        return a * p.x + b * p.y + c * p.z + d;
    }
};

struct Frustum {
    FrustumPlane planes[6];  // left, right, bottom, top, near, far

    /// Extract 6 frustum planes from a VP matrix (Gribb-Hartmann method)
    void extractFromVP(const math::Matrix4x4& vp) {
        // Extract rows of the VP matrix
        float r0[4], r1[4], r2[4], r3[4];
        for (int c = 0; c < 4; c++) {
            r0[c] = vp.get(0, c);
            r1[c] = vp.get(1, c);
            r2[c] = vp.get(2, c);
            r3[c] = vp.get(3, c);
        }

        // Left:   row3 + row0
        planes[0] = { r3[0]+r0[0], r3[1]+r0[1], r3[2]+r0[2], r3[3]+r0[3] };
        // Right:  row3 - row0
        planes[1] = { r3[0]-r0[0], r3[1]-r0[1], r3[2]-r0[2], r3[3]-r0[3] };
        // Bottom: row3 + row1
        planes[2] = { r3[0]+r1[0], r3[1]+r1[1], r3[2]+r1[2], r3[3]+r1[3] };
        // Top:    row3 - row1
        planes[3] = { r3[0]-r1[0], r3[1]-r1[1], r3[2]-r1[2], r3[3]-r1[3] };
        // Near:   row3 + row2
        planes[4] = { r3[0]+r2[0], r3[1]+r2[1], r3[2]+r2[2], r3[3]+r2[3] };
        // Far:    row3 - row2
        planes[5] = { r3[0]-r2[0], r3[1]-r2[1], r3[2]-r2[2], r3[3]-r2[3] };

        // Normalize planes
        for (auto& p : planes) {
            float len = std::sqrt(p.a*p.a + p.b*p.b + p.c*p.c);
            if (len > 0.0001f) {
                float inv = 1.0f / len;
                p.a *= inv; p.b *= inv; p.c *= inv; p.d *= inv;
            }
        }
    }

    /// Test if a sphere is visible (returns true if ANY part is visible)
    bool testSphere(const math::Vector3D& center, float radius) const {
        for (const auto& p : planes) {
            if (p.distanceToPoint(center) < -radius) return false;
        }
        return true;
    }

    /// Test if an AABB is visible (proper box-plane test)
    bool testAABB(const math::Vector3D& mn, const math::Vector3D& mx) const {
        for (const auto& p : planes) {
            // Find the positive vertex (the one most in the direction of the plane normal)
            math::Vector3D pVertex(
                p.a >= 0 ? mx.x : mn.x,
                p.b >= 0 ? mx.y : mn.y,
                p.c >= 0 ? mx.z : mn.z
            );
            // If the positive vertex is behind the plane, the AABB is fully outside
            if (p.distanceToPoint(pVertex) < 0.0f) return false;
        }
        return true;
    }
};

// ── Cached Uniform Locations ───────────────────────────────────
struct UniformCache {
    // Per-frame
    GLint view = -1, projection = -1, viewPos = -1;
    GLint dirLightDir = -1, dirLightColor = -1;
    GLint skyColor = -1, groundColor = -1, ambientIntensity = -1;
    GLint lightSpaceMatrix[2] = {-1, -1};
    GLint shadowEnabled = -1;
    GLint shadowMap[2] = {-1, -1};
    GLint cascadeSplit = -1;
    GLint iblEnabled = -1, irradianceMap = -1, prefilterMap = -1, brdfLUT = -1;
    GLint iblIntensity = -1;
    GLint numPointLights = -1, numSpotLights = -1;
    GLint volumetricIntensity = -1;
    GLint fogDensity = -1;
    GLint renderMode = -1;

    // Per-object
    GLint model = -1, normalMatrix = -1;

    // Material (per-object)
    GLint matAlbedo = -1, matMetallic = -1, matRoughness = -1, matAo = -1;
    GLint matAlbedoTex = -1, matUseAlbedoTex = -1;
    GLint matNormalMap = -1, matUseNormalMap = -1;
    GLint matMRTex = -1, matUseMRTex = -1;       // Metallic/Roughness map
    GLint matEmissiveColor = -1, matEmissiveIntensity = -1;
    GLint matEmissiveTex = -1, matUseEmissiveTex = -1;
    GLint matAoTex = -1, matUseAoTex = -1;
    GLint matHeightMap = -1, matUseHeightMap = -1, matParallaxScale = -1;

    // Point lights [8]
    struct PointLightLoc {
        GLint position = -1, color = -1, constant = -1, linear = -1, quadratic = -1;
    } pointLights[8];

    // Spot lights [4]
    struct SpotLightLoc {
        GLint position = -1, direction = -1, color = -1;
        GLint cutOff = -1, outerCutOff = -1;
        GLint constant = -1, linear = -1, quadratic = -1;
    } spotLights[4];

    // Depth shader
    GLint depthLightSpace = -1, depthModel = -1;

    void resolve(GLuint phongHandle, GLuint depthHandle);  // impl in ForwardRenderer.cpp
};


/// ForwardRenderer v3 — Optimized PBR + Shadows + Multi-Light.
///
/// Improvements over v2:
///   - Cached uniform locations (zero snprintf/glGetUniformLocation per frame)
///   - Frustum culling (Gribb-Hartmann plane extraction)
///   - Front-to-back sorting (early-Z rejection)
///   - Optimized normal matrix (skip inverse for uniform scale)
///   - Configurable RenderSettings + RenderStats
///
class ForwardRenderer {
public:
    ForwardRenderer() = default;

    bool init(int viewportWidth, int viewportHeight);  // impl in ForwardRenderer.cpp

    void resize(int width, int height) {
        m_viewW = width;
        m_viewH = height;
        glViewport(0, 0, width, height);
    }

    // ── Settings ───────────────────────────────────────────────
    void setSettings(const RenderSettings& s) { m_settings = s; }
    RenderSettings& getSettings() { return m_settings; }
    const RenderStats& getStats() const { return m_stats; }

    // ── Light Configuration ────────────────────────────────────
    void setDirectionalLight(const DirectionalLight& light) { m_dirLight = light; }

    void addPointLight(const PointLight& light) {
        if (m_numPointLights < 8)
            m_pointLights[m_numPointLights++] = light;
    }

    void addSpotLight(const SpotLight& light) {
        if (m_numSpotLights < 4)
            m_spotLights[m_numSpotLights++] = light;
    }

    void clearLights() {
        m_numPointLights = 0;
        m_numSpotLights = 0;
    }

    void setShadowsEnabled(bool enabled) { m_settings.shadows = enabled; }
    void setEnvironmentMap(const EnvironmentMap* env) { m_envMap = env; }

    // ── Rendering ──────────────────────────────────────────────

    void begin(const math::Matrix4x4& view, const math::Matrix4x4& projection) {
        m_view = view;
        m_projection = projection;

        math::Matrix4x4 viewInv = view.inverse();
        m_viewPos = math::Vector3D(viewInv.get(0,3), viewInv.get(1,3), viewInv.get(2,3));

        // Extract frustum planes from VP matrix
        if (m_settings.frustumCulling) {
            math::Matrix4x4 vp = projection * view;
            m_frustum.extractFromVP(vp);
        }

        m_renderQueue.clear();
        // Keep reserved capacity across frames (zero realloc after warmup)
        if (m_renderQueue.capacity() < 256) m_renderQueue.reserve(256);
        m_stats = {};
    }

    void submit(const Mesh3D& mesh, const Material& material,
                const math::Matrix4x4& modelMatrix = math::Matrix4x4::identity()) {
        m_renderQueue.push_back({&mesh, material, modelMatrix});
    }

    void submit(const RenderItem3D& item) {
        m_renderQueue.push_back(item);
    }

    void end();  // impl in ForwardRenderer.cpp

    int getDrawCalls() const { return m_stats.drawCalls; }

    ShaderProgram& getPhongShader()  { return m_phongShader; }
    ShadowMap& getShadowMap()        { return m_shadowMap; }

private:
    // impl in ForwardRenderer.cpp
    void cullInvisibleObjects();
    void sortByMaterial();  // Group by shader→material to minimize state changes
    void calcCascadeLightSpaceMatrices(math::Matrix4x4 out[2]);
    void renderShadowPass(const math::Matrix4x4 lightSpaceMatrices[2]);
    void setNormalMatrix(const math::Matrix4x4& model);
    void renderScenePass(const math::Matrix4x4 lightSpaceMatrices[2]);



    // ── Member Data ────────────────────────────────────────────

    // Shaders
    ShaderProgram m_phongShader;
    ShaderProgram m_depthShader;

    // Cached uniform locations
    UniformCache m_uniforms;

    // Shadow
    ShadowMap m_shadowMap;
    float m_cascadeSplitDepth = 10.0f;  // View-space depth for cascade split

    // Camera
    math::Matrix4x4 m_view, m_projection;
    math::Vector3D  m_viewPos;

    // Frustum
    Frustum m_frustum;

    // Lights
    DirectionalLight m_dirLight;
    PointLight  m_pointLights[8];
    SpotLight   m_spotLights[4];
    int m_numPointLights = 0;
    int m_numSpotLights  = 0;

    // Render queue
    std::vector<RenderItem3D> m_renderQueue;  // Pre-reserved, capacity persists across frames
    int m_viewW = 800, m_viewH = 600;

    // Settings & stats
    RenderSettings m_settings;
    RenderStats    m_stats;

    const EnvironmentMap* m_envMap = nullptr;
};

} // namespace renderer
} // namespace engine
