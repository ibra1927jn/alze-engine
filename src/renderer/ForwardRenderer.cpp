// ForwardRenderer.cpp — Implementaciones de ForwardRenderer y UniformCache
#include "ForwardRenderer.h"
#include "ShaderLibrary.h"
#include <cstdio>
#include <algorithm>
#include <cmath>

namespace engine {
namespace renderer {

// ── UniformCache::resolve ────────────────────────────────────────────────────
void UniformCache::resolve(GLuint phongHandle, GLuint depthHandle) {
    auto loc = [](GLuint h, const char* n) { return glGetUniformLocation(h, n); };

    view                = loc(phongHandle, "uView");
    projection          = loc(phongHandle, "uProjection");
    viewPos             = loc(phongHandle, "uViewPos");
    dirLightDir         = loc(phongHandle, "uDirLightDir");
    dirLightColor       = loc(phongHandle, "uDirLightColor");
    skyColor            = loc(phongHandle, "uSkyColor");
    groundColor         = loc(phongHandle, "uGroundColor");
    ambientIntensity    = loc(phongHandle, "uAmbientIntensity");
    lightSpaceMatrix[0] = loc(phongHandle, "uLightSpaceMatrix[0]");
    lightSpaceMatrix[1] = loc(phongHandle, "uLightSpaceMatrix[1]");
    shadowEnabled       = loc(phongHandle, "uShadowEnabled");
    shadowMap[0]        = loc(phongHandle, "uShadowMap[0]");
    shadowMap[1]        = loc(phongHandle, "uShadowMap[1]");
    cascadeSplit        = loc(phongHandle, "uCascadeSplit");
    iblEnabled          = loc(phongHandle, "uIBLEnabled");
    irradianceMap       = loc(phongHandle, "uIrradianceMap");
    prefilterMap        = loc(phongHandle, "uPrefilterMap");
    brdfLUT             = loc(phongHandle, "uBRDFLUT");
    iblIntensity        = loc(phongHandle, "uIBLIntensity");
    numPointLights      = loc(phongHandle, "uNumPointLights");
    numSpotLights       = loc(phongHandle, "uNumSpotLights");
    volumetricIntensity = loc(phongHandle, "uVolumetricIntensity");
    fogDensity          = loc(phongHandle, "uFogDensity");
    renderMode          = loc(phongHandle, "uRenderMode");
    model               = loc(phongHandle, "uModel");
    normalMatrix        = loc(phongHandle, "uNormalMatrix");
    matAlbedo           = loc(phongHandle, "uAlbedo");
    matMetallic         = loc(phongHandle, "uMetallic");
    matRoughness        = loc(phongHandle, "uRoughness");
    matAo               = loc(phongHandle, "uAo");
    matAlbedoTex        = loc(phongHandle, "uAlbedoTex");
    matUseAlbedoTex     = loc(phongHandle, "uUseAlbedoTex");
    matNormalMap        = loc(phongHandle, "uNormalMap");
    matUseNormalMap     = loc(phongHandle, "uUseNormalMap");
    matMRTex            = loc(phongHandle, "uMetallicRoughnessTex");
    matUseMRTex         = loc(phongHandle, "uUseMetallicRoughnessTex");
    matEmissiveColor    = loc(phongHandle, "uEmissiveColor");
    matEmissiveIntensity = loc(phongHandle, "uEmissiveIntensity");
    matEmissiveTex      = loc(phongHandle, "uEmissiveTex");
    matUseEmissiveTex   = loc(phongHandle, "uUseEmissiveTex");
    matAoTex            = loc(phongHandle, "uAOTex");
    matUseAoTex         = loc(phongHandle, "uUseAOTex");
    matHeightMap        = loc(phongHandle, "uHeightMap");
    matUseHeightMap     = loc(phongHandle, "uUseHeightMap");
    matParallaxScale    = loc(phongHandle, "uParallaxScale");

    char buf[64];
    for (int i = 0; i < 8; i++) {
        snprintf(buf, 64, "uPointLights[%d].position",  i); pointLights[i].position  = loc(phongHandle, buf);
        snprintf(buf, 64, "uPointLights[%d].color",     i); pointLights[i].color     = loc(phongHandle, buf);
        snprintf(buf, 64, "uPointLights[%d].constant",  i); pointLights[i].constant  = loc(phongHandle, buf);
        snprintf(buf, 64, "uPointLights[%d].linear",    i); pointLights[i].linear    = loc(phongHandle, buf);
        snprintf(buf, 64, "uPointLights[%d].quadratic", i); pointLights[i].quadratic = loc(phongHandle, buf);
    }
    for (int i = 0; i < 4; i++) {
        snprintf(buf, 64, "uSpotLights[%d].position",    i); spotLights[i].position    = loc(phongHandle, buf);
        snprintf(buf, 64, "uSpotLights[%d].direction",   i); spotLights[i].direction   = loc(phongHandle, buf);
        snprintf(buf, 64, "uSpotLights[%d].color",       i); spotLights[i].color       = loc(phongHandle, buf);
        snprintf(buf, 64, "uSpotLights[%d].cutOff",      i); spotLights[i].cutOff      = loc(phongHandle, buf);
        snprintf(buf, 64, "uSpotLights[%d].outerCutOff", i); spotLights[i].outerCutOff = loc(phongHandle, buf);
        snprintf(buf, 64, "uSpotLights[%d].constant",    i); spotLights[i].constant    = loc(phongHandle, buf);
        snprintf(buf, 64, "uSpotLights[%d].linear",      i); spotLights[i].linear      = loc(phongHandle, buf);
        snprintf(buf, 64, "uSpotLights[%d].quadratic",   i); spotLights[i].quadratic   = loc(phongHandle, buf);
    }
    depthLightSpace = loc(depthHandle, "uLightSpaceMatrix");
    depthModel      = loc(depthHandle, "uModel");
}

// ── ForwardRenderer::init ────────────────────────────────────────────────────
bool ForwardRenderer::init(int viewportWidth, int viewportHeight) {
    m_viewW = viewportWidth;
    m_viewH = viewportHeight;
    // Cargar shaders desde archivos .glsl (fallback a embebidos si no existen)
    ShaderLibrary::init();
    m_phongShader.compile(ShaderLibrary::PBR_VERT, ShaderLibrary::PBR_FRAG);
    m_depthShader.compile(ShaderLibrary::DEPTH_VERT, ShaderLibrary::DEPTH_FRAG);
    m_shadowMap.init(m_settings.shadowResolution);
    m_uniforms.resolve(m_phongShader.getHandle(), m_depthShader.getHandle());
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    return true;
}

// ── ForwardRenderer::end ─────────────────────────────────────────────────────
void ForwardRenderer::end() {
    m_stats.totalObjects = static_cast<int>(m_renderQueue.size());
    if (m_settings.frustumCulling) cullInvisibleObjects();
    if (m_settings.sortObjects && m_renderQueue.size() > 1) {
        sortByMaterial();  // Primary: group by material to minimize state changes
        // sortFrontToBack is now integrated as quaternary sort key within sortByMaterial
    }

    math::Matrix4x4 lightSpaceMatrices[2];
    lightSpaceMatrices[0] = math::Matrix4x4::identity();
    lightSpaceMatrices[1] = math::Matrix4x4::identity();
    if (m_settings.shadows) {
        calcCascadeLightSpaceMatrices(lightSpaceMatrices);
        renderShadowPass(lightSpaceMatrices);
    }
    renderScenePass(lightSpaceMatrices);
}

// ── ForwardRenderer private methods ─────────────────────────────────────────

void ForwardRenderer::cullInvisibleObjects() {
    auto it = std::remove_if(m_renderQueue.begin(), m_renderQueue.end(),
        [this](const RenderItem3D& item) -> bool {
            if (!item.mesh || !item.mesh->isValid()) return true;
            math::Vector3D center(item.modelMatrix.get(0,3), item.modelMatrix.get(1,3), item.modelMatrix.get(2,3));
            float sx2 = item.modelMatrix.get(0,0)*item.modelMatrix.get(0,0) + item.modelMatrix.get(1,0)*item.modelMatrix.get(1,0) + item.modelMatrix.get(2,0)*item.modelMatrix.get(2,0);
            float sy2 = item.modelMatrix.get(0,1)*item.modelMatrix.get(0,1) + item.modelMatrix.get(1,1)*item.modelMatrix.get(1,1) + item.modelMatrix.get(2,1)*item.modelMatrix.get(2,1);
            float sz2 = item.modelMatrix.get(0,2)*item.modelMatrix.get(0,2) + item.modelMatrix.get(1,2)*item.modelMatrix.get(1,2) + item.modelMatrix.get(2,2)*item.modelMatrix.get(2,2);
            float maxScale = std::sqrt(std::max({sx2, sy2, sz2}));
            float radius = maxScale * 1.75f;
            return !m_frustum.testSphere(center, radius);
        }
    );
    int culled = static_cast<int>(std::distance(it, m_renderQueue.end()));
    m_renderQueue.erase(it, m_renderQueue.end());
    m_stats.culledObjects = culled;
}

void ForwardRenderer::sortFrontToBack() {
    std::sort(m_renderQueue.begin(), m_renderQueue.end(),
        [this](const RenderItem3D& a, const RenderItem3D& b) -> bool {
            if (a.mesh != b.mesh) {
                math::Vector3D posA(a.modelMatrix.get(0,3), a.modelMatrix.get(1,3), a.modelMatrix.get(2,3));
                math::Vector3D posB(b.modelMatrix.get(0,3), b.modelMatrix.get(1,3), b.modelMatrix.get(2,3));
                math::Vector3D da = posA - m_viewPos;
                math::Vector3D db = posB - m_viewPos;
                return da.sqrMagnitude() < db.sqrMagnitude();
            }
            return a.material.metallic > b.material.metallic;
        }
    );
}

void ForwardRenderer::sortByMaterial() {
    // Sort by: texture handle → albedo color → metallic → distance
    // This groups same-material objects together, minimizing GPU state changes
    std::sort(m_renderQueue.begin(), m_renderQueue.end(),
        [this](const RenderItem3D& a, const RenderItem3D& b) -> bool {
            // Primary sort: by albedo texture (same texture = no rebind)
            uintptr_t texA = a.material.albedoTexture ? reinterpret_cast<uintptr_t>(a.material.albedoTexture.get()) : 0;
            uintptr_t texB = b.material.albedoTexture ? reinterpret_cast<uintptr_t>(b.material.albedoTexture.get()) : 0;
            if (texA != texB) return texA < texB;

            // Secondary: by normal map
            uintptr_t nmA = a.material.normalMap ? reinterpret_cast<uintptr_t>(a.material.normalMap.get()) : 0;
            uintptr_t nmB = b.material.normalMap ? reinterpret_cast<uintptr_t>(b.material.normalMap.get()) : 0;
            if (nmA != nmB) return nmA < nmB;

            // Tertiary: by PBR parameters (group similar materials)
            if (a.material.metallic != b.material.metallic) return a.material.metallic < b.material.metallic;
            if (a.material.roughness != b.material.roughness) return a.material.roughness < b.material.roughness;

            // Quaternary: front-to-back for early-Z
            math::Vector3D posA(a.modelMatrix.get(0,3), a.modelMatrix.get(1,3), a.modelMatrix.get(2,3));
            math::Vector3D posB(b.modelMatrix.get(0,3), b.modelMatrix.get(1,3), b.modelMatrix.get(2,3));
            return (posA - m_viewPos).sqrMagnitude() < (posB - m_viewPos).sqrMagnitude();
        }
    );

    // Count material batches
    int batches = m_renderQueue.empty() ? 0 : 1;
    for (size_t i = 1; i < m_renderQueue.size(); i++) {
        const auto& a = m_renderQueue[i-1].material;
        const auto& b = m_renderQueue[i].material;
        if (a.albedoTexture != b.albedoTexture ||
            a.normalMap != b.normalMap ||
            a.metallic != b.metallic ||
            a.roughness != b.roughness) {
            batches++;
        }
    }
    m_stats.materialBatches = batches;
}

void ForwardRenderer::calcCascadeLightSpaceMatrices(math::Matrix4x4 out[2]) {
    math::Vector3D lightDir = m_dirLight.direction.normalized();
    float totalDist = m_settings.shadowDistance;
    float cascadeSplits[2] = { totalDist * 0.4f, totalDist };
    m_cascadeSplitDepth = cascadeSplits[0];

    for (int c = 0; c < 2; c++) {
        float cascDist = cascadeSplits[c];
        math::Vector3D lightPos = m_viewPos - lightDir * cascDist;
        math::Matrix4x4 lightView = math::Matrix4x4::lookAt(lightPos, m_viewPos, math::Vector3D::Up);
        math::Matrix4x4 lightProj = math::Matrix4x4::orthographic(-cascDist, cascDist, -cascDist, cascDist, 0.1f, cascDist * 3.0f);
        out[c] = lightProj * lightView;

        float shadowRes = static_cast<float>(m_shadowMap.getResolution());
        float texelSize = (cascDist * 2.0f) / shadowRes;
        float ox = out[c].get(0, 3);
        float oy = out[c].get(1, 3);
        ox = std::floor(ox / texelSize) * texelSize;
        oy = std::floor(oy / texelSize) * texelSize;
        out[c].set(0, 3, ox);
        out[c].set(1, 3, oy);
    }
}

void ForwardRenderer::renderShadowPass(const math::Matrix4x4 lightSpaceMatrices[2]) {
    float totalDist = m_settings.shadowDistance;
    float cascadeDists[2] = { totalDist * 0.4f, totalDist };

    for (int c = 0; c < 2; c++) {
        m_shadowMap.beginPass(c);
        m_depthShader.use();
        glUniformMatrix4fv(m_uniforms.depthLightSpace, 1, GL_FALSE, lightSpaceMatrices[c].data());

        float maxDist  = cascadeDists[c] * 2.0f;
        float maxDist2 = maxDist * maxDist;

        for (const auto& item : m_renderQueue) {
            if (!item.mesh || !item.mesh->isValid()) continue;
            math::Vector3D objPos(item.modelMatrix.get(0,3), item.modelMatrix.get(1,3), item.modelMatrix.get(2,3));
            math::Vector3D diff = objPos - m_viewPos;
            if (diff.dot(diff) > maxDist2) { m_stats.shadowCulledObjects++; continue; }
            glUniformMatrix4fv(m_uniforms.depthModel, 1, GL_FALSE, item.modelMatrix.data());
            item.mesh->draw();
        }
        m_depthShader.unbind();
        m_shadowMap.endPass(m_viewW, m_viewH);
    }
}

void ForwardRenderer::setNormalMatrix(const math::Matrix4x4& model) {
    float sx2 = model.get(0,0)*model.get(0,0) + model.get(1,0)*model.get(1,0) + model.get(2,0)*model.get(2,0);
    float sy2 = model.get(0,1)*model.get(0,1) + model.get(1,1)*model.get(1,1) + model.get(2,1)*model.get(2,1);
    float sz2 = model.get(0,2)*model.get(0,2) + model.get(1,2)*model.get(1,2) + model.get(2,2)*model.get(2,2);
    float nm[9];
    float avgS2 = (sx2 + sy2 + sz2) / 3.0f;
    bool uniformScale = (std::abs(sx2 - avgS2) < avgS2 * 0.05f) &&
                        (std::abs(sy2 - avgS2) < avgS2 * 0.05f) &&
                        (std::abs(sz2 - avgS2) < avgS2 * 0.05f);
    if (uniformScale) {
        float invS = 1.0f / std::sqrt(sx2);
        nm[0] = model.get(0,0)*invS; nm[1] = model.get(0,1)*invS; nm[2] = model.get(0,2)*invS;
        nm[3] = model.get(1,0)*invS; nm[4] = model.get(1,1)*invS; nm[5] = model.get(1,2)*invS;
        nm[6] = model.get(2,0)*invS; nm[7] = model.get(2,1)*invS; nm[8] = model.get(2,2)*invS;
    } else {
        math::Matrix4x4 normalMat4 = model.inverse().transposed();
        nm[0] = normalMat4.get(0,0); nm[1] = normalMat4.get(0,1); nm[2] = normalMat4.get(0,2);
        nm[3] = normalMat4.get(1,0); nm[4] = normalMat4.get(1,1); nm[5] = normalMat4.get(1,2);
        nm[6] = normalMat4.get(2,0); nm[7] = normalMat4.get(2,1); nm[8] = normalMat4.get(2,2);
    }
    glUniformMatrix3fv(m_uniforms.normalMatrix, 1, GL_TRUE, nm);
}

void ForwardRenderer::renderScenePass(const math::Matrix4x4 lightSpaceMatrices[2]) {
    m_phongShader.use();

    glUniformMatrix4fv(m_uniforms.view, 1, GL_FALSE, m_view.data());
    glUniformMatrix4fv(m_uniforms.projection, 1, GL_FALSE, m_projection.data());
    glUniform3f(m_uniforms.viewPos, m_viewPos.x, m_viewPos.y, m_viewPos.z);

    math::Vector3D lightDir = (math::Vector3D::Zero - m_dirLight.direction).normalized();
    glUniform3f(m_uniforms.dirLightDir, lightDir.x, lightDir.y, lightDir.z);
    glUniform3f(m_uniforms.dirLightColor, m_dirLight.color.x, m_dirLight.color.y, m_dirLight.color.z);
    glUniform3f(m_uniforms.skyColor, m_dirLight.skyColor.x, m_dirLight.skyColor.y, m_dirLight.skyColor.z);
    glUniform3f(m_uniforms.groundColor, m_dirLight.groundColor.x, m_dirLight.groundColor.y, m_dirLight.groundColor.z);
    glUniform1f(m_uniforms.ambientIntensity, m_dirLight.ambientIntensity);

    glUniformMatrix4fv(m_uniforms.lightSpaceMatrix[0], 1, GL_FALSE, lightSpaceMatrices[0].data());
    glUniformMatrix4fv(m_uniforms.lightSpaceMatrix[1], 1, GL_FALSE, lightSpaceMatrices[1].data());
    glUniform1i(m_uniforms.shadowEnabled, m_settings.shadows ? 1 : 0);
    if (m_settings.shadows) {
        // Slots 10-11: shadow maps (antes 7-8, movidos para evitar colision con IBL)
        m_shadowMap.bindTexture(0, 10);
        glUniform1i(m_uniforms.shadowMap[0], 10);
        m_shadowMap.bindTexture(1, 11);
        glUniform1i(m_uniforms.shadowMap[1], 11);
        glUniform1f(m_uniforms.cascadeSplit, m_cascadeSplitDepth);
    }
    if (m_uniforms.volumetricIntensity >= 0) glUniform1f(m_uniforms.volumetricIntensity, m_settings.volumetricIntensity);
    if (m_uniforms.fogDensity >= 0)          glUniform1f(m_uniforms.fogDensity, m_settings.fogDensity);
    if (m_uniforms.renderMode >= 0)          glUniform1i(m_uniforms.renderMode, m_settings.renderMode);

    if (m_envMap && m_envMap->isValid()) {
        // Slots 7-9: IBL (antes 4-6, colisionaban con material emissive/AO/height)
        m_envMap->bind(7, 8, 9);
        glUniform1i(m_uniforms.irradianceMap, 7);
        glUniform1i(m_uniforms.prefilterMap, 8);
        glUniform1i(m_uniforms.brdfLUT, 9);
        glUniform1i(m_uniforms.iblEnabled, 1);
        if (m_uniforms.iblIntensity >= 0) glUniform1f(m_uniforms.iblIntensity, m_settings.iblIntensity);
    } else {
        glUniform1i(m_uniforms.iblEnabled, 0);
    }

    glUniform1i(m_uniforms.numPointLights, m_numPointLights);
    for (int i = 0; i < m_numPointLights; i++) {
        const auto& pl  = m_pointLights[i];
        const auto& loc = m_uniforms.pointLights[i];
        glUniform3f(loc.position, pl.position.x, pl.position.y, pl.position.z);
        glUniform3f(loc.color, pl.color.x, pl.color.y, pl.color.z);
        glUniform1f(loc.constant, pl.constant);
        glUniform1f(loc.linear, pl.linear);
        glUniform1f(loc.quadratic, pl.quadratic);
    }
    glUniform1i(m_uniforms.numSpotLights, m_numSpotLights);
    for (int i = 0; i < m_numSpotLights; i++) {
        const auto& sl  = m_spotLights[i];
        const auto& loc = m_uniforms.spotLights[i];
        glUniform3f(loc.position, sl.position.x, sl.position.y, sl.position.z);
        glUniform3f(loc.direction, sl.direction.x, sl.direction.y, sl.direction.z);
        glUniform3f(loc.color, sl.color.x, sl.color.y, sl.color.z);
        glUniform1f(loc.cutOff, sl.cutOff);
        glUniform1f(loc.outerCutOff, sl.outerCutOff);
        glUniform1f(loc.constant, sl.constant);
        glUniform1f(loc.linear, sl.linear);
        glUniform1f(loc.quadratic, sl.quadratic);
    }

    const Material* lastMat = nullptr;
    for (const auto& item : m_renderQueue) {
        if (!item.mesh || !item.mesh->isValid()) continue;
        glUniformMatrix4fv(m_uniforms.model, 1, GL_FALSE, item.modelMatrix.data());
        setNormalMatrix(item.modelMatrix);

        const auto& mat = item.material;
        // Comparar por valor en vez de por puntero (las copias en RenderItem3D tienen direcciones distintas)
        const bool sameMat = (lastMat && *lastMat == mat);
        if (!sameMat) {
            glUniform3f(m_uniforms.matAlbedo, mat.albedoColor.x, mat.albedoColor.y, mat.albedoColor.z);
            glUniform1f(m_uniforms.matMetallic, mat.metallic);
            glUniform1f(m_uniforms.matRoughness, mat.roughness);
            glUniform1f(m_uniforms.matAo, mat.ao);

            if (mat.albedoTexture && mat.albedoTexture->isValid()) { mat.albedoTexture->bind(0); glUniform1i(m_uniforms.matAlbedoTex, 0); glUniform1i(m_uniforms.matUseAlbedoTex, 1); }
            else glUniform1i(m_uniforms.matUseAlbedoTex, 0);
            if (mat.normalMap && mat.normalMap->isValid()) { mat.normalMap->bind(1); glUniform1i(m_uniforms.matNormalMap, 1); glUniform1i(m_uniforms.matUseNormalMap, 1); }
            else glUniform1i(m_uniforms.matUseNormalMap, 0);
            if (mat.metallicRoughnessMap && mat.metallicRoughnessMap->isValid()) { mat.metallicRoughnessMap->bind(3); glUniform1i(m_uniforms.matMRTex, 3); glUniform1i(m_uniforms.matUseMRTex, 1); }
            else glUniform1i(m_uniforms.matUseMRTex, 0);

            glUniform3f(m_uniforms.matEmissiveColor, mat.emissiveColor.x, mat.emissiveColor.y, mat.emissiveColor.z);
            glUniform1f(m_uniforms.matEmissiveIntensity, mat.emissiveIntensity);
            if (mat.emissiveTexture && mat.emissiveTexture->isValid()) { mat.emissiveTexture->bind(4); glUniform1i(m_uniforms.matEmissiveTex, 4); glUniform1i(m_uniforms.matUseEmissiveTex, 1); }
            else glUniform1i(m_uniforms.matUseEmissiveTex, 0);
            if (mat.aoTexture && mat.aoTexture->isValid()) { mat.aoTexture->bind(5); glUniform1i(m_uniforms.matAoTex, 5); glUniform1i(m_uniforms.matUseAoTex, 1); }
            else glUniform1i(m_uniforms.matUseAoTex, 0);
            if (mat.heightMap && mat.heightMap->isValid()) { mat.heightMap->bind(6); glUniform1i(m_uniforms.matHeightMap, 6); glUniform1i(m_uniforms.matUseHeightMap, 1); glUniform1f(m_uniforms.matParallaxScale, mat.parallaxScale); }
            else glUniform1i(m_uniforms.matUseHeightMap, 0);

            lastMat = &mat;
        }
        item.mesh->draw();
        m_stats.drawCalls++;
        m_stats.totalTriangles += item.mesh->getIndexCount() / 3;
    }
    m_phongShader.unbind();
}

} // namespace renderer
} // namespace engine
