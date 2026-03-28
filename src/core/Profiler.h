#pragma once

#include <chrono>
#include <string>
#include <string_view>
#include <vector>
#include <sstream>
#include <iomanip>
#include <array>
#include <algorithm>
#include <cstdint>

namespace engine {
namespace core {

/// Profiler — Medidor de rendimiento con IDs enteros (zero-overhead lookup).
///
/// Usa IDs numéricos en lugar de strings para máximo rendimiento
/// en hot paths. Mantiene historial de frames para graficar.
///
/// Uso:
///   // Definir secciones como constantes
///   constexpr uint32_t PROF_PHYSICS   = 0;
///   constexpr uint32_t PROF_COLLISION = 1;
///   constexpr uint32_t PROF_RENDER    = 2;
///
///   Profiler::beginFrame();
///   Profiler::begin(PROF_PHYSICS);
///   physicsSystem->update(dt);
///   Profiler::end(PROF_PHYSICS);
///   Profiler::endFrame();
///
class Profiler {
public:
    using Clock = std::chrono::high_resolution_clock;

    static constexpr int HISTORY_SIZE  = 120;   // 2 segundos a 60fps
    static constexpr int MAX_SECTIONS  = 16;    // Max profiled sections

    // ── Section IDs predefinidos ────────────────────────────────
    static constexpr uint32_t SECTION_PHYSICS   = 0;
    static constexpr uint32_t SECTION_COLLISION = 1;
    static constexpr uint32_t SECTION_RENDER    = 2;
    static constexpr uint32_t SECTION_INPUT     = 3;
    static constexpr uint32_t SECTION_PARTICLES = 4;
    static constexpr uint32_t SECTION_EVENTS    = 5;
    static constexpr uint32_t SECTION_SLEEP     = 6;
    static constexpr uint32_t SECTION_CUSTOM_0  = 7;

    struct Metric {
        float lastMs;
        float avgMs;
        float maxMs;
        float totalMs;
        int   calls;

        Metric() : lastMs(0), avgMs(0), maxMs(0), totalMs(0), calls(0) {}
    };

    /// Iniciar medición del frame
    static void beginFrame() {
        s_frameStart = Clock::now();
        s_drawCalls = 0;
    }

    /// Finalizar medición del frame
    static void endFrame() {
        auto now = Clock::now();
        float ms = std::chrono::duration<float, std::milli>(now - s_frameStart).count();

        s_frameMetric.lastMs = ms;
        s_frameMetric.avgMs = s_frameMetric.avgMs * 0.95f + ms * 0.05f;
        if (ms > s_frameMetric.maxMs) s_frameMetric.maxMs = ms;
        s_frameMetric.totalMs += ms;
        s_frameMetric.calls++;

        // Historial circular
        s_frameHistory[s_historyIndex] = ms;
        s_historyIndex = (s_historyIndex + 1) % HISTORY_SIZE;
        if (s_historyCount < HISTORY_SIZE) s_historyCount++;
    }

    /// Iniciar medición de un bloque por ID (O(1), zero alloc)
    static void begin(uint32_t sectionId) {
        if (sectionId >= MAX_SECTIONS) return;
        s_sectionTimers[sectionId] = Clock::now();
    }

    /// Finalizar medición de un bloque por ID
    static void end(uint32_t sectionId) {
        if (sectionId >= MAX_SECTIONS) return;

        auto now = Clock::now();
        float ms = std::chrono::duration<float, std::milli>(now - s_sectionTimers[sectionId]).count();

        auto& metric = s_sectionMetrics[sectionId];
        metric.lastMs = ms;
        metric.avgMs = metric.avgMs * 0.95f + ms * 0.05f;
        if (ms > metric.maxMs) metric.maxMs = ms;
        metric.totalMs += ms;
        metric.calls++;
        s_sectionActive[sectionId] = true;

        // Historial por sección
        s_sectionHistory[sectionId][s_sectionHistIdx[sectionId]] = ms;
        s_sectionHistIdx[sectionId] = (s_sectionHistIdx[sectionId] + 1) % HISTORY_SIZE;
    }

    // ── String-based API (backward compat, wraps ID-based) ─────

    /// Iniciar medición de un bloque por nombre (busca o asigna ID)
    static void begin(std::string_view name) {
        begin(getOrCreateId(name));
    }

    /// Finalizar medición de un bloque por nombre
    static void end(std::string_view name) {
        end(getOrCreateId(name));
    }

    // ── Queries ────────────────────────────────────────────────

    static const Metric& getMetric(uint32_t sectionId) {
        return s_sectionMetrics[sectionId < MAX_SECTIONS ? sectionId : 0];
    }

    static const Metric& getMetric(std::string_view name) {
        return getMetric(getOrCreateId(name));
    }

    static const Metric& getFrameMetric() { return s_frameMetric; }

    // ── Frame History (para graficar) ──────────────────────────

    /// Obtener historial de tiempos de frame (más reciente al final)
    static std::vector<float> getFrameHistory() {
        std::vector<float> result;
        result.reserve(s_historyCount);
        for (int i = 0; i < s_historyCount; i++) {
            int idx = (s_historyIndex - s_historyCount + i + HISTORY_SIZE) % HISTORY_SIZE;
            result.push_back(s_frameHistory[idx]);
        }
        return result;
    }

