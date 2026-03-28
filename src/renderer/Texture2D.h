#pragma once

#include <glad/gl.h>
#include <iostream>
#include <string>
#include "ImageDecoder.h"

namespace engine {
namespace renderer {

/// Texture2D — Textura 2D OpenGL con mipmap y filtrado.
///
/// Soporta carga desde:
///   - Archivo (PNG, BMP, HDR via ImageDecoder propio)
///   - Memoria (raw RGBA data)
///   - Procedural (solid color, checkerboard)
///
/// Uso:
///   Texture2D tex;
///   tex.loadFromMemory(data, 512, 512, 4);
///   tex.bind(0);  // Activa en slot GL_TEXTURE0
///
class Texture2D {
public:
    Texture2D() = default;
    ~Texture2D() { destroy(); }

    // No copiable
    Texture2D(const Texture2D&) = delete;
    Texture2D& operator=(const Texture2D&) = delete;

    // Movible
    Texture2D(Texture2D&& other) noexcept
        : m_handle(other.m_handle), m_width(other.m_width), m_height(other.m_height) {
        other.m_handle = 0;
    }
    Texture2D& operator=(Texture2D&& other) noexcept {
        if (this != &other) {
            destroy();
            m_handle = other.m_handle;
            m_width = other.m_width;
            m_height = other.m_height;
            other.m_handle = 0;
        }
        return *this;
    }

    /// Cargar desde datos raw en memoria
    /// @param data Puntero a pixeles (RGBA u otro formato)
    /// @param width Ancho en pixeles
    /// @param height Alto en pixeles
    /// @param channels 1=R, 3=RGB, 4=RGBA
    /// @param sRGB true to use sRGB internal format (for albedo/emissive textures)
    bool loadFromMemory(const unsigned char* data, int width, int height, int channels = 4, bool sRGB = false) {
        if (!data || width <= 0 || height <= 0) return false;

        destroy();
        m_width = width;
        m_height = height;

        GLenum internalFormat, dataFormat;
        switch (channels) {
            case 1: internalFormat = GL_R8;    dataFormat = GL_RED;  break;
            case 3: internalFormat = sRGB ? GL_SRGB8 : GL_RGB8;          dataFormat = GL_RGB;  break;
            case 4: internalFormat = sRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;  dataFormat = GL_RGBA; break;
            default: return false;
        }

        glGenTextures(1, &m_handle);
        glBindTexture(GL_TEXTURE_2D, m_handle);

        // Filtrado bilineal con mipmap
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0,
                     dataFormat, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        // Anisotropic filtering (max quality)
        float maxAniso = 0;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso);
        if (maxAniso > 0)
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, maxAniso > 16.0f ? 16.0f : maxAniso);

        glBindTexture(GL_TEXTURE_2D, 0);
        return true;
    }

    /// Cargar desde archivo (PNG, JPG, BMP, TGA)
    /// @param sRGB true for albedo/emissive textures (gamma-correct sampling)
    bool loadFromFile(const std::string& path, bool flipY = true, bool sRGB = false) {
        if (flipY) stbi_set_flip_vertically_on_load(1);
        else       stbi_set_flip_vertically_on_load(0);

        int w, h, channels;
        unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 0);
        if (!data) {
            std::cerr << "[Texture2D] Error cargando: " << path
                      << " (" << stbi_failure_reason() << ")" << std::endl;
            return false;
        }

        bool ok = loadFromMemory(data, w, h, channels, sRGB);
        stbi_image_free(data);

        if (ok)
            std::cout << "[Texture2D] Cargado: " << path
                      << " (" << w << "x" << h << " ch=" << channels
                      << (sRGB ? " sRGB" : " linear") << ")" << std::endl;
        return ok;
    }

    /// Crear textura de un solo color (1x1 pixel)
    bool loadSolidColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255) {
        unsigned char pixel[4] = {r, g, b, a};
        return loadFromMemory(pixel, 1, 1, 4);
    }

    /// Crear textura checkerboard (para debug/default)
    bool loadCheckerboard(int size = 8, int tileSize = 1) {
        int totalPixels = size * size * 4;
        auto* data = new unsigned char[totalPixels];
        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                bool white = ((x / tileSize) + (y / tileSize)) % 2 == 0;
                int idx = (y * size + x) * 4;
                unsigned char v = white ? 200 : 60;
                data[idx] = v; data[idx+1] = v; data[idx+2] = v; data[idx+3] = 255;
            }
        }
        bool ok = loadFromMemory(data, size, size, 4);
        delete[] data;
        return ok;
    }

    /// Activar textura en un slot (0-15)
    void bind(int slot = 0) const {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, m_handle);
    }

    void unbind() const {
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    GLuint getHandle() const { return m_handle; }
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    bool isValid() const { return m_handle != 0; }

    /// Wrap an existing GL handle (does NOT take ownership — will NOT delete)
    void wrapHandle(GLuint handle, int w = 0, int h = 0) {
        // Liberar handle previo si teniamos ownership para evitar GPU memory leak
        if (m_handle && m_owned) { glDeleteTextures(1, &m_handle); }
        m_handle = handle; m_width = w; m_height = h; m_owned = false;
    }

private:
    GLuint m_handle = 0;
    int m_width = 0;
    int m_height = 0;
    bool m_owned = true;

    void destroy() {
        if (m_handle && m_owned) {
            glDeleteTextures(1, &m_handle);
        }
        m_handle = 0;
    }
};

} // namespace renderer
} // namespace engine
