#pragma once

#include "math/Vector3D.h"
#include "math/Matrix4x4.h"
#include <vector>
#include <cstdint>

namespace engine {
namespace renderer {

/// LODSystem — Level-of-Detail mesh switching.
///
/// Features:
///   - Per-object LOD levels (up to 4: high/med/low/billboard)
///   - Distance-based automatic LOD selection
///   - Hysteresis band to prevent flickering at boundaries
///   - LOD bias for quality/performance tuning
///
/// Usage:
///   LODGroup group;
///   group.addLevel(meshHigh,  0.0f, 20.0f);
///   group.addLevel(meshMed,  18.0f, 50.0f);
///   group.addLevel(meshLow,  45.0f, 100.0f);
///   int lod = group.selectLOD(cameraPos, objectPos);
///
class LODGroup {
public:
    static constexpr int MAX_LEVELS = 4;

    struct LODLevel {
        const Mesh3D* mesh = nullptr;
        float minDistance = 0.0f;
        float maxDistance = 100.0f;
    };

    /// Add a LOD level (must be added in order: highest detail first)
    void addLevel(const Mesh3D* mesh, float minDist, float maxDist) {
        if (m_levelCount >= MAX_LEVELS) return;
        m_levels[m_levelCount].mesh = mesh;
        m_levels[m_levelCount].minDistance = minDist;
        m_levels[m_levelCount].maxDistance = maxDist;
        m_levelCount++;
    }

    /// Select the appropriate LOD level based on camera-to-object distance
    /// Returns LOD index (0 = highest detail), or -1 if out of range
    int selectLOD(const math::Vector3D& cameraPos,
                  const math::Vector3D& objectPos,
                  float lodBias = 1.0f) const {
        float distSq = (objectPos - cameraPos).sqrMagnitude();
        float dist = std::sqrt(distSq) * lodBias;

        for (int i = 0; i < m_levelCount; i++) {
            // Add hysteresis: use minDistance for switching UP, maxDistance for switching DOWN
            float effectiveMin = m_levels[i].minDistance;
            float effectiveMax = m_levels[i].maxDistance;

            // Hysteresis band (5% of range)
            if (i == m_currentLOD) {
                effectiveMin -= (effectiveMax - effectiveMin) * 0.05f;
                effectiveMax += (effectiveMax - effectiveMin) * 0.05f;
            }

            if (dist >= effectiveMin && dist < effectiveMax) {
                m_currentLOD = i;
                return i;
            }
        }

        // Beyond all LOD ranges → use lowest detail or cull
        if (m_levelCount > 0 && dist >= m_levels[m_levelCount - 1].maxDistance) {
            return -1; // Out of range, should be culled
        }

        m_currentLOD = 0;
        return 0; // Default to highest detail
    }

    /// Get the mesh for a specific LOD level
    const Mesh3D* getMesh(int lodLevel) const {
        if (lodLevel < 0 || lodLevel >= m_levelCount) return nullptr;
        return m_levels[lodLevel].mesh;
    }

    /// Get the currently selected LOD mesh
    const Mesh3D* getCurrentMesh() const {
        return getMesh(m_currentLOD);
    }

    int getLevelCount() const { return m_levelCount; }
    int getCurrentLOD() const { return m_currentLOD; }

private:
    LODLevel m_levels[MAX_LEVELS] = {};
    int m_levelCount = 0;
    mutable int m_currentLOD = 0;
};

/// LODManager — Manages LOD groups for multiple objects.
///
class LODManager {
public:
    /// Register an object's LOD group, returns its index
    uint32_t addGroup(const LODGroup& group) {
        m_groups.push_back(group);
        return static_cast<uint32_t>(m_groups.size() - 1);
    }

    /// Update all LOD selections based on camera position
    void update(const math::Vector3D& cameraPos,
                const std::vector<math::Vector3D>& objectPositions,
                float lodBias = 1.0f) {
        m_stats = {};
        uint32_t count = static_cast<uint32_t>(
            objectPositions.size() < m_groups.size() ? objectPositions.size() : m_groups.size());

        for (uint32_t i = 0; i < count; i++) {
            int lod = m_groups[i].selectLOD(cameraPos, objectPositions[i], lodBias);
            if (lod < 0) {
                m_stats.culled++;
            } else {
                m_stats.perLevel[lod]++;
            }
        }
        m_stats.total = count;
    }

    /// Get the LOD group for an object
    LODGroup& getGroup(uint32_t index) { return m_groups[index]; }
    const LODGroup& getGroup(uint32_t index) const { return m_groups[index]; }

    uint32_t getGroupCount() const { return static_cast<uint32_t>(m_groups.size()); }

    // ── Statistics ─────────────────────────────────────────────
    struct Stats {
        uint32_t total = 0;
        uint32_t culled = 0;
        uint32_t perLevel[LODGroup::MAX_LEVELS] = {};
    };

    const Stats& getStats() const { return m_stats; }

private:
    std::vector<LODGroup> m_groups;
    Stats m_stats;
};

} // namespace renderer
} // namespace engine
