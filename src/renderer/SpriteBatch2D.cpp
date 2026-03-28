#include "SpriteBatch2D.h"
#include <glad/gl.h>
#include <cmath>
#include <algorithm>
#include <vector>
#include <cstdint>

namespace engine {
namespace renderer {

bool SpriteBatch2D::init() {
    if (!m_shader.compile(SPRITE_VS, SPRITE_FS)) return false;

    m_sprites.reserve(1024);

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);
    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, MAX_VERTICES * sizeof(Vertex), nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(4 * sizeof(float)));

    std::vector<uint32_t> indices(MAX_INDICES);
    for (int i = 0; i < MAX_SPRITES; i++) {
        int vi = i * 4, ii = i * 6;
        indices[ii+0]=vi+0; indices[ii+1]=vi+1; indices[ii+2]=vi+2;
        indices[ii+3]=vi+2; indices[ii+4]=vi+3; indices[ii+5]=vi+0;
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);
    m_initialized = true;
    return true;
}

void SpriteBatch2D::shutdown() {
    if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
    if (m_vbo) { glDeleteBuffers(1, &m_vbo);       m_vbo = 0; }
    if (m_ebo) { glDeleteBuffers(1, &m_ebo);       m_ebo = 0; }
    m_initialized = false;
}

void SpriteBatch2D::begin(const math::Matrix4x4& viewProjection, BlendMode blend) {
    m_sprites.clear();
    m_viewProjection = viewProjection;
    m_blendMode = blend;
    m_drawCalls = 0;
    m_spriteCount = 0;
}

void SpriteBatch2D::draw(const Texture2D* texture,
                         const math::Vector2D& position, const math::Vector2D& size,
                         float rotation, const math::Vector2D& origin,
                         const SpriteRect& uv, const SpriteColor& tint, float depth)
{
    if (!texture || m_sprites.size() >= MAX_SPRITES) return;
    SpriteEntry e;
    e.texture = texture; e.position = position; e.size = size;
    e.rotation = rotation; e.origin = origin; e.uv = uv; e.tint = tint; e.depth = depth;
    m_sprites.push_back(e);
}

void SpriteBatch2D::draw(const Texture2D* texture, float x, float y, float w, float h,
                         const SpriteColor& tint, float depth)
{
    draw(texture, math::Vector2D(x, y), math::Vector2D(w, h),
         0.0f, math::Vector2D(0.5f, 0.5f), SpriteRect::full(), tint, depth);
}

void SpriteBatch2D::end() {
    if (m_sprites.empty() || !m_initialized) return;

    std::sort(m_sprites.begin(), m_sprites.end(),
        [](const SpriteEntry& a, const SpriteEntry& b) {
            if (a.depth != b.depth) return a.depth < b.depth;
            return a.texture < b.texture;
        });

    glEnable(GL_BLEND);
    switch (m_blendMode) {
        case BlendMode::ALPHA:    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); break;
        case BlendMode::ADDITIVE: glBlendFunc(GL_SRC_ALPHA, GL_ONE); break;
        case BlendMode::MULTIPLY: glBlendFunc(GL_DST_COLOR, GL_ZERO); break;
    }

    m_shader.use();
    GLint vpLoc  = glGetUniformLocation(m_shader.getHandle(), "uVP");
    GLint texLoc = glGetUniformLocation(m_shader.getHandle(), "uTexture");
    glUniformMatrix4fv(vpLoc, 1, GL_FALSE, m_viewProjection.data());
    glUniform1i(texLoc, 0);
    glBindVertexArray(m_vao);

    const Texture2D* currentTex = nullptr;
    int vertexCount = 0;

    for (const auto& sprite : m_sprites) {
        if (sprite.texture != currentTex && vertexCount > 0) {
            flushBatch(vertexCount);
            vertexCount = 0;
        }
        if (sprite.texture != currentTex) {
            currentTex = sprite.texture;
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, currentTex->getHandle());
        }
        generateQuad(sprite, vertexCount);
        vertexCount += 4;
        if (vertexCount >= MAX_VERTICES) {
            flushBatch(vertexCount);
            vertexCount = 0;
        }
    }
    if (vertexCount > 0) flushBatch(vertexCount);

    m_spriteCount = static_cast<int>(m_sprites.size());
    glBindVertexArray(0);
    m_shader.unbind();
}

void SpriteBatch2D::generateQuad(const SpriteEntry& s, int baseVertex) {
    float ox = -s.origin.x * s.size.x;
    float oy = -s.origin.y * s.size.y;

    float x0 = ox,            y0 = oy;
    float x1 = ox + s.size.x, y1 = oy;
    float x2 = ox + s.size.x, y2 = oy + s.size.y;
    float x3 = ox,            y3 = oy + s.size.y;

    if (s.rotation != 0.0f) {
        float cosR = std::cos(s.rotation);
        float sinR = std::sin(s.rotation);
        rotatePoint(x0, y0, cosR, sinR);
        rotatePoint(x1, y1, cosR, sinR);
        rotatePoint(x2, y2, cosR, sinR);
        rotatePoint(x3, y3, cosR, sinR);
    }

    float px = s.position.x, py = s.position.y;
    int i = baseVertex;
    m_vertexBuffer[i+0] = { px+x0, py+y0, s.uv.u0, s.uv.v0, s.tint.r, s.tint.g, s.tint.b, s.tint.a };
    m_vertexBuffer[i+1] = { px+x1, py+y1, s.uv.u1, s.uv.v0, s.tint.r, s.tint.g, s.tint.b, s.tint.a };
    m_vertexBuffer[i+2] = { px+x2, py+y2, s.uv.u1, s.uv.v1, s.tint.r, s.tint.g, s.tint.b, s.tint.a };
    m_vertexBuffer[i+3] = { px+x3, py+y3, s.uv.u0, s.uv.v1, s.tint.r, s.tint.g, s.tint.b, s.tint.a };
}

void SpriteBatch2D::rotatePoint(float& x, float& y, float cosR, float sinR) {
    float rx = x * cosR - y * sinR;
    float ry = x * sinR + y * cosR;
    x = rx; y = ry;
}

void SpriteBatch2D::flushBatch(int vertexCount) {
    int indexCount = (vertexCount / 4) * 6;
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertexCount * sizeof(Vertex), m_vertexBuffer);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
    m_drawCalls++;
}

} // namespace renderer
} // namespace engine
