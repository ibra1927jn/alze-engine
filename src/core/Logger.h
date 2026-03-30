#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <ctime>
#include <mutex>
#include <atomic>

namespace engine {
namespace core {

/// Logger — Sistema de logging con salida a consola + archivo.
///
/// Niveles: TRACE < DEBUG < INFO < WARN < ERROR
/// Solo se imprimen mensajes con nivel >= al nivel mínimo configurado.
///
/// Uso:
///   Logger::setFile("engine.log");           // Activar log a archivo
///   Logger::info("Motor", "Ventana creada");
///   Logger::warn("Physics", "dt alto: " + std::to_string(dt));
///   Logger::setMinLevel(Logger::Level::WARN);
///   Logger::flush();                          // Forzar escritura
///
class Logger {
public:
    enum class Level {
        TRACE = 0,
        DEBUG = 1,
        INFO  = 2,
        WARN  = 3,
        ERR   = 4,
        SILENT = 5
    };

    /// Configurar nivel mínimo (por defecto: INFO)
    static void setMinLevel(Level level) { s_minLevel = level; }
    static Level getMinLevel() { return s_minLevel; }

    /// Activar salida a archivo (además de consola)
    static void setFile(const std::string& path) {
        std::lock_guard<std::mutex> lock(s_mutex); // Proteger acceso al archivo
        if (s_file.is_open()) s_file.close();
        s_file.open(path, std::ios::out | std::ios::trunc);
        s_fileEnabled = s_file.is_open();
        if (s_fileEnabled) {
            s_file << "=== Engine Log Started ===" << std::endl;
            s_filePath = path;
        }
    }

    /// Cerrar archivo de log
    static void closeFile() {
        std::lock_guard<std::mutex> lock(s_mutex); // Proteger acceso al archivo
        if (s_file.is_open()) {
            s_file << "=== Engine Log Ended ===" << std::endl;
            s_file.close();
        }
        s_fileEnabled = false;
    }

    /// Forzar escritura a disco
    static void flush() {
        std::lock_guard<std::mutex> lock(s_mutex); // Proteger acceso al archivo
        if (s_file.is_open()) s_file.flush();
    }

    /// ¿Está el archivo de log activo?
    static bool isFileEnabled() { return s_fileEnabled; }
    static const std::string& getFilePath() { return s_filePath; }

    /// Logging con nivel explícito
    static void log(Level level, const std::string& tag, const std::string& msg) {
        if (level < s_minLevel) return;

        const char* levelStr = "";
        const char* colorCode = "";
        switch (level) {
            case Level::TRACE: levelStr = "TRACE"; colorCode = "\033[90m"; break;
            case Level::DEBUG: levelStr = "DEBUG"; colorCode = "\033[36m"; break;
            case Level::INFO:  levelStr = "INFO "; colorCode = "\033[32m"; break;
            case Level::WARN:  levelStr = "WARN "; colorCode = "\033[33m"; break;
            case Level::ERR:   levelStr = "ERROR"; colorCode = "\033[31m"; break;
            default: break;
        }

        // Timestamp
        char timeBuf[16];
        time_t now = time(nullptr);
        strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", localtime(&now));

        // Proteger escritura a consola y archivo contra data race
        std::lock_guard<std::mutex> lock(s_mutex);

        // Console output (with color)
        std::cout << colorCode
                  << "[" << timeBuf << "] "
                  << "[" << levelStr << "] "
                  << "[" << tag << "] "
                  << msg
                  << "\033[0m"
                  << std::endl;

        // File output (no color codes)
        if (s_fileEnabled && s_file.is_open()) {
            s_file << "[" << timeBuf << "] "
                   << "[" << levelStr << "] "
                   << "[" << tag << "] "
                   << msg << "\n";
            s_lineCount++;
            // Auto-flush every 100 lines
            if (s_lineCount % 100 == 0) s_file.flush();
        }
    }

    // ── Atajos por nivel ───────────────────────────────────────
    static void trace(const std::string& tag, const std::string& msg) { log(Level::TRACE, tag, msg); }
    static void debug(const std::string& tag, const std::string& msg) { log(Level::DEBUG, tag, msg); }
    static void info(const std::string& tag, const std::string& msg)  { log(Level::INFO, tag, msg); }
    static void warn(const std::string& tag, const std::string& msg)  { log(Level::WARN, tag, msg); }
    static void error(const std::string& tag, const std::string& msg) { log(Level::ERR, tag, msg); }

    /// Obtener contador de líneas escritas
    static int getLineCount() { return s_lineCount; }

private:
    static inline std::atomic<Level> s_minLevel{Level::INFO};
    static inline std::ofstream s_file;
    static inline bool s_fileEnabled = false;
    static inline std::string s_filePath;
    static inline int s_lineCount = 0;
    static inline std::mutex s_mutex;
};

} // namespace core
} // namespace engine
