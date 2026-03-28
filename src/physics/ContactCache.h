#pragma once

#include <cstdint>
#include <array>
#include <algorithm>
#include "math/Vector2D.h"

namespace engine {
namespace physics {

/// ContactCache — Warm Starting con Fixed-Size Open-Addressing Hash Table.
///
/// Reemplaza el unordered_map con una tabla de hash fija (8192 slots),
/// eliminando allocaciones dinámicas por frame.
///
/// Usa double-buffering: frame actual → frame anterior al inicio de cada frame.
/// El swap es O(1) (intercambia arrays via pointer swap).
///
/// Key = pairHash(min(a,b), max(a,b)) → O(1) lookup con linear probing
///
struct CachedContact {
    float normalImpulse  = 0.0f;  // Impulso acumulado en normal
    float tangentImpulse = 0.0f;  // Impulso acumulado en tangente
    float normalX = 0.0f;         // Normal del contacto
    float normalY = 0.0f;
    float normalZ = 0.0f;         // Normal Z (3D completo)
    uint32_t frameAge = 0;        // Frames desde última actualización
};

class ContactCache {
public:
    static constexpr int TABLE_SIZE = 8192;  // Must be power of 2
    static constexpr int TABLE_MASK = TABLE_SIZE - 1;
    static constexpr uint64_t EMPTY_KEY = UINT64_MAX;

    ContactCache() {
        m_keysA.fill(EMPTY_KEY);
        m_keysB.fill(EMPTY_KEY);
        m_current  = &m_keysA;
        m_previous = &m_keysB;
        m_dataCurrent  = &m_dataA;
        m_dataPrevious = &m_dataB;
    }

    /// Llamar al inicio de cada frame: intercambia buffers
    void beginFrame() {
        std::swap(m_current, m_previous);
        std::swap(m_dataCurrent, m_dataPrevious);
        // Clear current buffer
        m_current->fill(EMPTY_KEY);
        m_currentCount = m_previousCount;
        m_previousCount = 0;
    }

    /// Genera un hash único para un par de entidades (orden-independiente)
    static uint64_t pairHash(uint32_t a, uint32_t b) {
        uint32_t lo = a < b ? a : b;
        uint32_t hi = a < b ? b : a;
        return (static_cast<uint64_t>(lo) << 32) | hi;
    }

    /// Obtiene el impulso cacheado del frame anterior (warm start)
    CachedContact getWarmStart(uint32_t a, uint32_t b) const {
        uint64_t key = pairHash(a, b);
        uint32_t slot = fibHash(key);

        for (int i = 0; i < MAX_PROBE; i++) {
            uint32_t idx = (slot + i) & TABLE_MASK;
            if ((*m_previous)[idx] == key) {
                return (*m_dataPrevious)[idx];
            }
            if ((*m_previous)[idx] == EMPTY_KEY) {
                return CachedContact{};
            }
        }
        return CachedContact{};
    }

    /// Guarda el impulso resuelto en el frame actual
    void store(uint32_t a, uint32_t b, float normalImpulse, float tangentImpulse,
               float nx, float ny) {
        uint64_t key = pairHash(a, b);
        uint32_t slot = fibHash(key);

        for (int i = 0; i < MAX_PROBE; i++) {
            uint32_t idx = (slot + i) & TABLE_MASK;
            if ((*m_current)[idx] == EMPTY_KEY || (*m_current)[idx] == key) {
                (*m_current)[idx] = key;
                (*m_dataCurrent)[idx] = CachedContact{normalImpulse, tangentImpulse, nx, ny, 0};
                m_previousCount++;
                return;
            }
        }
        // Table full — silently drop (extremely rare with 8192 slots)
    }

    /// Estadísticas
    int getCachedContacts() const { return m_currentCount; }
    int getActiveContacts() const { return m_previousCount; }

    /// Porcentaje de warm starts exitosos en el frame actual
    float getWarmStartRatio() const {
        return m_totalQueries > 0 ?
            static_cast<float>(m_warmHits) / m_totalQueries : 0.0f;
    }

    void resetStats() { m_warmHits = 0; m_totalQueries = 0; }
    void recordQuery(bool hit) {
        m_totalQueries++;
        if (hit) m_warmHits++;
    }

private:
    static constexpr int MAX_PROBE = 16;  // Max linear probing distance

    /// Fibonacci hashing for better distribution
    static uint32_t fibHash(uint64_t key) {
        // Fibonacci hash: multiply by golden ratio constant, take upper bits
        return static_cast<uint32_t>((key * 11400714819323198485ULL) >> 51) & TABLE_MASK;
    }

    using KeyTable  = std::array<uint64_t, TABLE_SIZE>;
    using DataTable = std::array<CachedContact, TABLE_SIZE>;

    KeyTable   m_keysA = {};
    KeyTable   m_keysB = {};
    DataTable  m_dataA = {};
    DataTable  m_dataB = {};

    KeyTable*  m_current  = nullptr;
    KeyTable*  m_previous = nullptr;
    DataTable* m_dataCurrent  = nullptr;
    DataTable* m_dataPrevious = nullptr;

    int m_currentCount  = 0;
    int m_previousCount = 0;
    int m_warmHits = 0;
    int m_totalQueries = 0;
};

} // namespace physics
} // namespace engine
