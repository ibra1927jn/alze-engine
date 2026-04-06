// EnvironmentMap.cpp — Implementaciones de EnvironmentMap (IBL pipeline)
#include "EnvironmentMap.h"
#include "ImageDecoder.h"
#include "core/Logger.h"
#include <cstdint>

namespace engine {
namespace renderer {

bool EnvironmentMap::generate(const SkyParams& sky, int envSize, int irrSize, int pfSize) {
    m_envSize = envSize;

    if (!m_captureShader.compile(IBLShaders::CUBEMAP_VERT, IBLShaders::SKY_CAPTURE_FRAG)) return false;
    if (!m_irradianceShader.compile(IBLShaders::CUBEMAP_VERT, IBLShaders::IRRADIANCE_FRAG)) return false;
    if (!m_prefilterShader.compile(IBLShaders::CUBEMAP_VERT, IBLShaders::PREFILTER_FRAG)) return false;
    if (!m_brdfShader.compile(IBLShaders::BRDF_VERT, IBLShaders::BRDF_FRAG)) return false;

    createCube();
    createQuad();

    GLuint captureFBO, captureRBO;
    glGenFramebuffers(1, &captureFBO);
    glGenRenderbuffers(1, &captureRBO);

    float viewMatData[6][16];
    computeCubemapViews(viewMatData);
    math::Matrix4x4 captureProj = math::Matrix4x4::perspective(90.0f * 3.14159f / 180.0f, 1.0f, 0.1f, 10.0f);

    GLint prevFBO, prevViewport[4];
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
    glGetIntegerv(GL_VIEWPORT, prevViewport);

    // 1. Capture skybox → HDR cubemap
    core::Logger::info("IBL", "Capturing environment cubemap...");
    glGenTextures(1, &m_envCubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_envCubemap);
    for (int i = 0; i < 6; i++)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA16F, envSize, envSize, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, envSize, envSize);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);

    m_captureShader.use();
    m_captureShader.setMat4("uProjection", captureProj);
    m_captureShader.setVec3("uSkyColorTop",     sky.topColor);
    m_captureShader.setVec3("uSkyColorHorizon", sky.horizonColor);
    m_captureShader.setVec3("uSkyColorBottom",  sky.bottomColor);
    m_captureShader.setVec3("uSunDir",   sky.sunDir.normalized());
    m_captureShader.setVec3("uSunColor", sky.sunColor);
    m_captureShader.setFloat("uSunSize", sky.sunSize);

    glViewport(0, 0, envSize, envSize);
    for (int i = 0; i < 6; i++) {
        math::Matrix4x4 view = buildView(viewMatData[i]);
        m_captureShader.setMat4("uView", view);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_envCubemap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        drawCube();
    }
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_envCubemap);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    // 2. Irradiance convolution
    core::Logger::info("IBL", "Computing irradiance map...");
    glGenTextures(1, &m_irradianceMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_irradianceMap);
    for (int i = 0; i < 6; i++)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA16F, irrSize, irrSize, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, irrSize, irrSize);
    m_irradianceShader.use();
    m_irradianceShader.setMat4("uProjection", captureProj);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_envCubemap);
    m_irradianceShader.setInt("uEnvironmentMap", 0);

    glViewport(0, 0, irrSize, irrSize);
    for (int i = 0; i < 6; i++) {
        math::Matrix4x4 view = buildView(viewMatData[i]);
        m_irradianceShader.setMat4("uView", view);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_irradianceMap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        drawCube();
    }

    // 3. Pre-filtered environment
    core::Logger::info("IBL", "Pre-filtering environment...");
    glGenTextures(1, &m_prefilterMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_prefilterMap);
    for (int i = 0; i < 6; i++)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA16F, pfSize, pfSize, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    m_prefilterShader.use();
    m_prefilterShader.setMat4("uProjection", captureProj);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_envCubemap);
    m_prefilterShader.setInt("uEnvironmentMap", 0);

    const int maxMipLevels = 5;
    for (int mip = 0; mip < maxMipLevels; mip++) {
        int mipW = static_cast<int>(pfSize * std::pow(0.5, mip));
        glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipW, mipW);
        glViewport(0, 0, mipW, mipW);
        float roughness = (maxMipLevels > 1) ? static_cast<float>(mip) / static_cast<float>(maxMipLevels - 1) : 0.0f;
        m_prefilterShader.setFloat("uRoughness", roughness);
        for (int i = 0; i < 6; i++) {
            math::Matrix4x4 view = buildView(viewMatData[i]);
            m_prefilterShader.setMat4("uView", view);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_prefilterMap, mip);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            drawCube();
        }
    }

    // 4. BRDF LUT
    core::Logger::info("IBL", "Generating BRDF LUT...");
    glGenTextures(1, &m_brdfLUT);
    glBindTexture(GL_TEXTURE_2D, m_brdfLUT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 256, 256, 0, GL_RG, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 256, 256);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_brdfLUT, 0);
    glViewport(0, 0, 256, 256);
    m_brdfShader.use();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    drawQuad();

    glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
    glDeleteRenderbuffers(1, &captureRBO);
    glDeleteFramebuffers(1, &captureFBO);

    core::Logger::info("IBL", "Done! Env=" + std::to_string(envSize) + " Irr=" + std::to_string(irrSize)
              + " PF=" + std::to_string(pfSize) + " BRDF=256");
    return true;
}

