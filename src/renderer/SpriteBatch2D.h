#pragma once

#include <glad/gl.h>
#include "Texture2D.h"
#include "ShaderProgram.h"
#include "math/Vector2D.h"
#include "math/Matrix4x4.h"
#include <vector>
#include <algorithm>
#include <cmath>

namespace engine {
namespace renderer {

struct SpriteRect {
    float u0 = 0.0f, v0 = 0.0f, u1 = 1.0f, v1 = 1.0f;
    static SpriteRect full() { return {0, 0, 1, 1}; }
    static SpriteRect fromPixels(int x, int y, int w, int h, int atlasW, int atlasH) {
        float iw = 1.0f / static_cast<float>(atlasW);
        float ih = 1.0f / static_cast<float>(atlasH);
        return { x * iw, y * ih, (x + w) * iw, (y + h) * ih };
    }
};

struct SpriteColor {
    float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
    SpriteColor() = default;
    SpriteColor(float r_, float g_, float b_, float a_ = 1.0f) : r(r_), g(g_), b(b_), a(a_) {}
    static SpriteColor white()  { return {1, 1, 1, 1}; }
    static SpriteColor red()    { return {1, 0, 0, 1}; }
    static SpriteColor green()  { return {0, 1, 0, 1}; }
    static SpriteColor blue()   { return {0, 0, 1, 1}; }
    static SpriteColor yellow() { return {1, 1, 0, 1}; }
    static SpriteColor clear()  { return {0, 0, 0, 0}; }
};

enum class BlendMode { ALPHA, ADDITIVE, MULTIPLY };

class SpriteBatch2D {
public:
    static constexpr int MAX_SPRITES  = 10000;
    static constexpr int MAX_VERTICES = MAX_SPRITES * 4;
    static constexpr int MAX_INDICES  = MAX_SPRITES * 6;

    SpriteBatch2D() = default;
    ~SpriteBatch2D() { shutdown(); }
    SpriteBatch2D(const SpriteBatch2D&) = delete;
    SpriteBatch2D& operator=(const SpriteBatch2D&) = delete;

    bool init();
    void shutdown();
    void begin(const math::Matrix4x4& viewProjection, BlendMode blend = BlendMode::ALPHA);
    void draw(const Texture2D* texture,
              const math::Vector2D& position, const math::Vector2D& size,
              float rotation = 0.0f,
              const math::Vector2D& origin = math::Vector2D(0.5f, 0.5f),
              const SpriteRect& uv = SpriteRect::full(),
              const SpriteColor& tint = SpriteColor::white(),
              float depth = 0.0f);
    void draw(const Texture2D* texture, float x, float y, float w, float h,
              const SpriteColor& tint = SpriteColor::white(), float depth = 0.0f);
    void end();

    int getDrawCalls() const { return m_drawCalls; }
    int getSpriteCount() const { return m_spriteCount; }

    static math::Matrix4x4 ortho2D(float width, float height) {
        return math::Matrix4x4::orthographic(0, width, height, 0, -1000, 1000);
    }
    static math::Matrix4x4 camera2D(float camX, float camY, float zoom, float viewW, float viewH) {
        float hw = viewW * 0.5f / zoom, hh = viewH * 0.5f / zoom;
        return math::Matrix4x4::orthographic(camX-hw, camX+hw, camY+hh, camY-hh, -1000, 1000);
    }

private:
    struct Vertex { float x, y, u, v, r, g, b, a; };
    struct SpriteEntry {
        const Texture2D* texture = nullptr;
        math::Vector2D position, size, origin;
        float rotation = 0.0f;
        SpriteRect uv;
        SpriteColor tint;
        float depth = 0.0f;
    };

    void generateQuad(const SpriteEntry& s, int baseVertex);
    static void rotatePoint(float& x, float& y, float cosR, float sinR);
    void flushBatch(int vertexCount);

    bool m_initialized = false;
    GLuint m_vao = 0, m_vbo = 0, m_ebo = 0;
    ShaderProgram m_shader;
    math::Matrix4x4 m_viewProjection;
    BlendMode m_blendMode = BlendMode::ALPHA;
    std::vector<SpriteEntry> m_sprites;
    Vertex m_vertexBuffer[MAX_VERTICES];
    int m_drawCalls = 0, m_spriteCount = 0;

    static constexpr const char* SPRITE_VS = R"(
        #version 330 core
        layout(location = 0) in vec2 aPos;
        layout(location = 1) in vec2 aTexCoord;
        layout(location = 2) in vec4 aColor;
        uniform mat4 uVP;
        out vec2 vTexCoord;
        out vec4 vColor;
        void main() {
            gl_Position = uVP * vec4(aPos, 0.0, 1.0);
            vTexCoord = aTexCoord;
            vColor = aColor;
        }
    )";

    static constexpr const char* SPRITE_FS = R"(
        #version 330 core
        in vec2 vTexCoord;
        in vec4 vColor;
        uniform sampler2D uTexture;
        out vec4 FragColor;
        void main() {
            vec4 texColor = texture(uTexture, vTexCoord);
            FragColor = texColor * vColor;
            if (FragColor.a < 0.01) discard;
        }
    )";
};

} // namespace renderer
} // namespace engine
