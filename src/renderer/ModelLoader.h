#pragma once

#include <glad/gl.h>
#include "cgltf.h"
#include "MeshPrimitives.h"
#include "Material.h"
#include "Texture2D.h"
#include <vector>

namespace engine {
namespace renderer {

struct LoadedModel {
    struct MeshEntry {
        Mesh3D mesh;
        int materialIndex = -1;
        math::Vector3D position;
        math::Vector3D scale = math::Vector3D(1, 1, 1);
    };
    std::vector<MeshEntry> meshEntries;
    std::vector<Material>  materials;
    std::vector<Texture2D> textures;
    bool isValid() const { return !meshEntries.empty(); }
};

namespace ModelLoader {
    const float* getBufferFloat(const cgltf_accessor* accessor, cgltf_size index);
    uint32_t     getIndex(const cgltf_accessor* accessor, cgltf_size index);
    GLuint       loadGLTFTexture(const cgltf_image* image, const char* basePath);
    bool         load(const char* filepath, LoadedModel& out);
} // namespace ModelLoader

} // namespace renderer
} // namespace engine
