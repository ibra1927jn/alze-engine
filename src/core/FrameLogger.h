#pragma once

#include <fstream>
#include <string>
#include <cstdio>
#include <chrono>

namespace engine {
namespace core {

/// FrameLogger — Graba datos de cada frame a CSV para análisis.
///
/// Cada línea contiene: frame, dt, fps, entities, collisions,
/// particles, physics_ms, collision_ms, frame_ms, player_x, player_y, player_vx, player_vy
///
/// Uso:
///   FrameLogger::start("session_001.csv");
///   // Cada frame:
///   FrameLogger::logFrame(data);
///   // Al final:
///   FrameLogger::stop();
///
/// Importar en Excel/Python para gráficas de rendimiento.
///
class FrameLogger {
public:
    struct FrameData {
        int frame = 0;
        float dt = 0;
        float fps = 0;
        int entityCount = 0;
        int broadTests = 0;
        int narrowTests = 0;
        int collisionsResolved = 0;
        int particleCount = 0;
        float physicsMs = 0;
        float collisionMs = 0;
        float frameMs = 0;
        float playerX = 0, playerY = 0;
        float playerVx = 0, playerVy = 0;
        bool playerOnGround = false;
        float cameraZoom = 1.0f;
    };

    /// Iniciar grabación a CSV
    static void start(const std::string& path) {
        if (s_file.is_open()) s_file.close();
        s_file.open(path, std::ios::out | std::ios::trunc);
        s_active = s_file.is_open();
        s_frameCount = 0;
        s_path = path;

        if (s_active) {
            // CSV header
            s_file << "frame,dt,fps,entities,broad_tests,narrow_tests,"
                   << "collisions,particles,physics_ms,collision_ms,frame_ms,"
                   << "player_x,player_y,player_vx,player_vy,on_ground,zoom"
                   << "\n";
        }
    }

    /// Grabar un frame
    static void logFrame(const FrameData& d) {
        if (!s_active) return;

        char line[256];
        std::snprintf(line, sizeof(line),
            "%d,%.3f,%.1f,%d,%d,%d,%d,%d,%.3f,%.3f,%.3f,%.1f,%.1f,%.1f,%.1f,%d,%.2f\n",
            d.frame, d.dt, d.fps, d.entityCount, d.broadTests,
            d.narrowTests, d.collisionsResolved, d.particleCount,
            d.physicsMs, d.collisionMs, d.frameMs,
            d.playerX, d.playerY, d.playerVx, d.playerVy,
            d.playerOnGround ? 1 : 0, d.cameraZoom);
        s_file << line;

        s_frameCount++;
        // Flush every 60 frames (~1 second)
        if (s_frameCount % 60 == 0) s_file.flush();
    }

    /// Detener grabación
    static void stop() {
        if (s_file.is_open()) {
            s_file.flush();
            s_file.close();
        }
        s_active = false;
    }

    static bool isActive() { return s_active; }
    static int getFrameCount() { return s_frameCount; }
    static const std::string& getPath() { return s_path; }

private:
    static inline std::ofstream s_file;
    static inline bool s_active = false;
    static inline int s_frameCount = 0;
    static inline std::string s_path;
};

} // namespace core
} // namespace engine
