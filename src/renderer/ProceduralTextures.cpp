#include "ProceduralTextures.h"

#include <cmath>
#include <vector>

namespace engine {
namespace renderer {
namespace ProceduralTextures {

// ── Helpers ────────────────────────────────────────────────────

inline float fract(float x) { return x - std::floor(x); }

inline float hash(float x, float y) {
    float h = std::sin(x * 127.1f + y * 311.7f) * 43758.5453f;
    return fract(h);
}

// Value noise 2D
inline float valueNoise(float x, float y) {
    float ix = std::floor(x), iy = std::floor(y);
    float fx = x - ix, fy = y - iy;
    // Smoothstep
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);

    float a = hash(ix, iy);
    float b = hash(ix + 1, iy);
    float c = hash(ix, iy + 1);
    float d = hash(ix + 1, iy + 1);

    return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
}

// FBM (Fractal Brownian Motion)
inline float fbm(float x, float y, int octaves = 5) {
    float value = 0.0f, amplitude = 0.5f;
    float frequency = 1.0f;
    for (int i = 0; i < octaves; i++) {
        value += amplitude * valueNoise(x * frequency, y * frequency);
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }
    return value;
}

inline uint8_t clampByte(float v) {
    return static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
}

// Height → Normal (Sobel filter)
inline void heightToNormal(const std::vector<float>& heights, int size,
                           std::vector<Pixel>& output, float strength = 2.0f) {
    output.resize(size * size);
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int xm = (x - 1 + size) % size;
            int xp = (x + 1) % size;
            int ym = (y - 1 + size) % size;
            int yp = (y + 1) % size;

            float dX = heights[y * size + xp] - heights[y * size + xm];
            float dY = heights[yp * size + x] - heights[ym * size + x];

            // Normal in tangent space
            float nx = -dX * strength;
            float ny = -dY * strength;
            float nz = 1.0f;
            float len = std::sqrt(nx * nx + ny * ny + nz * nz);
            nx /= len; ny /= len; nz /= len;

            output[y * size + x] = {
                clampByte((nx * 0.5f + 0.5f) * 255),
                clampByte((ny * 0.5f + 0.5f) * 255),
                clampByte((nz * 0.5f + 0.5f) * 255),
                255
            };
        }
    }
}

// ════════════════════════════════════════════════════════════════
// Normal Maps
// ════════════════════════════════════════════════════════════════

GLuint createBrickNormal(int size) {
    std::vector<float> heights(size * size);
    float brickW = size / 8.0f, brickH = size / 4.0f;
    float mortarW = size / 128.0f;

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int row = static_cast<int>(y / brickH);
            float offsetX = (row % 2 == 0) ? 0 : brickW * 0.5f;
            float localX = std::fmod(x + offsetX, brickW);
            float localY = std::fmod(static_cast<float>(y), brickH);

            bool isMortar = localX < mortarW || localX > brickW - mortarW ||
                           localY < mortarW || localY > brickH - mortarW;

            if (isMortar) {
                heights[y * size + x] = 0.0f;
            } else {
                // Surface variation
                float noiseval = valueNoise(x * 0.05f, y * 0.05f) * 0.15f;
                heights[y * size + x] = 0.8f + noiseval;
            }
        }
    }

    std::vector<Pixel> normals;
    heightToNormal(heights, size, normals, 4.0f);

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, normals.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

GLuint createScratchesNormal(int size) {
    std::vector<float> heights(size * size, 0.5f);

    // Random scratches (angled lines)
    for (int s = 0; s < 120; s++) {
        float angle = hash(s * 13.0f, s * 7.0f) * 3.14159f;
        float startX = hash(s * 17.0f, s * 3.0f) * size;
        float startY = hash(s * 23.0f, s * 11.0f) * size;
        float length = 30 + hash(s * 29.0f, s * 37.0f) * 150;
        float depth = 0.1f + hash(s * 41.0f, s * 43.0f) * 0.25f;
        float dx = std::cos(angle), dy = std::sin(angle);

        for (float t = 0; t < length; t += 0.5f) {
            int px = static_cast<int>(startX + dx * t) % size;
            int py = static_cast<int>(startY + dy * t) % size;
            if (px < 0) px += size;
            if (py < 0) py += size;
            heights[py * size + px] -= depth;
            // Adjacent pixels for width
            int px2 = (px + 1) % size;
            heights[py * size + px2] -= depth * 0.5f;
        }
    }

    std::vector<Pixel> normals;
    heightToNormal(heights, size, normals, 3.0f);

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, normals.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

GLuint createMarbleNormal(int size) {
    std::vector<float> heights(size * size);

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float fx = static_cast<float>(x) / size;
            float fy = static_cast<float>(y) / size;
            float noise = fbm(fx * 6, fy * 6, 6);
            float marble = std::sin((fx + noise * 2.0f) * 12.0f) * 0.5f + 0.5f;
            heights[y * size + x] = marble;
        }
    }

    std::vector<Pixel> normals;
    heightToNormal(heights, size, normals, 1.5f);

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, normals.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

