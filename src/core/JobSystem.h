#pragma once

#include <vector>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace engine {
namespace core {

/// JobSystem — Thread pool para paralelizar el ECS.
///
/// Crea N-1 worker threads (donde N = núcleos del CPU).
/// Los workers duermen hasta que se encolan jobs.
///
/// Uso:
///   JobSystem jobs;
///   jobs.init();
///   jobs.parallel_for(0, 10000, 64, [](int start, int end) {
///       // Procesar entidades [start, end)
///   });
///   jobs.shutdown();
///
/// Thread safety:
///   - Los jobs NO deben mutar datos compartidos
///   - PhysicsSystem: integración es independiente por entidad → parallelizable
///   - CollisionSystem: resolución muta posiciones → NO parallelizable
///
class JobSystem {
public:
    ~JobSystem() { shutdown(); }

    /// Inicializar con N-1 workers (mínimo 1)
    void init(int numWorkers = -1) {
        if (m_running) return;

        if (numWorkers < 0) {
            numWorkers = static_cast<int>(std::thread::hardware_concurrency()) - 1;
        }
        if (numWorkers < 1) numWorkers = 1;

        m_running = true;
        m_numWorkers = numWorkers;

        m_workers.reserve(numWorkers);
        for (int i = 0; i < numWorkers; i++) {
            m_workers.emplace_back([this]() { workerLoop(); });
        }
    }

    /// Apagar todos los workers
    void shutdown() {
        if (!m_running) return;
        m_running = false;
        m_cv.notify_all();
        for (auto& w : m_workers) {
            if (w.joinable()) w.join();
        }
        m_workers.clear();
    }

    /// Paralelizar un bucle for: divide [start, end) en chunks
    /// y ejecuta cada chunk en un worker thread.
    /// BLOQUEA hasta que todos los chunks completen.
    void parallel_for(int start, int end, int chunkSize,
                      std::function<void(int, int)> body) {
        if (end <= start) return;

        // Si pocos elementos, ejecutar en thread principal
        if (end - start <= chunkSize || !m_running) {
            body(start, end);
            return;
        }

        // Contar chunks sin allocar
        int numChunks = (end - start + chunkSize - 1) / chunkSize;

        // Si solo 1 chunk, ejecutar directamente
        if (numChunks <= 1) {
            body(start, end);
            return;
        }

        // Encolar chunks como jobs (sin vector intermedio)
        m_pendingJobs.store(numChunks);

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (int i = start; i < end; i += chunkSize) {
                int chunkEnd = i + chunkSize;
                if (chunkEnd > end) chunkEnd = end;
                m_jobs.push_back([this, &body, i, chunkEnd]() {
                    body(i, chunkEnd);
                    if (m_pendingJobs.fetch_sub(1) == 1) {
                        // Ultimo job: notificar al hilo principal
                        m_doneCv.notify_one();
                    }
                });
            }
        }
        m_cv.notify_all();

        // Esperar a que todos los chunks terminen
        std::unique_lock<std::mutex> lock(m_doneMutex);
        m_doneCv.wait(lock, [this]() { return m_pendingJobs.load() == 0; });
    }

    // ── Info ────────────────────────────────────────────────────
    int getNumWorkers() const { return m_numWorkers; }
    bool isRunning() const { return m_running; }

private:
    void workerLoop() {
        while (m_running) {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cv.wait(lock, [this]() {
                    return !m_jobs.empty() || !m_running;
                });
                if (!m_running && m_jobs.empty()) return;
                if (m_jobs.empty()) continue;
                job = m_jobs.back();
                m_jobs.pop_back();
            }
            job();
        }
    }

    std::vector<std::thread> m_workers;
    std::vector<std::function<void()>> m_jobs;
    std::mutex m_mutex;
    std::condition_variable m_cv;

    std::mutex m_doneMutex;
    std::condition_variable m_doneCv;
    std::atomic<int> m_pendingJobs{0};

    int m_numWorkers = 0;
    std::atomic<bool> m_running{false};
};

} // namespace core
} // namespace engine
