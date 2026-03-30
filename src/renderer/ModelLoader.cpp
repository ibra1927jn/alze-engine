#include "ModelLoader.h"
#include "ImageDecoder.h"
#include "core/Logger.h"
#include <glad/gl.h>

namespace engine {
namespace renderer {
namespace ModelLoader {

const float* getBufferFloat(const cgltf_accessor* accessor, cgltf_size index) {
    const uint8_t* base = static_cast<const uint8_t*>(accessor->buffer_view->buffer->data);
    base += accessor->buffer_view->offset + accessor->offset;
    base += index * accessor->stride;
    return reinterpret_cast<const float*>(base);
}

uint32_t getIndex(const cgltf_accessor* accessor, cgltf_size index) {
    const uint8_t* base = static_cast<const uint8_t*>(accessor->buffer_view->buffer->data);
    base += accessor->buffer_view->offset + accessor->offset;
    if (accessor->component_type == cgltf_component_type_r_16u)
        return *(reinterpret_cast<const uint16_t*>(base + index * 2));
    if (accessor->component_type == cgltf_component_type_r_32u)
        return *(reinterpret_cast<const uint32_t*>(base + index * 4));
    if (accessor->component_type == cgltf_component_type_r_8u)
        return *(base + index);
    return 0;
}

GLuint loadGLTFTexture(const cgltf_image* image, const char* basePath) {
    int w, h, channels;
    unsigned char* data = nullptr;

    if (image->buffer_view) {
        const uint8_t* bufData = static_cast<const uint8_t*>(image->buffer_view->buffer->data);
        bufData += image->buffer_view->offset;
        int bufLen = static_cast<int>(image->buffer_view->size);
        data = stbi_load_from_memory(bufData, bufLen, &w, &h, &channels, 0);
    } else if (image->uri) {
        std::string fullPath = std::string(basePath) + "/" + image->uri;
        data = stbi_load(fullPath.c_str(), &w, &h, &channels, 0);
    }

    if (!data) {
        core::Logger::error("ModelLoader", std::string("Failed to load texture: ") + (image->name ? image->name : "unnamed"));
        return 0;
    }

    GLenum format = GL_RGB, internalFormat = GL_RGB8;
    if (channels == 4) { format = GL_RGBA; internalFormat = GL_RGBA8; }
    else if (channels == 1) { format = GL_RED; internalFormat = GL_R8; }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, w, h, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    float maxAniso = 0;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso);
    if (maxAniso > 0)
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, maxAniso > 16.0f ? 16.0f : maxAniso);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);
    core::Logger::info("ModelLoader", "Loaded texture: " + std::to_string(w) + "x" + std::to_string(h) + " (" + std::to_string(channels) + "ch)");
    return tex;
}