    /// Obtener historial de una sección por ID
    static std::vector<float> getSectionHistory(uint32_t sectionId) {
        if (sectionId >= MAX_SECTIONS || !s_sectionActive[sectionId]) return {};
        std::vector<float> result;
        int idx = s_sectionHistIdx[sectionId];
        for (int i = 0; i < HISTORY_SIZE; i++) {
            int pos = (idx + i) % HISTORY_SIZE;
            result.push_back(s_sectionHistory[sectionId][pos]);
        }
        return result;
    }

    // ── Counters ───────────────────────────────────────────────

    static void addDrawCall(int count = 1) { s_drawCalls += count; }
    static int  getDrawCalls() { return s_drawCalls; }

    static void setMemoryUsage(size_t bytes) { s_memoryBytes = bytes; }
    static size_t getMemoryUsage() { return s_memoryBytes; }

    static std::vector<std::string> getSectionNames() {
        std::vector<std::string> names;
        for (uint32_t i = 0; i < MAX_SECTIONS; i++) {
            if (s_sectionActive[i]) {
                names.push_back(getSectionName(i));
            }
        }
        return names;
    }

    static void resetMax() {
        for (uint32_t i = 0; i < MAX_SECTIONS; i++) {
            s_sectionMetrics[i].maxMs = 0;
        }
        s_frameMetric.maxMs = 0;
    }

    /// Texto completo del profiler
    static std::string generateReport() {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2);
        oss << "=== PROFILER ===\n";
        oss << "Frame: avg=" << s_frameMetric.avgMs << "ms"
            << " max=" << s_frameMetric.maxMs << "ms"
            << " frames=" << s_frameMetric.calls << "\n";
        oss << "Draw calls: " << s_drawCalls << "\n";
        oss << "Memory: " << (s_memoryBytes / 1024) << " KB\n";
        for (uint32_t i = 0; i < MAX_SECTIONS; i++) {
            if (s_sectionActive[i]) {
                oss << "  " << getSectionName(i) << ": "
                    << s_sectionMetrics[i].avgMs << "ms"
                    << " (max " << s_sectionMetrics[i].maxMs << "ms)\n";
            }
        }
        return oss.str();
    }

private:
    /// Map nombre → ID (lazy, solo para backward compat)
    static uint32_t getOrCreateId(std::string_view name) {
        for (uint32_t i = 0; i < s_nameCount; i++) {
            if (s_sectionNames[i] == name) return i;
        }
        if (s_nameCount < MAX_SECTIONS) {
            s_sectionNames[s_nameCount] = std::string(name);
            return s_nameCount++;
        }
        return 0; // Fallback to first section
    }

    static std::string getSectionName(uint32_t id) {
        static const char* defaults[] = {
            "Physics", "Collision", "Render", "Input",
            "Particles", "Events", "Sleep", "Custom0",
            "S8", "S9", "S10", "S11", "S12", "S13", "S14", "S15"
        };
        if (id < s_nameCount && !s_sectionNames[id].empty()) return s_sectionNames[id];
        return defaults[id < MAX_SECTIONS ? id : 0];
    }

    // ── Data ───────────────────────────────────────────────────

    // Section timers — fixed array, O(1) access
    inline static std::array<Clock::time_point, MAX_SECTIONS> s_sectionTimers = {};
    inline static std::array<Metric, MAX_SECTIONS>            s_sectionMetrics{};
    inline static std::array<bool, MAX_SECTIONS>              s_sectionActive = {};

    // Frame data
    inline static Metric            s_frameMetric = {};
    inline static Clock::time_point s_frameStart;
    inline static int               s_drawCalls = 0;
    inline static size_t            s_memoryBytes = 0;

    // Frame history ring buffer
    inline static std::array<float, HISTORY_SIZE> s_frameHistory = {};
    inline static int s_historyIndex = 0;
    inline static int s_historyCount = 0;

    // Per-section history — fixed array of ring buffers
    inline static std::array<std::array<float, HISTORY_SIZE>, MAX_SECTIONS> s_sectionHistory = {};
    inline static std::array<int, MAX_SECTIONS> s_sectionHistIdx = {};

    // Name mapping (backward compat)
    inline static std::array<std::string, MAX_SECTIONS> s_sectionNames = {};
    inline static uint32_t s_nameCount = 0;
};

/// ScopedTimer — RAII profiler guard. Automatically calls begin/end.
///
/// Usage:
///   {
///       ScopedTimer timer(Profiler::SECTION_PHYSICS);
///       physicsWorld.step(dt);
///   }  // Automatically calls Profiler::end() on scope exit
///
class ScopedTimer {
public:
    explicit ScopedTimer(uint32_t sectionId) : m_id(sectionId) {
        Profiler::begin(m_id);
    }
    ~ScopedTimer() {
        Profiler::end(m_id);
    }
    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;
private:
    uint32_t m_id;
};

} // namespace core
} // namespace engine

