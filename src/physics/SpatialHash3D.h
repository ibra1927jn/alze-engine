#pragma once

#include "math/Vector3D.h"
#include "Collider3D.h"
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace engine {
namespace physics {

/// SpatialHash3D — 3D Broad Phase with zero heap allocation per frame.
///
/// Uses a flat array + sort approach (identical to the 2D SpatialHash):
///   1. Each body maps to one or more grid cells via its AABB
///   2. All (cellKey, bodyId) entries go into a flat vector
///   3. Sort by cellKey → bodies in the same cell are adjacent
///   4. Walk sorted list → emit pairs for each cell with 2+ bodies
///   5. Sort + deduplicate pairs
///
/// No unordered_map, no per-frame allocations (vectors reuse capacity).
///
class SpatialHash3D {
public:
    using BodyId = uint32_t;

    SpatialHash3D(float cellSize = 2.0f)
        : m_cellSize(cellSize), m_invCellSize(1.0f / cellSize) {
        m_entries.reserve(4096);
        m_pairResult.reserve(1024);
    }

    void clear() {
        m_entries.clear();  // Keeps capacity — no allocation
    }

    /// Insert a body into the grid
    void insert(BodyId id, const AABB3D& aabb) {
        int minX = cellCoord(aabb.min.x);
        int minY = cellCoord(aabb.min.y);
        int minZ = cellCoord(aabb.min.z);
        int maxX = cellCoord(aabb.max.x);
        int maxY = cellCoord(aabb.max.y);
        int maxZ = cellCoord(aabb.max.z);

        for (int z = minZ; z <= maxZ; z++) {
            for (int y = minY; y <= maxY; y++) {
                for (int x = minX; x <= maxX; x++) {
                    uint64_t cellKey = hashCell(x, y, z);
                    m_entries.push_back({cellKey, id});
                }
            }
        }
    }

    /// Get all potential collision pairs (no duplicates)
    const std::vector<std::pair<BodyId, BodyId>>& getPotentialPairs() {
        m_pairResult.clear();

        if (m_entries.size() < 2) return m_pairResult;

        // Sort entries by cell key — O(N log N) but no allocation
        std::sort(m_entries.begin(), m_entries.end(),
            [](const Entry& a, const Entry& b) { return a.cellKey < b.cellKey; }
        );

        // Walk sorted entries — bodies with the same cellKey are adjacent
        size_t n = m_entries.size();
        size_t i = 0;
        while (i < n) {
            // Find the run of entries with the same cellKey
            size_t runStart = i;
            uint64_t key = m_entries[i].cellKey;
            while (i < n && m_entries[i].cellKey == key) i++;
            size_t runEnd = i;

            int runLen = static_cast<int>(runEnd - runStart);
            if (runLen < 2) continue;

            // Emit all pairs in this cell
            for (int a = static_cast<int>(runStart); a < static_cast<int>(runEnd); a++) {
                for (int b = a + 1; b < static_cast<int>(runEnd); b++) {
                    BodyId idA = std::min(m_entries[a].bodyId, m_entries[b].bodyId);
                    BodyId idB = std::max(m_entries[a].bodyId, m_entries[b].bodyId);
                    m_pairResult.push_back({idA, idB});
                }
            }
        }

        // Deduplicate pairs (a body spanning multiple cells → duplicate pairs)
        std::sort(m_pairResult.begin(), m_pairResult.end());
        m_pairResult.erase(
            std::unique(m_pairResult.begin(), m_pairResult.end()),
            m_pairResult.end()
        );

        return m_pairResult;
    }

    void setCellSize(float size) {
        m_cellSize = size;
        m_invCellSize = 1.0f / size;
    }

    float getCellSize() const { return m_cellSize; }

private:
    struct Entry {
        uint64_t cellKey;
        BodyId bodyId;
    };

    int cellCoord(float v) const {
        return static_cast<int>(std::floor(v * m_invCellSize));
    }

    uint64_t hashCell(int x, int y, int z) const {
        // Large primes for spatial hashing (from Teschner et al.)
        uint64_t h = static_cast<uint64_t>(static_cast<uint32_t>(x)) * 73856093ULL;
        h ^= static_cast<uint64_t>(static_cast<uint32_t>(y)) * 19349663ULL;
        h ^= static_cast<uint64_t>(static_cast<uint32_t>(z)) * 83492791ULL;
        return h;
    }

    float m_cellSize;
    float m_invCellSize;
    std::vector<Entry> m_entries;
    std::vector<std::pair<BodyId, BodyId>> m_pairResult;
};

} // namespace physics
} // namespace engine