bool load(const char* filepath, LoadedModel& out) {
    core::Logger::info("ModelLoader", std::string("Loading: ") + filepath);

    cgltf_options options = {};
    cgltf_data* data = nullptr;
    cgltf_result result = cgltf_parse_file(&options, filepath, &data);
    if (result != cgltf_result_success) {
        core::Logger::error("ModelLoader", "Parse failed: " + std::to_string(static_cast<int>(result)));
        return false;
    }
    result = cgltf_load_buffers(&options, data, filepath);
    if (result != cgltf_result_success) {
        core::Logger::error("ModelLoader", "Buffer load failed");
        cgltf_free(data); return false;
    }

    std::string basePath = filepath;
    size_t lastSlash = basePath.find_last_of("/\\");
    basePath = (lastSlash != std::string::npos) ? basePath.substr(0, lastSlash) : ".";

    // Load textures
    std::vector<int> imageToTexIndex(data->images_count, -1);
    for (cgltf_size i = 0; i < data->images_count; i++) {
        GLuint texHandle = loadGLTFTexture(&data->images[i], basePath.c_str());
        if (texHandle != 0) {
            imageToTexIndex[i] = static_cast<int>(out.textures.size());
            Texture2D tex; tex.wrapHandle(texHandle);
            out.textures.push_back(std::move(tex));
        }
    }

    // Load materials
    for (cgltf_size i = 0; i < data->materials_count; i++) {
        const cgltf_material& gltfMat = data->materials[i];
        Material mat;
        if (gltfMat.has_pbr_metallic_roughness) {
            const auto& pbr = gltfMat.pbr_metallic_roughness;
            mat.albedoColor = math::Vector3D(pbr.base_color_factor[0],
                                             pbr.base_color_factor[1],
                                             pbr.base_color_factor[2]);
            mat.metallic  = pbr.metallic_factor;
            mat.roughness = pbr.roughness_factor;
            if (pbr.base_color_texture.texture && pbr.base_color_texture.texture->image) {
                cgltf_size imgIdx = pbr.base_color_texture.texture->image - data->images;
                if (imgIdx < data->images_count && imageToTexIndex[imgIdx] >= 0)
                    mat.albedoTexture = &out.textures[imageToTexIndex[imgIdx]];
            }
        }
        if (gltfMat.normal_texture.texture && gltfMat.normal_texture.texture->image) {
            cgltf_size imgIdx = gltfMat.normal_texture.texture->image - data->images;
            if (imgIdx < data->images_count && imageToTexIndex[imgIdx] >= 0)
                mat.normalMap = &out.textures[imageToTexIndex[imgIdx]];
        }
        out.materials.push_back(mat);
        if (gltfMat.name)
            core::Logger::info("ModelLoader", std::string("Material: ") + gltfMat.name
                      + " (metallic=" + std::to_string(mat.metallic) + " roughness=" + std::to_string(mat.roughness) + ")");
    }

    // Load meshes
    for (cgltf_size m = 0; m < data->meshes_count; m++) {
        const cgltf_mesh& gltfMesh = data->meshes[m];
        for (cgltf_size p = 0; p < gltfMesh.primitives_count; p++) {
            const cgltf_primitive& prim = gltfMesh.primitives[p];
            if (prim.type != cgltf_primitive_type_triangles) continue;

            const cgltf_accessor* posAccessor  = nullptr;
            const cgltf_accessor* normAccessor = nullptr;
            const cgltf_accessor* uvAccessor   = nullptr;
            for (cgltf_size a = 0; a < prim.attributes_count; a++) {
                if (prim.attributes[a].type == cgltf_attribute_type_position)
                    posAccessor = prim.attributes[a].data;
                else if (prim.attributes[a].type == cgltf_attribute_type_normal)
                    normAccessor = prim.attributes[a].data;
                else if (prim.attributes[a].type == cgltf_attribute_type_texcoord)
                    uvAccessor = prim.attributes[a].data;
            }
            if (!posAccessor) continue;

            cgltf_size vertCount = posAccessor->count;
            std::vector<float> vertices(vertCount * 8);
            for (cgltf_size v = 0; v < vertCount; v++) {
                const float* pos = getBufferFloat(posAccessor, v);
                vertices[v*8+0]=pos[0]; vertices[v*8+1]=pos[1]; vertices[v*8+2]=pos[2];
                if (normAccessor) {
                    const float* norm = getBufferFloat(normAccessor, v);
                    vertices[v*8+3]=norm[0]; vertices[v*8+4]=norm[1]; vertices[v*8+5]=norm[2];
                } else { vertices[v*8+3]=0; vertices[v*8+4]=1; vertices[v*8+5]=0; }
                if (uvAccessor) {
                    const float* uv = getBufferFloat(uvAccessor, v);
                    vertices[v*8+6]=uv[0]; vertices[v*8+7]=uv[1];
                } else { vertices[v*8+6]=0; vertices[v*8+7]=0; }
            }

            std::vector<uint32_t> indices;
            if (prim.indices) {
                indices.resize(prim.indices->count);
                for (cgltf_size i = 0; i < prim.indices->count; i++)
                    indices[i] = getIndex(prim.indices, i);
            } else {
                indices.resize(vertCount);
                for (cgltf_size i = 0; i < vertCount; i++) indices[i] = static_cast<uint32_t>(i);
            }

            LoadedModel::MeshEntry entry;
            entry.mesh.create(vertices.data(), vertices.size(), indices.data(), indices.size());
            if (prim.material)
                entry.materialIndex = static_cast<int>(prim.material - data->materials);

            core::Logger::info("ModelLoader", std::string("Mesh: ") + (gltfMesh.name ? gltfMesh.name : "unnamed")
                      + " [" + std::to_string(vertCount) + " verts, " + std::to_string(indices.size()) + " indices]");
            out.meshEntries.push_back(std::move(entry));
        }
    }

    cgltf_free(data);
    core::Logger::info("ModelLoader", "Loaded! " + std::to_string(out.meshEntries.size()) + " meshes, "
              + std::to_string(out.materials.size()) + " materials, " + std::to_string(out.textures.size()) + " textures");
    return out.isValid();
}

} // namespace ModelLoader
} // namespace renderer
} // namespace engine