bool EnvironmentMap::generateFromHDRI(const char* hdrPath, int envSize, int irrSize, int pfSize) {
    m_envSize = envSize;

    stbi_set_flip_vertically_on_load(1);
    int w, h, channels;
    float* data = stbi_loadf(hdrPath, &w, &h, &channels, 0);
    if (!data) {
        core::Logger::error("IBL", std::string("Failed to load HDRI: ") + hdrPath);
        return false;
    }
    core::Logger::info("IBL", std::string("Loaded HDRI: ") + hdrPath + " (" + std::to_string(w) + "x" + std::to_string(h) + ")");

    GLuint hdrTex;
    glGenTextures(1, &hdrTex);
    glBindTexture(GL_TEXTURE_2D, hdrTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, channels == 4 ? GL_RGBA : GL_RGB, GL_FLOAT, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    stbi_image_free(data);

    ShaderProgram equirectShader;
    if (!equirectShader.compile(IBLShaders::CUBEMAP_VERT, HDRIShaders::EQUIRECT_TO_CUBE_FRAG)) return false;
    if (!m_irradianceShader.compile(IBLShaders::CUBEMAP_VERT, IBLShaders::IRRADIANCE_FRAG)) return false;
    if (!m_prefilterShader.compile(IBLShaders::CUBEMAP_VERT, IBLShaders::PREFILTER_FRAG)) return false;
    if (!m_brdfShader.compile(IBLShaders::BRDF_VERT, IBLShaders::BRDF_FRAG)) return false;

    createCube();
    createQuad();

    GLuint captureFBO, captureRBO;
    glGenFramebuffers(1, &captureFBO);
    glGenRenderbuffers(1, &captureRBO);

    float viewMatData[6][16];
    computeCubemapViews(viewMatData);
    math::Matrix4x4 captureProj = math::Matrix4x4::perspective(90.0f * 3.14159f / 180.0f, 1.0f, 0.1f, 10.0f);

    GLint prevFBO, prevViewport[4];
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
    glGetIntegerv(GL_VIEWPORT, prevViewport);

    // Convert equirectangular → cubemap
    core::Logger::info("IBL", "Converting equirectangular -> cubemap...");
    glGenTextures(1, &m_envCubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_envCubemap);
    for (int i = 0; i < 6; i++)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA16F, envSize, envSize, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, envSize, envSize);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);
    equirectShader.use();
    equirectShader.setMat4("uProjection", captureProj);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrTex);
    equirectShader.setInt("uEquirectangularMap", 0);
    glViewport(0, 0, envSize, envSize);
    for (int i = 0; i < 6; i++) {
        math::Matrix4x4 view = buildView(viewMatData[i]);
        equirectShader.setMat4("uView", view);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_envCubemap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        drawCube();
    }
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_envCubemap);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    glDeleteTextures(1, &hdrTex);

    generateIBLFromCubemap(captureFBO, captureRBO, viewMatData, captureProj, irrSize, pfSize);

    glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
    glDeleteRenderbuffers(1, &captureRBO);
    glDeleteFramebuffers(1, &captureFBO);
    core::Logger::info("IBL", "HDRI pipeline complete!");
    return true;
}

