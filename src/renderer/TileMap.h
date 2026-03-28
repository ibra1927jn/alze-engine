#pragma once

#include "SpriteBatch2D.h"
#include "Texture2D.h"
#include "math/Vector2D.h"
#include <vector>
#include <algorithm>
#include <cmath>

namespace engine {
namespace renderer {

/// TileLayer — A 2D grid of tile IDs that reference a tileset texture.
///
/// Architecture:
///   - Grid of integer tile IDs (0 = empty, 1+ = tile index in tileset)
///   - Tileset: texture atlas with tiles arranged in a grid
///   - Rendered via SpriteBatch2D for maximum batching efficiency
///   - Camera culling: only submits visible tiles to the batch
///
/// Usage:
///   TileLayer layer;
///   layer.init(100, 50, 32);  // 100x50 grid, 32px tiles
///   layer.setTileset(&tilesetTexture, 16, 16);  // 16x16 tiles in atlas
///   layer.setTile(5, 3, 42);  // Place tile 42 at grid pos (5,3)
///   layer.render(batch, cameraX, cameraY, viewW, viewH);
///
class TileLayer {
public:
    TileLayer() = default;

    /// Initialize the tile grid
    void init(int gridWidth, int gridHeight, int tileSize) {
        m_gridW = gridWidth;
        m_gridH = gridHeight;
        m_tileSize = tileSize;
        m_tiles.resize(gridWidth * gridHeight, 0);
    }

    /// Set the tileset texture atlas
    void setTileset(const Texture2D* texture, int tilesPerRow, int tilesPerCol) {
        m_tileset = texture;
        m_tilesPerRow = tilesPerRow;
        m_tilesPerCol = tilesPerCol;
    }

    // ── Tile manipulation ──────────────────────────────────────

    void setTile(int x, int y, int tileId) {
        if (x >= 0 && x < m_gridW && y >= 0 && y < m_gridH)
            m_tiles[y * m_gridW + x] = tileId;
    }

    int getTile(int x, int y) const {
        if (x >= 0 && x < m_gridW && y >= 0 && y < m_gridH)
            return m_tiles[y * m_gridW + x];
        return 0;
    }

    void fill(int tileId) {
        std::fill(m_tiles.begin(), m_tiles.end(), tileId);
    }

    void clear() { fill(0); }

    // ── Rendering ──────────────────────────────────────────────

    /// Render visible tiles through the SpriteBatch2D
    /// cameraX/Y = center of view, viewW/H = viewport dimensions
    void render(SpriteBatch2D& batch, float cameraX, float cameraY,
                float viewW, float viewH, float depth = 0.0f) const
    {
        if (!m_tileset || m_tilesPerRow <= 0) return;

        float ts = static_cast<float>(m_tileSize);

        // Calculate visible tile range (with 1-tile margin for smooth scrolling)
        int startX = std::max(0, static_cast<int>((cameraX - viewW * 0.5f) / ts) - 1);
        int startY = std::max(0, static_cast<int>((cameraY - viewH * 0.5f) / ts) - 1);
        int endX = std::min(m_gridW, static_cast<int>((cameraX + viewW * 0.5f) / ts) + 2);
        int endY = std::min(m_gridH, static_cast<int>((cameraY + viewH * 0.5f) / ts) + 2);

        // UV size per tile in the tileset
        float tileUW = 1.0f / static_cast<float>(m_tilesPerRow);
        float tileVH = 1.0f / static_cast<float>(m_tilesPerCol);

        math::Vector2D tileDrawSize(ts, ts);

        for (int y = startY; y < endY; y++) {
            for (int x = startX; x < endX; x++) {
                int tileId = m_tiles[y * m_gridW + x];
                if (tileId <= 0) continue;  // 0 = empty

                // Convert tileId to atlas UV (1-indexed: tile 1 = atlas position 0)
                int atlasIdx = tileId - 1;
                int atlasCol = atlasIdx % m_tilesPerRow;
                int atlasRow = atlasIdx / m_tilesPerRow;

                SpriteRect uv;
                uv.u0 = atlasCol * tileUW;
                uv.v0 = atlasRow * tileVH;
                uv.u1 = uv.u0 + tileUW;
                uv.v1 = uv.v0 + tileVH;

                // World position (tile center)
                float wx = x * ts + ts * 0.5f;
                float wy = y * ts + ts * 0.5f;

                batch.draw(m_tileset, math::Vector2D(wx, wy), tileDrawSize,
                           0.0f, math::Vector2D(0.5f, 0.5f), uv,
                           SpriteColor::white(), depth);
            }
        }
    }

    // ── Info ────────────────────────────────────────────────────

    int getGridWidth()  const { return m_gridW; }
    int getGridHeight() const { return m_gridH; }
    int getTileSize()   const { return m_tileSize; }

    /// World dimensions in pixels
    float getWorldWidth()  const { return m_gridW * static_cast<float>(m_tileSize); }
    float getWorldHeight() const { return m_gridH * static_cast<float>(m_tileSize); }

private:
    const Texture2D* m_tileset = nullptr;
    int m_tilesPerRow = 0;
    int m_tilesPerCol = 0;

    int m_gridW = 0, m_gridH = 0;
    int m_tileSize = 32;
    std::vector<int> m_tiles;
};

/// TileMap — Multiple tile layers with parallax support.
class TileMap {
public:
    /// Add a new layer (returns layer index)
    int addLayer(int gridW, int gridH, int tileSize) {
        m_layers.emplace_back();
        m_layers.back().init(gridW, gridH, tileSize);
        m_parallax.push_back(1.0f);
        return static_cast<int>(m_layers.size()) - 1;
    }

    TileLayer& getLayer(int index) { return m_layers[index]; }
    const TileLayer& getLayer(int index) const { return m_layers[index]; }

    /// Set parallax factor for a layer (0.5 = moves at half camera speed = background)
    void setParallax(int layerIndex, float factor) {
        if (layerIndex >= 0 && layerIndex < static_cast<int>(m_parallax.size()))
            m_parallax[layerIndex] = factor;
    }

    /// Render all layers back-to-front with parallax
    void render(SpriteBatch2D& batch, float cameraX, float cameraY,
                float viewW, float viewH) const
    {
        for (int i = 0; i < static_cast<int>(m_layers.size()); i++) {
            float px = cameraX * m_parallax[i];
            float py = cameraY * m_parallax[i];
            float depth = static_cast<float>(i) * 0.01f;
            m_layers[i].render(batch, px, py, viewW, viewH, depth);
        }
    }

    int getLayerCount() const { return static_cast<int>(m_layers.size()); }

private:
    std::vector<TileLayer> m_layers;
    std::vector<float> m_parallax;
};

} // namespace renderer
} // namespace engine
