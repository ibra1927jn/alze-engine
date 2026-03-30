#pragma once

#include <cstdio>
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
        if (s_file) { std::fclose(s_file); s_file = nullptr; }
        s_file = std::fopen(path.c_str(), "w");
        s_fileEnabled = (s_file != nullptr);
        if (s_fileEnabled) {
            std::fputs("=== Engine Log Started ===\n", s_file);
            std::fflush(s_file);
            s_filePath = path;
        }
    }

    /// Cerrar archivo de log
    static void closeFile() {
        std::lock_guard<std::mutex> lock(s_mutex); // Proteger acceso al archivo
        if (s_file) {
            std::fputs("=== Engine Log Ended ===\n", s_file);
            std::fclose(s_file);
            s_file = nullptr;
        }
        s_fileEnabled = false;
    }

    /// Forzar escritura a disco
    static void flush() {
        std::lock_guard<std::mutex> lock(s_mutex); // Proteger acceso al archivo
        if (s_file) std::fflush(s_file);
    }

    /// ¿Está el archivo de log activo?
    static bool isFileEnabled() {
        std::lock_guard<std::mutex> lock(s_mutex);
        return s_fileEnabled;
    }
    static std::string getFilePath() {
        std::lock_guard<std::mutex> lock(s_mutex);
        return s_filePath;
    }

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

        // Timestamp (localtime_r es thread-safe)
        char timeBuf[16];
        time_t now = time(nullptr);
        struct tm tmBuf;
        localtime_r(&now, &tmBuf);
        strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &tmBuf);

        // Proteger escritura a consola y archivo contra data race
        std::lock_guard<std::mutex> lock(s_mutex);

        // Console output (with color)
        std::fprintf(stdout, "%s[%s] [%s] [%s] %s\033[0m\n",
                     colorCode, timeBuf, levelStr, tag.c_str(), msg.c_str());

        // File output (no color codes)
        if (s_fileEnabled && s_file) {
            std::fprintf(s_file, "[%s] [%s] [%s] %s\n",
                         timeBuf, levelStr, tag.c_str(), msg.c_str());
            s_lineCount++;
            // Auto-flush every 100 lines
            if (s_lineCount % 100 == 0) std::fflush(s_file);
        }
    }

    // ── Atajos por nivel ───────────────────────────────────────
    static void trace(const std::string& tag, const std::string& msg) { log(Level::TRACE, tag, msg); }
    static void debug(const std::string& tag, const std::string& msg) { log(Level::DEBUG, tag, msg); }
    static void info(const std::string& tag, const std::string& msg)  { log(Level::INFO, tag, msg); }
    static void warn(const std::string& tag, const std::string& msg)  { log(Level::WARN, tag, msg); }
    static void error(const std::string& tag, const std::string& msg) { log(Level::ERR, tag, msg); }

    /// Obtener contador de líneas escritas
    static int getLineCount() {
        std::lock_guard<std::mutex> lock(s_mutex);
        return s_lineCount;
    }

private:
    static inline std::atomic<Level> s_minLevel{Level::INFO};
    static inline std::FILE* s_file = nullptr;
    static inline bool s_fileEnabled = false;
    static inline std::string s_filePath;
    static inline int s_lineCount = 0;
    static inline std::mutex s_mutex;
};

} // namespace core
} // namespace engine
