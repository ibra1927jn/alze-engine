#include "PostProcess.h"
#include <cstdint>
#include "core/Logger.h"

namespace engine {
namespace renderer {

bool PostProcess::init(int width, int height) {
    m_width = width;
    m_height = height;

    if (!m_brightShader.compile(PostProcessShaders::QUAD_VERT, PostProcessShaders::BRIGHT_FRAG)) return false;
    if (!m_blurShader.compile(PostProcessShaders::QUAD_VERT, PostProcessShaders::BLUR_FRAG)) return false;
    if (!m_compositeShader.compile(PostProcessShaders::QUAD_VERT, PostProcessShaders::COMPOSITE_FRAG)) return false;

    createQuad();
    return createFBOs(width, height);
}

void PostProcess::resize(int width, int height) {
    m_width = width; m_height = height;
    destroyFBOs();
    createFBOs(width, height);
}

void PostProcess::beginScene() {
    glBindFramebuffer(GL_FRAMEBUFFER, m_hdrFBO);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void PostProcess::endScene(const Settings& settings) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    int bloomW = settings.halfResBloom ? m_width / 2 : m_width;
    int bloomH = settings.halfResBloom ? m_height / 2 : m_height;

    // 1. Bright pass
    glViewport(0, 0, bloomW, bloomH);
    glBindFramebuffer(GL_FRAMEBUFFER, m_pingpongFBO[0]);
    glClear(GL_COLOR_BUFFER_BIT);
    m_brightShader.use();
    m_brightShader.setFloat("uThreshold", settings.bloomThreshold);
    m_brightShader.setFloat("uSoftKnee", settings.bloomSoftKnee);
    bindTex(m_hdrColorTex, 0);
    m_brightShader.setInt("uScene", 0);
    drawQuad();

    // 2. Gaussian blur (ping-pong)
    bool horizontal = true;
    m_blurShader.use();
    for (int i = 0; i < settings.bloomPasses * 2; i++) {
        int target = horizontal ? 1 : 0;
        int source = horizontal ? 0 : 1;
        glBindFramebuffer(GL_FRAMEBUFFER, m_pingpongFBO[target]);
        glClear(GL_COLOR_BUFFER_BIT);
        m_blurShader.setInt("uHorizontal", horizontal ? 1 : 0);
        bindTex(m_pingpongTex[source], 0);
        m_blurShader.setInt("uImage", 0);
        drawQuad();
        horizontal = !horizontal;
    }

    // 3. Composite → screen
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_width, m_height);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    m_compositeShader.use();
    bindTex(m_hdrColorTex, 0);
    m_compositeShader.setInt("uScene", 0);
    bindTex(m_pingpongTex[0], 1);
    m_compositeShader.setInt("uBloom", 1);
    m_compositeShader.setFloat("uBloomIntensity", settings.bloomIntensity);
    m_compositeShader.setFloat("uVignetteStrength", settings.vignetteStrength);
    m_compositeShader.setFloat("uExposure", settings.exposure);
    m_compositeShader.setVec2("uResolution", static_cast<float>(m_width), static_cast<float>(m_height));
    m_compositeShader.setInt("uFXAAEnabled", settings.fxaaEnabled ? 1 : 0);
    m_compositeShader.setFloat("uChromaticAberration", settings.chromaticAberration);
    m_compositeShader.setFloat("uFilmGrain", settings.filmGrain);
    m_compositeShader.setFloat("uSharpenStrength", settings.sharpenStrength);
    m_compositeShader.setFloat("uTime", settings.time);
    m_compositeShader.setFloat("uColorTemp", settings.colorTemp);
    m_compositeShader.setFloat("uColorTint", settings.colorTint);
    m_compositeShader.setFloat("uColorContrast", settings.colorContrast);
    m_compositeShader.setFloat("uColorSaturation", settings.colorSaturation);

    if (settings.ssaoEnabled && settings.ssaoTexture) {
        bindTex(settings.ssaoTexture, 2);
        m_compositeShader.setInt("uSSAOTex", 2);
        m_compositeShader.setInt("uSSAOEnabled", 1);
    } else {
        m_compositeShader.setInt("uSSAOEnabled", 0);
    }

    drawQuad();
    glEnable(GL_DEPTH_TEST);
}

void PostProcess::bindTex(GLuint tex, int slot) {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, tex);
}

bool PostProcess::createFBOs(int w, int h) {
    glGenFramebuffers(1, &m_hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_hdrFBO);

    glGenTextures(1, &m_hdrColorTex);
    glBindTexture(GL_TEXTURE_2D, m_hdrColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_hdrColorTex, 0);

    glGenTextures(1, &m_hdrDepthTex);
    glBindTexture(GL_TEXTURE_2D, m_hdrDepthTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_hdrDepthTex, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        core::Logger::error("PostProcess", "HDR FBO failed!");
        return false;
    }

    for (int i = 0; i < 2; i++) {
        glGenFramebuffers(1, &m_pingpongFBO[i]);
        glGenTextures(1, &m_pingpongTex[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, m_pingpongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, m_pingpongTex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_pingpongTex[i], 0);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void PostProcess::createQuad() {
    float v[] = { -1,-1,0,0, 1,-1,1,0, 1,1,1,1, -1,1,0,1 };
    uint32_t idx[] = { 0,1,2, 0,2,3 };
    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);
    glGenBuffers(1, &m_quadEBO);
    glBindVertexArray(m_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, (void*)8);
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_quadEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);
    glBindVertexArray(0);
}

void PostProcess::drawQuad() {
    glBindVertexArray(m_quadVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void PostProcess::destroyFBOs() {
    if (m_hdrColorTex) glDeleteTextures(1, &m_hdrColorTex);
    if (m_hdrDepthTex) glDeleteTextures(1, &m_hdrDepthTex);
    if (m_hdrFBO) glDeleteFramebuffers(1, &m_hdrFBO);
    for (int i = 0; i < 2; i++) {
        if (m_pingpongTex[i]) glDeleteTextures(1, &m_pingpongTex[i]);
        if (m_pingpongFBO[i]) glDeleteFramebuffers(1, &m_pingpongFBO[i]);
    }
    m_hdrFBO = m_hdrColorTex = m_hdrDepthTex = 0;
    m_pingpongFBO[0] = m_pingpongFBO[1] = 0;
    m_pingpongTex[0] = m_pingpongTex[1] = 0;
}

void PostProcess::destroy() {
    destroyFBOs();
    if (m_quadEBO) glDeleteBuffers(1, &m_quadEBO);
    if (m_quadVBO) glDeleteBuffers(1, &m_quadVBO);
    if (m_quadVAO) glDeleteVertexArrays(1, &m_quadVAO);
}

} // namespace renderer
} // namespace engine
