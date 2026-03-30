#pragma once

// ShaderLoader — Utilidad para cargar shaders GLSL desde archivos.
// Lee archivos de texto plano y devuelve su contenido como string.
// Fallback: si el archivo no existe, retorna nullptr para que el caller
// pueda usar los shaders embebidos como respaldo.

#include <string>
#include <cstdio>
#include "core/Logger.h"

namespace engine {
namespace renderer {

class ShaderLoader {
public:
    /// Carga un archivo GLSL y retorna su contenido como string.
    /// Retorna string vacio si el archivo no se puede leer.
    static std::string loadFromFile(const char* filePath) {
        std::FILE* file = std::fopen(filePath, "r");
        if (!file) {
            core::Logger::warn("ShaderLoader", std::string("No se pudo abrir: ") + filePath);
            return "";
        }
        std::fseek(file, 0, SEEK_END);
        long size = std::ftell(file);
        std::fseek(file, 0, SEEK_SET);
        std::string content(static_cast<size_t>(size), '\0');
        std::fread(&content[0], 1, static_cast<size_t>(size), file);
        std::fclose(file);
        return content;
    }

    /// Carga un par vertex+fragment desde la carpeta base.
    /// Ejemplo: loadPair("assets/shaders/", "pbr") carga pbr.vert y pbr.frag
    /// Retorna true si ambos archivos se cargaron correctamente.
    static bool loadPair(const char* basePath, const char* name,
                         std::string& outVert, std::string& outFrag) {
        std::string base(basePath);
        std::string vertPath = base + name + ".vert";
        std::string fragPath = base + name + ".frag";

        outVert = loadFromFile(vertPath.c_str());
        outFrag = loadFromFile(fragPath.c_str());

        if (outVert.empty() || outFrag.empty()) {
            core::Logger::error("ShaderLoader", std::string("Fallo al cargar par de shaders: ") + name);
            return false;
        }

        core::Logger::info("ShaderLoader", "Cargados: " + vertPath + " + " + fragPath);
        return true;
    }
};

} // namespace renderer
} // namespace engine
