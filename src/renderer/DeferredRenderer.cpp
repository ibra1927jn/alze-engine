#include "DeferredRenderer.h"
#include "MeshPrimitives.h"
#include <glad/gl.h>

namespace engine {
namespace renderer {

bool DeferredRenderer::init(int width, int height) {
    m_width = width;
    m_height = height;

    glGenFramebuffers(1, &m_gBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, m_gBuffer);

    // RT0: Position (RGB16F) + Metallic (A)
    glGenTextures(1, &m_gPosition);
    glBindTexture(GL_TEXTURE_2D, m_gPosition);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_gPosition, 0);

    // RT1: Normal (RGB16F) + Roughness (A)
    glGenTextures(1, &m_gNormal);
    glBindTexture(GL_TEXTURE_2D, m_gNormal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_gNormal, 0);

    // RT2: Albedo (RGBA8)
    glGenTextures(1, &m_gAlbedo);
    glBindTexture(GL_TEXTURE_2D, m_gAlbedo);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, m_gAlbedo, 0);

    // RT3: Velocity (RG16F)
    glGenTextures(1, &m_gVelocity);
    glBindTexture(GL_TEXTURE_2D, m_gVelocity);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, width, height, 0, GL_RG, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, m_gVelocity, 0);

    // Depth
    glGenRenderbuffers(1, &m_depthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_depthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthRBO);

    GLenum attachments[4] = {
        GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
        GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3
    };
    glDrawBuffers(4, attachments);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (!m_geometryShader.compile(GEOMETRY_VS, GEOMETRY_FS)) return false;
    if (!m_lightingShader.compile(LIGHTING_VS, LIGHTING_FS)) return false;

    createScreenQuad();
    cacheUniformLocations();
    m_initialized = true;
    return true;
}

void DeferredRenderer::cacheUniformLocations() {
    // Geometry shader uniforms
    GLuint gh = m_geometryShader.getHandle();
    m_uniforms.geoView       = glGetUniformLocation(gh, "uView");
    m_uniforms.geoProjection = glGetUniformLocation(gh, "uProjection");
    m_uniforms.geoModel      = glGetUniformLocation(gh, "uModel");
    m_uniforms.geoAlbedo     = glGetUniformLocation(gh, "uAlbedo");
    m_uniforms.geoMetallic   = glGetUniformLocation(gh, "uMetallic");
    m_uniforms.geoRoughness  = glGetUniformLocation(gh, "uRoughness");

    // Lighting shader uniforms
    GLuint lh = m_lightingShader.getHandle();
    m_uniforms.litGPosition        = glGetUniformLocation(lh, "gPosition");
    m_uniforms.litGNormal          = glGetUniformLocation(lh, "gNormal");
    m_uniforms.litGAlbedo          = glGetUniformLocation(lh, "gAlbedo");
    m_uniforms.litViewPos          = glGetUniformLocation(lh, "uViewPos");
    m_uniforms.litLightDir         = glGetUniformLocation(lh, "uLightDir");
    m_uniforms.litLightColor       = glGetUniformLocation(lh, "uLightColor");
    m_uniforms.litAmbientIntensity = glGetUniformLocation(lh, "uAmbientIntensity");
}

void DeferredRenderer::shutdown() {
    if (m_gBuffer)   { glDeleteFramebuffers(1,  &m_gBuffer);   m_gBuffer = 0; }
    if (m_gPosition) { glDeleteTextures(1,      &m_gPosition); m_gPosition = 0; }
    if (m_gNormal)   { glDeleteTextures(1,      &m_gNormal);   m_gNormal = 0; }
    if (m_gAlbedo)   { glDeleteTextures(1,      &m_gAlbedo);   m_gAlbedo = 0; }
    if (m_gVelocity) { glDeleteTextures(1,      &m_gVelocity); m_gVelocity = 0; }
    if (m_depthRBO)  { glDeleteRenderbuffers(1, &m_depthRBO);  m_depthRBO = 0; }
    if (m_quadVAO)   { glDeleteVertexArrays(1,  &m_quadVAO);   m_quadVAO = 0; }
    if (m_quadVBO)   { glDeleteBuffers(1,       &m_quadVBO);   m_quadVBO = 0; }
    m_initialized = false;
}

void DeferredRenderer::resize(int width, int height) {
    shutdown();
    init(width, height);
}

void DeferredRenderer::beginGeometryPass(const math::Matrix4x4& view, const math::Matrix4x4& projection) {
    m_view = view;
    m_projection = projection;
    glBindFramebuffer(GL_FRAMEBUFFER, m_gBuffer);
    glViewport(0, 0, m_width, m_height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    m_geometryShader.use();
    glUniformMatrix4fv(m_uniforms.geoView,       1, GL_FALSE, view.data());
    glUniformMatrix4fv(m_uniforms.geoProjection, 1, GL_FALSE, projection.data());
}

void DeferredRenderer::submitGeometry(const Mesh3D& mesh, const math::Matrix4x4& model,
                                      const math::Vector3D& albedo, float metallic, float roughness)
{
    glUniformMatrix4fv(m_uniforms.geoModel, 1, GL_FALSE, model.data());
    glUniform3f(m_uniforms.geoAlbedo,   albedo.x, albedo.y, albedo.z);
    glUniform1f(m_uniforms.geoMetallic,  metallic);
    glUniform1f(m_uniforms.geoRoughness, roughness);
    mesh.draw();
}

void DeferredRenderer::endGeometryPass() {
    m_geometryShader.unbind();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void DeferredRenderer::lightingPass(const math::Vector3D& viewPos,
                                    const math::Vector3D& lightDir,
                                    const math::Vector3D& lightColor,
                                    float ambientIntensity)
{
    glViewport(0, 0, m_width, m_height);
    glDisable(GL_DEPTH_TEST);
    m_lightingShader.use();

    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, m_gPosition);
    glUniform1i(m_uniforms.litGPosition, 0);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, m_gNormal);
    glUniform1i(m_uniforms.litGNormal, 1);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, m_gAlbedo);
    glUniform1i(m_uniforms.litGAlbedo, 2);

    glUniform3f(m_uniforms.litViewPos,          viewPos.x,   viewPos.y,   viewPos.z);
    glUniform3f(m_uniforms.litLightDir,         lightDir.x,  lightDir.y,  lightDir.z);
    glUniform3f(m_uniforms.litLightColor,       lightColor.x,lightColor.y,lightColor.z);
    glUniform1f(m_uniforms.litAmbientIntensity, ambientIntensity);

    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    m_lightingShader.unbind();
    glEnable(GL_DEPTH_TEST);
}

void DeferredRenderer::createScreenQuad() {
    float quadVertices[] = {
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
    };
    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);
    glBindVertexArray(m_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
}

} // namespace renderer
} // namespace engine