void EnvironmentMap::bind(int irradianceSlot, int prefilterSlot, int brdfSlot) const {
    glActiveTexture(GL_TEXTURE0 + irradianceSlot); glBindTexture(GL_TEXTURE_CUBE_MAP, m_irradianceMap);
    glActiveTexture(GL_TEXTURE0 + prefilterSlot);  glBindTexture(GL_TEXTURE_CUBE_MAP, m_prefilterMap);
    glActiveTexture(GL_TEXTURE0 + brdfSlot);       glBindTexture(GL_TEXTURE_2D,       m_brdfLUT);
}

void EnvironmentMap::destroy() {
    if (m_brdfLUT)       glDeleteTextures(1, &m_brdfLUT);
    if (m_prefilterMap)  glDeleteTextures(1, &m_prefilterMap);
    if (m_irradianceMap) glDeleteTextures(1, &m_irradianceMap);
    if (m_envCubemap)    glDeleteTextures(1, &m_envCubemap);
    if (m_cubeVBO) glDeleteBuffers(1,      &m_cubeVBO);
    if (m_cubeVAO) glDeleteVertexArrays(1, &m_cubeVAO);
    if (m_quadEBO) glDeleteBuffers(1,      &m_quadEBO);
    if (m_quadVBO) glDeleteBuffers(1,      &m_quadVBO);
    if (m_quadVAO) glDeleteVertexArrays(1, &m_quadVAO);
    m_envCubemap = m_irradianceMap = m_prefilterMap = m_brdfLUT = 0;
}

math::Matrix4x4 EnvironmentMap::buildView(const float raw[16]) {
    math::Matrix4x4 mat;
    for (int c = 0; c < 4; c++)
        for (int r = 0; r < 4; r++)
            mat.set(r, c, raw[c * 4 + r]);
    return mat;
}

void EnvironmentMap::generateIBLFromCubemap(GLuint captureFBO, GLuint /*captureRBO*/,
                                             float viewMatData[6][16],
                                             const math::Matrix4x4& captureProj,
                                             int irrSize, int pfSize)
{
    // 2. Irradiance
    core::Logger::info("IBL", "Computing irradiance map...");
    glGenTextures(1, &m_irradianceMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_irradianceMap);
    for (int i = 0; i < 6; i++)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA16F, irrSize, irrSize, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, irrSize, irrSize);
    m_irradianceShader.use();
    m_irradianceShader.setMat4("uProjection", captureProj);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_envCubemap);
    m_irradianceShader.setInt("uEnvironmentMap", 0);
    glViewport(0, 0, irrSize, irrSize);
    for (int i = 0; i < 6; i++) {
        math::Matrix4x4 view = buildView(viewMatData[i]);
        m_irradianceShader.setMat4("uView", view);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_irradianceMap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        drawCube();
    }

    // 3. Pre-filter
    core::Logger::info("IBL", "Pre-filtering environment...");
    glGenTextures(1, &m_prefilterMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_prefilterMap);
    for (int i = 0; i < 6; i++)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA16F, pfSize, pfSize, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    m_prefilterShader.use();
    m_prefilterShader.setMat4("uProjection", captureProj);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_envCubemap);
    m_prefilterShader.setInt("uEnvironmentMap", 0);
    const int maxMipLevels = 5;
    for (int mip = 0; mip < maxMipLevels; mip++) {
        int mipW = static_cast<int>(pfSize * std::pow(0.5, mip));
        glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipW, mipW);
        glViewport(0, 0, mipW, mipW);
        float roughness = (maxMipLevels > 1) ? static_cast<float>(mip) / static_cast<float>(maxMipLevels - 1) : 0.0f;
        m_prefilterShader.setFloat("uRoughness", roughness);
        for (int i = 0; i < 6; i++) {
            math::Matrix4x4 view = buildView(viewMatData[i]);
            m_prefilterShader.setMat4("uView", view);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_prefilterMap, mip);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            drawCube();
        }
    }

    // 4. BRDF LUT
    core::Logger::info("IBL", "Generating BRDF LUT...");
    glGenTextures(1, &m_brdfLUT);
    glBindTexture(GL_TEXTURE_2D, m_brdfLUT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 256, 256, 0, GL_RG, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 256, 256);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_brdfLUT, 0);
    glViewport(0, 0, 256, 256);
    m_brdfShader.use();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    drawQuad();
}

