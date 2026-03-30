#pragma once

#include <glad/gl.h>
#include <vector>
#include <cmath>
#include <cstdint>

namespace engine {
namespace renderer {

/// ProceduralTextures — Genera texturas de alta calidad sin archivos.
///
/// Genera normal maps, roughness maps y AO maps procedurales.
/// Todas las texturas son RGBA8 con resolución configurable.
///
namespace ProceduralTextures {

struct Pixel { uint8_t r, g, b, a; };

// ════════════════════════════════════════════════════════════════
// Normal Maps
// ════════════════════════════════════════════════════════════════

GLuint createBrickNormal(int size = 512);
GLuint createScratchesNormal(int size = 512);
GLuint createMarbleNormal(int size = 512);
GLuint createNoiseNormal(int size = 512, float scale = 8.0f);

// ════════════════════════════════════════════════════════════════
// Albedo Textures
// ════════════════════════════════════════════════════════════════

GLuint createBrickAlbedo(int size = 512);
GLuint createStoneFloorAlbedo(int size = 512);
GLuint createStoneFloorNormal(int size = 512);

} // namespace ProceduralTextures
} // namespace renderer
} // namespace engine
