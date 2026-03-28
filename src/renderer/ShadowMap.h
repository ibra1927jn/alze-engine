#pragma once

#include <glad/gl.h>
#include <iostream>

namespace engine {
namespace renderer {

/// ShadowMap — Cascaded Shadow Maps (2 cascades).
///
/// Each cascade has its own FBO + depth texture. Cascade 0 covers
/// near geometry (high detail), cascade 1 covers far geometry.
///
/// Usage:
///   ShadowMap shadow;
///   shadow.init(2048);
///   for (int c = 0; c < 2; c++) {
///       shadow.beginPass(c);
///       ... render scene with depth shader using getLightSpaceMatrix(c) ...
///       shadow.endPass(viewW, viewH);
///   }
///   shadow.bindTexture(0, slot0);
///   shadow.bindTexture(1, slot1);
///
static constexpr int CSM_NUM_CASCADES = 2;

class ShadowMap {
public:
    ShadowMap() = default;
    ~ShadowMap() { destroy(); }

    ShadowMap(const ShadowMap&) = delete;
    ShadowMap& operator=(const ShadowMap&) = delete;

    /// Initialize shadow maps for all cascades
    bool init(int resolution = 2048) {
        m_resolution = resolution;

        for (int c = 0; c < CSM_NUM_CASCADES; c++) {
            glGenFramebuffers(1, &m_fbo[c]);
            glGenTextures(1, &m_depthTex[c]);

            glBindTexture(GL_TEXTURE_2D, m_depthTex[c]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
                         resolution, resolution, 0,
                         GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
            float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
            glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

            // Hardware PCF comparison
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

            glBindFramebuffer(GL_FRAMEBUFFER, m_fbo[c]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                   GL_TEXTURE_2D, m_depthTex[c], 0);
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);

            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                std::cerr << "[ShadowMap] Cascade " << c << " FBO incomplete!" << std::endl;
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                return false;
            }
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        return true;
    }

    /// Begin shadow render pass for a specific cascade
    void beginPass(int cascade) {
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &m_prevFBO);
        glViewport(0, 0, m_resolution, m_resolution);
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo[cascade]);
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(2.0f, 4.0f);
    }

    /// End shadow render pass, restore previous framebuffer
    void endPass(int viewportW, int viewportH) {
        glDisable(GL_POLYGON_OFFSET_FILL);
        glBindFramebuffer(GL_FRAMEBUFFER, m_prevFBO);
        glViewport(0, 0, viewportW, viewportH);
    }

    /// Bind cascade depth texture for sampling
    void bindTexture(int cascade, int slot) const {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, m_depthTex[cascade]);
    }

    GLuint getDepthTexture(int cascade) const { return m_depthTex[cascade]; }
    int getResolution() const { return m_resolution; }
    bool isValid() const { return m_fbo[0] != 0; }

private:
    GLuint m_fbo[CSM_NUM_CASCADES] = {0, 0};
    GLuint m_depthTex[CSM_NUM_CASCADES] = {0, 0};
    int    m_resolution = 2048;
    GLint  m_prevFBO = 0;

    void destroy() {
        for (int c = 0; c < CSM_NUM_CASCADES; c++) {
            if (m_depthTex[c]) glDeleteTextures(1, &m_depthTex[c]);
            if (m_fbo[c]) glDeleteFramebuffers(1, &m_fbo[c]);
            m_fbo[c] = m_depthTex[c] = 0;
        }
    }
};

} // namespace renderer
} // namespace engine
