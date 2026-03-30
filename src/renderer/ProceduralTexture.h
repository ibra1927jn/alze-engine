#pragma once

#include <glad/gl.h>
#include <cmath>
#include <cstdint>
#include <vector>

namespace engine {
namespace renderer {

/// ProceduralTexture — CPU-generated noise textures for PBR surfaces.
///
/// Generates:
///   - Normal maps (Perlin noise-based surface detail)
///   - Roughness variation maps
///   - Checkerboard/grid patterns
///
/// All textures are uploaded to GPU as OpenGL textures.
///
class ProceduralTexture {
public:
    ProceduralTexture() = default;
    ~ProceduralTexture() { destroy(); }

    GLuint getHandle() const { return m_texture; }
    bool isValid() const { return m_texture != 0; }

    void bind(int slot = 0) const {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, m_texture);
    }

    // ── Factory methods ────────────────────────────────────────

    /// Generate a noise-based normal map
    /// scale: frequency of noise features (higher = finer detail)
    /// strength: bump intensity (0.1 = subtle, 1.0 = strong)
    static ProceduralTexture normalMap(int size = 256, float scale = 8.0f, float strength = 0.3f) {
        ProceduralTexture tex;
        std::vector<uint8_t> data(size * size * 4);

        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                float fx = static_cast<float>(x) / size;
                float fy = static_cast<float>(y) / size;

                // Sample noise at offset positions for derivative
                float h00 = fbm(fx * scale, fy * scale, 4);
                float h10 = fbm((fx + 1.0f / size) * scale, fy * scale, 4);
                float h01 = fbm(fx * scale, (fy + 1.0f / size) * scale, 4);

                // Derivatives → normal
                float dx = (h10 - h00) * strength;
                float dy = (h01 - h00) * strength;

                // Normal in tangent space (z-up)
                float nx = -dx;
                float ny = -dy;
                float nz = 1.0f;
                float len = std::sqrt(nx*nx + ny*ny + nz*nz);
                nx /= len; ny /= len; nz /= len;

                // Pack to [0,255]
                int idx = (y * size + x) * 4;
                data[idx + 0] = static_cast<uint8_t>((nx * 0.5f + 0.5f) * 255);
                data[idx + 1] = static_cast<uint8_t>((ny * 0.5f + 0.5f) * 255);
                data[idx + 2] = static_cast<uint8_t>((nz * 0.5f + 0.5f) * 255);
                data[idx + 3] = 255;
            }
        }

