#include "ScreenEffects.h"
#include <glad/gl.h>

namespace engine {
namespace renderer {

// ── ScreenSpaceReflections ────────────────────────────────────────

bool ScreenSpaceReflections::init(int width, int height) {
    m_width = width;
    m_height = height;

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    glGenTextures(1, &m_ssrTexture);
    glBindTexture(GL_TEXTURE_2D, m_ssrTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ssrTexture, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (!m_shader.compile(SSR_VS, SSR_FS)) return false;

    createQuad();
    m_initialized = true;
    return true;
}

void ScreenSpaceReflections::shutdown() {
    if (m_fbo)        { glDeleteFramebuffers(1,  &m_fbo);        m_fbo = 0; }
    if (m_ssrTexture) { glDeleteTextures(1,       &m_ssrTexture); m_ssrTexture = 0; }
    if (m_quadVAO)    { glDeleteVertexArrays(1,   &m_quadVAO);   m_quadVAO = 0; }
    if (m_quadVBO)    { glDeleteBuffers(1,         &m_quadVBO);   m_quadVBO = 0; }
    m_initialized = false;
}

void ScreenSpaceReflections::apply(GLuint gPosition, GLuint gNormal, GLuint sceneColor,
                                   GLuint depthTexture,
                                   const float* viewData, const float* projData)
{
    if (!m_initialized) return;

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_width, m_height);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    m_shader.use();
    GLuint h = m_shader.getHandle();

    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, gPosition);
    glUniform1i(glGetUniformLocation(h, "gPosition"), 0);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, gNormal);
    glUniform1i(glGetUniformLocation(h, "gNormal"), 1);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, sceneColor);
    glUniform1i(glGetUniformLocation(h, "uSceneColor"), 2);
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, depthTexture);
    glUniform1i(glGetUniformLocation(h, "uDepth"), 3);

    glUniformMatrix4fv(glGetUniformLocation(h, "uView"),       1, GL_FALSE, viewData);
    glUniformMatrix4fv(glGetUniformLocation(h, "uProjection"), 1, GL_FALSE, projData);

    glUniform1f(glGetUniformLocation(h, "uMaxDistance"), settings.maxDistance);
    glUniform1i(glGetUniformLocation(h, "uMaxSteps"),    settings.maxSteps);
    glUniform1f(glGetUniformLocation(h, "uThickness"),   settings.thickness);
    glUniform1f(glGetUniformLocation(h, "uIntensity"),   settings.intensity);
    glUniform2f(glGetUniformLocation(h, "uScreenSize"),
                static_cast<float>(m_width), static_cast<float>(m_height));

    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    m_shader.unbind();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glEnable(GL_DEPTH_TEST);
}

void ScreenSpaceReflections::createQuad() {
    float v[] = { -1,1,0,1, -1,-1,0,0, 1,1,1,1, 1,-1,1,0 };
    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);
    glBindVertexArray(m_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, (void*)8);
    glBindVertexArray(0);
}

// ── MotionBlur ────────────────────────────────────────────────────

bool MotionBlur::init(int width, int height) {
    m_width = width;
    m_height = height;

    glGenFramebuffers(1, &m_fbo);
    glGenTextures(1, &m_outputTex);
    glBindTexture(GL_TEXTURE_2D, m_outputTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_outputTex, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (!m_shader.compile(MB_VS, MB_FS)) return false;

    createQuad();
    m_initialized = true;
    return true;
}

void MotionBlur::shutdown() {
    if (m_fbo)       { glDeleteFramebuffers(1, &m_fbo);       m_fbo = 0; }
    if (m_outputTex) { glDeleteTextures(1,     &m_outputTex); m_outputTex = 0; }
    if (m_quadVAO)   { glDeleteVertexArrays(1, &m_quadVAO);   m_quadVAO = 0; }
    if (m_quadVBO)   { glDeleteBuffers(1,       &m_quadVBO);  m_quadVBO = 0; }
    m_initialized = false;
}

void MotionBlur::apply(GLuint sceneColor, GLuint depthTexture,
                       const float* currentVPInverse, const float* prevVP)
{
    if (!m_initialized) return;

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_width, m_height);
    glDisable(GL_DEPTH_TEST);

    m_shader.use();
    GLuint h = m_shader.getHandle();

    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, sceneColor);
    glUniform1i(glGetUniformLocation(h, "uSceneColor"), 0);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, depthTexture);
    glUniform1i(glGetUniformLocation(h, "uDepth"), 1);

    glUniformMatrix4fv(glGetUniformLocation(h, "uCurrentVPInverse"), 1, GL_FALSE, currentVPInverse);
    glUniformMatrix4fv(glGetUniformLocation(h, "uPrevVP"),            1, GL_FALSE, prevVP);

    glUniform1f(glGetUniformLocation(h, "uIntensity"), settings.intensity);
    glUniform1i(glGetUniformLocation(h, "uSamples"),   settings.samples);
    glUniform1f(glGetUniformLocation(h, "uMaxBlur"),   settings.maxBlur);

    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    m_shader.unbind();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glEnable(GL_DEPTH_TEST);
}

void MotionBlur::createQuad() {
    float v[] = { -1,1,0,1, -1,-1,0,0, 1,1,1,1, 1,-1,1,0 };
    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);
    glBindVertexArray(m_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, (void*)8);
    glBindVertexArray(0);
}

} // namespace renderer
} // namespace engine
