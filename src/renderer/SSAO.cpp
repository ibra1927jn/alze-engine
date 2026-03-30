#include "SSAO.h"
#include "core/Logger.h"
#include <cstdlib>

namespace engine {
namespace renderer {

bool SSAO::init(int width, int height) {
    m_width = width / 2;
    m_height = height / 2;

    if (!m_ssaoShader.compile(SSAOShaders::SSAO_VERT, SSAOShaders::SSAO_FRAG)) {
        core::Logger::error("SSAO", "Failed to compile SSAO shader");
        return false;
    }
    if (!m_blurShader.compile(SSAOShaders::SSAO_VERT, SSAOShaders::BLUR_FRAG)) {
        core::Logger::error("SSAO", "Failed to compile blur shader");
        return false;
    }

    m_ssaoShader.use();
    m_locDepthTex      = glGetUniformLocation(m_ssaoShader.getHandle(), "uDepthTex");
    m_locNoiseTex      = glGetUniformLocation(m_ssaoShader.getHandle(), "uNoiseTex");
    m_locProjection    = glGetUniformLocation(m_ssaoShader.getHandle(), "uProjection");
    m_locInvProjection = glGetUniformLocation(m_ssaoShader.getHandle(), "uInvProjection");
    m_locNoiseScale    = glGetUniformLocation(m_ssaoShader.getHandle(), "uNoiseScale");
    m_locRadius        = glGetUniformLocation(m_ssaoShader.getHandle(), "uRadius");
    m_locBias          = glGetUniformLocation(m_ssaoShader.getHandle(), "uBias");
    m_locPower         = glGetUniformLocation(m_ssaoShader.getHandle(), "uPower");

    generateSamples();

    for (int i = 0; i < 32; i++) {
        char buf[32];
        snprintf(buf, 32, "uSamples[%d]", i);
        GLint loc = glGetUniformLocation(m_ssaoShader.getHandle(), buf);
        glUniform3f(loc, m_samples[i * 3], m_samples[i * 3 + 1], m_samples[i * 3 + 2]);
    }

    generateNoiseTexture();
    createFBO(m_ssaoFBO, m_ssaoTex, m_width, m_height);
    createFBO(m_blurFBO, m_blurTex, m_width, m_height);
    createQuad();

    core::Logger::info("SSAO", "Initialized at " + std::to_string(m_width) + "x" + std::to_string(m_height) + " (half-res)");
    return true;
}

void SSAO::resize(int width, int height) {
    m_width = width / 2;
    m_height = height / 2;
    if (m_ssaoTex) glDeleteTextures(1, &m_ssaoTex);
    if (m_blurTex) glDeleteTextures(1, &m_blurTex);
    if (m_ssaoFBO) glDeleteFramebuffers(1, &m_ssaoFBO);
    if (m_blurFBO) glDeleteFramebuffers(1, &m_blurFBO);
    createFBO(m_ssaoFBO, m_ssaoTex, m_width, m_height);
    createFBO(m_blurFBO, m_blurTex, m_width, m_height);
}

void SSAO::generate(GLuint depthTexture, const math::Matrix4x4& projection) {
    if (!m_settings.enabled) return;

    math::Matrix4x4 invProj = projection.inverse();

    GLint prevFBO;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
    GLint prevViewport[4];
    glGetIntegerv(GL_VIEWPORT, prevViewport);

    glDisable(GL_DEPTH_TEST);

    // Pass 1: SSAO
    glBindFramebuffer(GL_FRAMEBUFFER, m_ssaoFBO);
    glViewport(0, 0, m_width, m_height);
    glClear(GL_COLOR_BUFFER_BIT);
    m_ssaoShader.use();

    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, depthTexture);
    glUniform1i(m_locDepthTex, 0);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, m_noiseTex);
    glUniform1i(m_locNoiseTex, 1);

    glUniformMatrix4fv(m_locProjection,    1, GL_FALSE, projection.data());
    glUniformMatrix4fv(m_locInvProjection, 1, GL_FALSE, invProj.data());
    glUniform2f(m_locNoiseScale, m_width / 4.0f, m_height / 4.0f);
    glUniform1f(m_locRadius, m_settings.radius);
    glUniform1f(m_locBias,   m_settings.bias);
    glUniform1f(m_locPower,  m_settings.power);

    drawQuad();

    // Pass 2: Bilateral Blur
    glBindFramebuffer(GL_FRAMEBUFFER, m_blurFBO);
    glClear(GL_COLOR_BUFFER_BIT);
    m_blurShader.use();
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, m_ssaoTex);
    glUniform1i(glGetUniformLocation(m_blurShader.getHandle(), "uSSAOTex"), 0);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, depthTexture);
    glUniform1i(glGetUniformLocation(m_blurShader.getHandle(), "uDepthTex"), 1);

    drawQuad();

    glEnable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
}

void SSAO::generateSamples() {
    for (int i = 0; i < 32; i++) {
        float x = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 2.0f - 1.0f;
        float y = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 2.0f - 1.0f;
        float z = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        float len = std::sqrt(x*x + y*y + z*z);
        if (len < 0.001f) len = 1.0f;
        x /= len; y /= len; z /= len;
        float scale = static_cast<float>(i) / 32.0f;
        scale = 0.1f + scale * scale * 0.9f;
        m_samples[i*3+0] = x * scale;
        m_samples[i*3+1] = y * scale;
        m_samples[i*3+2] = z * scale;
    }
}

void SSAO::generateNoiseTexture() {
    float noise[4 * 4 * 3];
    for (int i = 0; i < 16; i++) {
        noise[i*3+0] = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 2.0f - 1.0f;
        noise[i*3+1] = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 2.0f - 1.0f;
        noise[i*3+2] = 0.0f;
    }
    glGenTextures(1, &m_noiseTex);
    glBindTexture(GL_TEXTURE_2D, m_noiseTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 4, 4, 0, GL_RGB, GL_FLOAT, noise);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void SSAO::createFBO(GLuint& fbo, GLuint& tex, int w, int h) {
    glGenFramebuffers(1, &fbo);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, w, h, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        core::Logger::error("SSAO", "FBO incomplete!");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SSAO::createQuad() {
    float quadVerts[] = {
        -1,-1,0,0, 1,-1,1,0, 1,1,1,1,
        -1,-1,0,0, 1,1,1,1, -1,1,0,1,
    };
    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);
    glBindVertexArray(m_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, (void*)8);
    glBindVertexArray(0);
}

void SSAO::drawQuad() {
    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void SSAO::destroy() {
    if (m_ssaoTex)  glDeleteTextures(1, &m_ssaoTex);
    if (m_blurTex)  glDeleteTextures(1, &m_blurTex);
    if (m_noiseTex) glDeleteTextures(1, &m_noiseTex);
    if (m_ssaoFBO)  glDeleteFramebuffers(1, &m_ssaoFBO);
    if (m_blurFBO)  glDeleteFramebuffers(1, &m_blurFBO);
    if (m_quadVBO)  glDeleteBuffers(1, &m_quadVBO);
    if (m_quadVAO)  glDeleteVertexArrays(1, &m_quadVAO);
}

} // namespace renderer
} // namespace engine