GLuint createNoiseNormal(int size, float scale) {
    std::vector<float> heights(size * size);

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float fx = static_cast<float>(x) / size * scale;
            float fy = static_cast<float>(y) / size * scale;
            heights[y * size + x] = fbm(fx, fy, 5);
        }
    }

    std::vector<Pixel> normals;
    heightToNormal(heights, size, normals, 2.0f);

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, normals.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

// ════════════════════════════════════════════════════════════════
// Albedo Textures
// ════════════════════════════════════════════════════════════════

GLuint createBrickAlbedo(int size) {
    std::vector<Pixel> pixels(size * size);
    float brickW = size / 8.0f, brickH = size / 4.0f;
    float mortarW = size / 128.0f;

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int row = static_cast<int>(y / brickH);
            float offsetX = (row % 2 == 0) ? 0 : brickW * 0.5f;
            float localX = std::fmod(x + offsetX, brickW);
            float localY = std::fmod(static_cast<float>(y), brickH);

            bool isMortar = localX < mortarW || localX > brickW - mortarW ||
                           localY < mortarW || localY > brickH - mortarW;

            if (isMortar) {
                float n = valueNoise(x * 0.1f, y * 0.1f);
                uint8_t v = clampByte(160 + n * 40);
                pixels[y * size + x] = {v, static_cast<uint8_t>(v - 10), static_cast<uint8_t>(v - 15), 255};
            } else {
                // Brick color with variation
                int brickId = row * 8 + static_cast<int>((x + offsetX) / brickW);
                float variation = hash(static_cast<float>(brickId), static_cast<float>(row)) * 0.3f;
                float n = valueNoise(x * 0.08f, y * 0.08f) * 0.15f;
                float r = 0.55f + variation + n;
                float g = 0.25f + variation * 0.5f + n;
                float b = 0.18f + variation * 0.3f + n;
                pixels[y * size + x] = {clampByte(r * 255), clampByte(g * 255), clampByte(b * 255), 255};
            }
        }
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

GLuint createStoneFloorAlbedo(int size) {
    std::vector<Pixel> pixels(size * size);
    float tileSize = size / 4.0f;
    float gap = size / 256.0f;

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float lx = std::fmod(static_cast<float>(x), tileSize);
            float ly = std::fmod(static_cast<float>(y), tileSize);
            bool isGap = lx < gap || lx > tileSize - gap ||
                        ly < gap || ly > tileSize - gap;

            if (isGap) {
                pixels[y * size + x] = {40, 38, 35, 255};
            } else {
                float n1 = fbm(x * 0.02f, y * 0.02f, 4) * 0.2f;
                float n2 = valueNoise(x * 0.1f, y * 0.1f) * 0.05f;
                int tileId = static_cast<int>(x / tileSize) + static_cast<int>(y / tileSize) * 4;
                float tileVar = hash(static_cast<float>(tileId), 42.0f) * 0.1f;
                float base = 0.45f + tileVar + n1 + n2;
                uint8_t v = clampByte(base * 255);
                pixels[y * size + x] = {v, static_cast<uint8_t>(v - 3), static_cast<uint8_t>(v - 5), 255};
            }
        }
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

GLuint createStoneFloorNormal(int size) {
    std::vector<float> heights(size * size);
    float tileSize = size / 4.0f;
    float gap = size / 256.0f;

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float lx = std::fmod(static_cast<float>(x), tileSize);
            float ly = std::fmod(static_cast<float>(y), tileSize);
            bool isGap = lx < gap || lx > tileSize - gap ||
                        ly < gap || ly > tileSize - gap;
            if (isGap) {
                heights[y * size + x] = 0.0f;
            } else {
                heights[y * size + x] = 0.7f + fbm(x * 0.03f, y * 0.03f, 4) * 0.3f;
            }
        }
    }

    std::vector<Pixel> normals;
    heightToNormal(heights, size, normals, 3.0f);

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, normals.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

} // namespace ProceduralTextures
} // namespace renderer
} // namespace engine