void EnvironmentMap::computeCubemapViews(float out[6][16]) {
    struct LookAt { float ex,ey,ez, cx,cy,cz, ux,uy,uz; };
    LookAt faces[6] = {
        {0,0,0,  1, 0, 0,  0,-1, 0},
        {0,0,0, -1, 0, 0,  0,-1, 0},
        {0,0,0,  0, 1, 0,  0, 0, 1},
        {0,0,0,  0,-1, 0,  0, 0,-1},
        {0,0,0,  0, 0, 1,  0,-1, 0},
        {0,0,0,  0, 0,-1,  0,-1, 0},
    };
    for (int f = 0; f < 6; f++) {
        auto& la = faces[f];
        float fx = la.cx-la.ex, fy = la.cy-la.ey, fz = la.cz-la.ez;
        float flen = std::sqrt(fx*fx+fy*fy+fz*fz);
        if (flen > 1e-6f) { fx/=flen; fy/=flen; fz/=flen; }
        float sx = fy*la.uz-fz*la.uy, sy = fz*la.ux-fx*la.uz, sz = fx*la.uy-fy*la.ux;
        float slen = std::sqrt(sx*sx+sy*sy+sz*sz);
        if (slen > 1e-6f) { sx/=slen; sy/=slen; sz/=slen; }
        float ux = sy*fz-sz*fy, uy = sz*fx-sx*fz, uz = sx*fy-sy*fx;
        out[f][0]=sx;  out[f][4]=sy;  out[f][8]=sz;   out[f][12]=-(sx*la.ex+sy*la.ey+sz*la.ez);
        out[f][1]=ux;  out[f][5]=uy;  out[f][9]=uz;   out[f][13]=-(ux*la.ex+uy*la.ey+uz*la.ez);
        out[f][2]=-fx; out[f][6]=-fy; out[f][10]=-fz; out[f][14]=(fx*la.ex+fy*la.ey+fz*la.ez);
        out[f][3]=0;   out[f][7]=0;   out[f][11]=0;   out[f][15]=1;
    }
}

void EnvironmentMap::createCube() {
    float v[] = {
        -1,-1,-1, 1,-1,-1, 1, 1,-1, 1, 1,-1,-1, 1,-1,-1,-1,-1,
        -1,-1, 1, 1,-1, 1, 1, 1, 1, 1, 1, 1,-1, 1, 1,-1,-1, 1,
        -1, 1, 1,-1, 1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 1,-1, 1, 1,
         1, 1, 1, 1, 1,-1, 1,-1,-1, 1,-1,-1, 1,-1, 1, 1, 1, 1,
        -1,-1,-1, 1,-1,-1, 1,-1, 1, 1,-1, 1,-1,-1, 1,-1,-1,-1,
        -1, 1,-1, 1, 1,-1, 1, 1, 1, 1, 1, 1,-1, 1, 1,-1, 1,-1,
    };
    glGenVertexArrays(1, &m_cubeVAO);
    glGenBuffers(1, &m_cubeVBO);
    glBindVertexArray(m_cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void EnvironmentMap::createQuad() {
    float v[] = { -1,-1,0,0, 1,-1,1,0, 1,1,1,1, -1,1,0,1 };
    uint32_t idx[] = { 0,1,2, 0,2,3 };
    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);
    glGenBuffers(1, &m_quadEBO);
    glBindVertexArray(m_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, reinterpret_cast<void*>(8));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_quadEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);
    glBindVertexArray(0);
}

void EnvironmentMap::drawCube() {
    glBindVertexArray(m_cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

void EnvironmentMap::drawQuad() {
    glBindVertexArray(m_quadVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

} // namespace renderer
} // namespace engine