        tex.upload(data.data(), size, size, GL_RGBA, true);
        return tex;
    }

    /// Generate a roughness variation map (Perlin-based)
    /// baseRoughness: center value, variation: how much it varies
    static ProceduralTexture roughnessMap(int size = 256, float scale = 4.0f,
                                          float baseRoughness = 0.5f, float variation = 0.3f) {
        ProceduralTexture tex;
        std::vector<uint8_t> data(size * size);

        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                float fx = static_cast<float>(x) / size;
                float fy = static_cast<float>(y) / size;

                float noise = fbm(fx * scale, fy * scale, 3);
                float roughness = baseRoughness + noise * variation;
                roughness = clamp01(roughness);

                data[y * size + x] = static_cast<uint8_t>(roughness * 255);
            }
        }

        tex.upload(data.data(), size, size, GL_RED, true);
        return tex;
    }

    /// Generate a grid/checkerboard pattern (for floors)
    /// lineWidth: width of grid lines in pixels
    static ProceduralTexture gridPattern(int size = 512, int cells = 10,
                                         float lineWidth = 0.02f,
                                         uint8_t bgR = 180, uint8_t bgG = 175, uint8_t bgB = 170,
                                         uint8_t lineR = 80, uint8_t lineG = 75, uint8_t lineB = 72) {
        ProceduralTexture tex;
        std::vector<uint8_t> data(size * size * 4);

        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                float fx = static_cast<float>(x) / size * cells;
                float fy = static_cast<float>(y) / size * cells;

                float distX = std::abs(fx - std::round(fx));
                float distY = std::abs(fy - std::round(fy));
                float dist = std::min(distX, distY);

                bool isLine = dist < lineWidth;

                // Add subtle noise to break up flat colors
                float noise = hash(x * 7919 + y * 6271) * 0.03f;

                int idx = (y * size + x) * 4;
                if (isLine) {
                    data[idx + 0] = clampByte(lineR + noise * 255);
                    data[idx + 1] = clampByte(lineG + noise * 255);
                    data[idx + 2] = clampByte(lineB + noise * 255);
                } else {
                    data[idx + 0] = clampByte(bgR + noise * 255);
                    data[idx + 1] = clampByte(bgG + noise * 255);
                    data[idx + 2] = clampByte(bgB + noise * 255);
                }
                data[idx + 3] = 255;
            }
        }

        tex.upload(data.data(), size, size, GL_RGBA, true);
        return tex;
    }

    /// Generate a subtle AO map (baked ambient occlusion from noise)
    static ProceduralTexture aoMap(int size = 256, float scale = 3.0f) {
        ProceduralTexture tex;
        std::vector<uint8_t> data(size * size);

        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                float fx = static_cast<float>(x) / size;
                float fy = static_cast<float>(y) / size;
                float noise = fbm(fx * scale, fy * scale, 3);
                float ao = 0.75f + noise * 0.25f; // Range [0.5, 1.0]
                ao = clamp01(ao);
                data[y * size + x] = static_cast<uint8_t>(ao * 255);
            }
        }

        tex.upload(data.data(), size, size, GL_RED, true);
        return tex;
    }

private:
    GLuint m_texture = 0;

    void upload(const void* pixels, int w, int h, GLenum format, bool generateMips) {
        glGenTextures(1, &m_texture);
        glBindTexture(GL_TEXTURE_2D, m_texture);

        GLenum internalFormat = (format == GL_RGBA) ? GL_RGBA8 : GL_R8;
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, w, h, 0, format, GL_UNSIGNED_BYTE, pixels);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        if (generateMips) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glGenerateMipmap(GL_TEXTURE_2D);
        } else {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }

        // Anisotropic filtering (if available)
        float maxAniso;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso);
        if (maxAniso > 1.0f) {
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, std::min(maxAniso, 8.0f));
        }

        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void destroy() {
        if (m_texture) {
            glDeleteTextures(1, &m_texture);
            m_texture = 0;
        }
    }

    // ── Noise functions ────────────────────────────────────────

    static float hash(int n) {
        n = (n << 13) ^ n;
        return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
    }

    static float smoothNoise(float x, float y) {
        int ix = static_cast<int>(std::floor(x));
        int iy = static_cast<int>(std::floor(y));
        float fx = x - ix;
        float fy = y - iy;

        // Smoothstep
        fx = fx * fx * (3.0f - 2.0f * fx);
        fy = fy * fy * (3.0f - 2.0f * fy);

        float v00 = hash(ix + iy * 57);
        float v10 = hash(ix + 1 + iy * 57);
        float v01 = hash(ix + (iy + 1) * 57);
        float v11 = hash(ix + 1 + (iy + 1) * 57);

        float i1 = v00 + fx * (v10 - v00);
        float i2 = v01 + fx * (v11 - v01);
        return i1 + fy * (i2 - i1);
    }

    /// Fractal Brownian Motion — layered noise
    static float fbm(float x, float y, int octaves) {
        float value = 0.0f;
        float amplitude = 0.5f;
        float frequency = 1.0f;
        for (int i = 0; i < octaves; i++) {
            value += amplitude * smoothNoise(x * frequency, y * frequency);
            frequency *= 2.0f;
            amplitude *= 0.5f;
        }
        return value;
    }

    static float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
    static uint8_t clampByte(float v) { return static_cast<uint8_t>(clamp01(v / 255.0f) * 255); }
};

} // namespace renderer
} // namespace engine
