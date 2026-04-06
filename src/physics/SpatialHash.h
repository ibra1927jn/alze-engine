#pragma once

#include "Vector2D.h"
#include "AABB.h"
#include "Logger.h"
#include <vector>
#include <cstdint>
#include <algorithm>

namespace engine {
namespace physics {

/// SpatialHash — Uniform Grid para Broad Phase.
///
/// Flujo optimizado para entity-centric collision:
///   1. clear()
///   2. insert() para cada entidad
///   3. buildGrid()   ← bucket-sort entries por celda
///   4. query(aabb)   ← O(K) lookup usando datos pre-sorted
///
/// Internamente usa counting-sort para organizar las entidades
/// por celda en un array contiguo. query() luego lee directamente
/// del array sorted usando offsets precalculados por celda.
///
class SpatialHash {
public:
    using EntityId = uint32_t;

    static constexpr int MAX_ENTRIES = 65536;

    SpatialHash(float cellSize = 64.0f, int gridDim = 128)
        : m_cellSize(cellSize), m_invCellSize(1.0f / cellSize)
        , m_gridDim(gridDim), m_gridCells(gridDim * gridDim)
    {
        m_counts.resize(m_gridCells, 0);
        m_offsets.resize(m_gridCells, 0);
        m_writePos.resize(m_gridCells, 0);
        m_usedCells.reserve(4096);
        m_entries.reserve(MAX_ENTRIES);
        m_sorted.reserve(MAX_ENTRIES);
        m_queryResult.reserve(128);
    }

    /// Limpiar el grid (llamar cada frame)
    void clear() {
        for (int cell : m_usedCells) {
            m_counts[cell] = 0;
        }
        m_usedCells.clear();
        m_entries.clear();
        m_entityCount = 0;
        m_outOfBoundsCount = 0;
        m_gridBuilt = false;
    }

    /// Insertar una entidad en el grid
    void insert(EntityId entity, const math::AABB& aabb) {
        // Detect out-of-bounds entities
        int rawMinX = static_cast<int>(std::floor(aabb.min.x * m_invCellSize));
        int rawMinY = static_cast<int>(std::floor(aabb.min.y * m_invCellSize));
        int rawMaxX = static_cast<int>(std::floor(aabb.max.x * m_invCellSize));
        int rawMaxY = static_cast<int>(std::floor(aabb.max.y * m_invCellSize));

        if (rawMinX < 0 || rawMinY < 0 || rawMaxX >= m_gridDim || rawMaxY >= m_gridDim) {
            m_outOfBoundsCount++;
            if (m_outOfBoundsCount <= 3) {
                core::Logger::warn("SpatialHash",
                    "Entity " + std::to_string(entity) + " out of grid bounds"
                    " (AABB: " + std::to_string(aabb.min.x) + "," + std::to_string(aabb.min.y) +
                    " -> " + std::to_string(aabb.max.x) + "," + std::to_string(aabb.max.y) + ")");
            }
        }

        int minX = clampCell(aabb.min.x);
        int minY = clampCell(aabb.min.y);
        int maxX = clampCell(aabb.max.x);
        int maxY = clampCell(aabb.max.y);

        for (int y = minY; y <= maxY; y++) {
            for (int x = minX; x <= maxX; x++) {
                int cell = y * m_gridDim + x;
                if (m_counts[cell] == 0) {
                    m_usedCells.push_back(cell);
                }
                m_counts[cell]++;
                m_entries.push_back({entity, cell});
            }
        }
        m_entityCount++;
    }

    /// Construir el grid sorted (llamar UNA VEZ después de todos los insert)
    /// Esto organiza las entidades por celda usando counting-sort
    void buildGrid() {
        // Prefix sum: calcular offset de inicio de cada celda
        int total = 0;
        for (int cell : m_usedCells) {
            m_offsets[cell] = total;
            total += m_counts[cell];
        }

        // Copiar offsets como write positions
        for (int cell : m_usedCells) {
            m_writePos[cell] = m_offsets[cell];
        }

        // Colocar entidades en posiciones finales (bucket sort)
        m_sorted.resize(total);
        for (auto& entry : m_entries) {
            m_sorted[m_writePos[entry.cell]++] = entry.entity;
        }

        m_gridBuilt = true;
    }

