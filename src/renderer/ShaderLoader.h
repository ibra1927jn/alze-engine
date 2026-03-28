#pragma once

// ShaderLoader — Utilidad para cargar shaders GLSL desde archivos.
// Lee archivos de texto plano y devuelve su contenido como string.
// Fallback: si el archivo no existe, retorna nullptr para que el caller
// pueda usar los shaders embebidos como respaldo.

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

namespace engine {
namespace renderer {

class ShaderLoader {
public:
    /// Carga un archivo GLSL y retorna su contenido como string.
    /// Retorna string vacio si el archivo no se puede leer.
    static std::string loadFromFile(const char* filePath) {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            std::cerr << "[ShaderLoader] No se pudo abrir: " << filePath << std::endl;
            return "";
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
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
            std::cerr << "[ShaderLoader] Fallo al cargar par de shaders: " << name << std::endl;
            return false;
        }

        std::cout << "[ShaderLoader] Cargados: " << vertPath << " + " << fragPath << std::endl;
        return true;
    }
};

} // namespace renderer
} // namespace engine
