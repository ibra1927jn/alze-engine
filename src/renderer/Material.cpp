#include "Material.h"
#include "ShaderProgram.h"
#include "Texture2D.h"

namespace engine {
namespace renderer {

void Material::apply(const ShaderProgram& shader) const {
    // PBR uniforms
    shader.setVec3("uAlbedo", albedoColor);
    shader.setFloat("uMetallic", metallic);
    shader.setFloat("uRoughness", roughness);
    shader.setFloat("uAo", ao);

    // Albedo texture
    if (albedoTexture && albedoTexture->isValid()) {
        albedoTexture->bind(0);
        shader.setInt("uAlbedoTex", 0);
        shader.setInt("uUseAlbedoTex", 1);
    } else {
        shader.setInt("uUseAlbedoTex", 0);
    }

    // Normal map
    if (normalMap && normalMap->isValid()) {
        normalMap->bind(1);
        shader.setInt("uNormalMap", 1);
        shader.setInt("uUseNormalMap", 1);
    } else {
        shader.setInt("uUseNormalMap", 0);
    }
}

} // namespace renderer
} // namespace engine