    /// Query rápido: buscar entidades en celdas que overlapen un AABB
    /// REQUIERE: buildGrid() llamado previamente
    /// Complejidad: O(celdas_tocadas × entidades_por_celda)
    std::vector<EntityId>& query(const math::AABB& aabb) {
        m_queryResult.clear();

        if (!m_gridBuilt) {
            // Fallback: si no se llamó buildGrid, usar scan lineal
            return queryLinear(aabb);
        }

        int minX = clampCell(aabb.min.x);
        int minY = clampCell(aabb.min.y);
        int maxX = clampCell(aabb.max.x);
        int maxY = clampCell(aabb.max.y);

        for (int y = minY; y <= maxY; y++) {
            for (int x = minX; x <= maxX; x++) {
                int cell = y * m_gridDim + x;
                int count = m_counts[cell];
                if (count == 0) continue;

                int start = m_offsets[cell];
                for (int i = start; i < start + count; i++) {
                    m_queryResult.push_back(m_sorted[i]);
                }
            }
        }

        // No dedup aquí — CollisionSystem usa entityA < entityB para evitar duplicados
        return m_queryResult;
    }

    // ── Stats ──────────────────────────────────────────────────
    int getEntityCount() const { return m_entityCount; }
    int getCellCount() const { return static_cast<int>(m_usedCells.size()); }
    float getCellSize() const { return m_cellSize; }
    int getGridDim() const { return m_gridDim; }
    int getOutOfBoundsCount() const { return m_outOfBoundsCount; }

    void setCellSize(float size) {
        if (size < 1e-8f) size = 1e-8f;
        m_cellSize = size;
        m_invCellSize = 1.0f / size;
    }

    // ── Legacy: getPotentialPairs (para tests) ─────────────────
    std::vector<std::pair<EntityId, EntityId>> getPotentialPairs() {
        if (!m_gridBuilt) buildGrid();

        std::vector<std::pair<EntityId, EntityId>> pairs;
        // Simple dedup via hash table
        static constexpr int DEDUP_SIZE = 32768;
        std::vector<uint64_t> dedup(DEDUP_SIZE, UINT64_MAX);

        for (int cell : m_usedCells) {
            int start = m_offsets[cell];
            int count = m_counts[cell];
            if (count < 2) continue;

            for (int i = start; i < start + count; i++) {
                for (int j = i + 1; j < start + count; j++) {
                    EntityId a = m_sorted[i];
                    EntityId b = m_sorted[j];
                    EntityId lo = a < b ? a : b;
                    EntityId hi = a < b ? b : a;
                    uint64_t key = (static_cast<uint64_t>(lo) << 16) | hi;
                    uint32_t slot = static_cast<uint32_t>(key) & (DEDUP_SIZE - 1);
                    if (dedup[slot] != key) {
                        dedup[slot] = key;
                        pairs.push_back({lo, hi});
                    }
                }
            }
        }
        return pairs;
    }

private:
    struct Entry {
        EntityId entity;
        int cell = 0;
    };

    int clampCell(float v) const {
        int c = static_cast<int>(std::floor(v * m_invCellSize));
        if (c < 0) c = 0;
        if (c >= m_gridDim) c = m_gridDim - 1;
        return c;
    }

    /// Fallback lineal para cuando buildGrid() no fue llamado (ej. tests)
    std::vector<EntityId>& queryLinear(const math::AABB& aabb) {
        int minX = clampCell(aabb.min.x);
        int minY = clampCell(aabb.min.y);
        int maxX = clampCell(aabb.max.x);
        int maxY = clampCell(aabb.max.y);

        for (int y = minY; y <= maxY; y++) {
            for (int x = minX; x <= maxX; x++) {
                int cell = y * m_gridDim + x;
                for (auto& entry : m_entries) {
                    if (entry.cell == cell) {
                        m_queryResult.push_back(entry.entity);
                    }
                }
            }
        }
        std::sort(m_queryResult.begin(), m_queryResult.end());
        m_queryResult.erase(
            std::unique(m_queryResult.begin(), m_queryResult.end()),
            m_queryResult.end()
        );
        return m_queryResult;
    }

    float m_cellSize = 0.0f;
    float m_invCellSize = 0.0f;
    int   m_gridDim = 0;
    int   m_gridCells = 0;
    int   m_entityCount = 0;
    int   m_outOfBoundsCount = 0;
    bool  m_gridBuilt = false;

    std::vector<int> m_counts;            // Per-cell entity count
    std::vector<int> m_offsets;           // Prefix sum offsets
    std::vector<int> m_writePos;          // Write positions (temp)
    std::vector<int> m_usedCells;         // Indices of non-empty cells
    std::vector<Entry> m_entries;          // Raw (entity, cell) pairs
    std::vector<EntityId> m_sorted;       // Sorted entity array
    std::vector<EntityId> m_queryResult;  // Reusable query result buffer
};

} // namespace physics
} // namespace engine
