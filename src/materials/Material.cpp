#include "materials/Material.h"
#include "core/ResourceManager.h"
#include "rendering/UBOManager.h"
#include <ShaderLib.h>
#include <functional>
#include <iostream>

namespace gfx {

Material::Material(const std::string& shaderName) 
    : m_shaderName(shaderName) {
    
    // Initialize UBO data with defaults
    m_uboData.ambient = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f);
    m_uboData.diffuse = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
    m_uboData.specular = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    m_uboData.shininess = 32.0f;
    m_uboData.metallic = 0.0f;
    m_uboData.roughness = 0.5f;
    m_uboData.useTexture = 0;
    
    generateMaterialID();
}

void Material::setAmbient(const glm::vec3& color) {
    m_uboData.ambient = glm::vec4(color, 1.0f);
}

void Material::setDiffuse(const glm::vec3& color) {
    m_uboData.diffuse = glm::vec4(color, 1.0f);
}

void Material::setSpecular(const glm::vec3& color) {
    m_uboData.specular = glm::vec4(color, 1.0f);
}

void Material::setShininess(float value) {
    m_uboData.shininess = value;
}

void Material::setMetallic(float value) {
    m_uboData.metallic = value;
}

void Material::setRoughness(float value) {
    m_uboData.roughness = value;
}

void Material::setTexture(const std::string& name, TextureHandle texture) {
    m_textures[name] = texture;
    m_uboData.useTexture = texture ? 1 : 0;
}

void Material::bind() {
    // Activate shader
    ShaderLib::instance()->use(m_shaderName);
    
    // Upload material UBO
    UBOManager::instance()->updateUBO("Material", &m_uboData, sizeof(MaterialUBO));
    
    // Bind textures
    int slot = 0;
    for (const auto& pair : m_textures) {
        if (pair.second) {
            (*pair.second).bind(slot);
            
            // Set texture sampler uniform (shader expects these names)
            auto* prog = (*ShaderLib::instance())[m_shaderName];
            if (prog) {
                prog->setUniform(pair.first, slot);
            }
            slot++;
        }
    }
}

void Material::unbind() {
    // Unbind textures
    for (const auto& pair : m_textures) {
        if (pair.second) {
            (*pair.second).unbind();
        }
    }
}

void Material::generateMaterialID() {
    // Simple hash from shader name
    std::hash<std::string> hasher;
    m_materialID = static_cast<MaterialID>(hasher(m_shaderName) & 0x00FFFFFF);
}

// ===== Factory Methods =====

Material* Material::createPhong(const glm::vec3& diffuse) {
    Material* mat = new Material("PhongUBO");  // Use UBO-based shader
    mat->setDiffuse(diffuse);
    mat->setAmbient(diffuse * 0.3f);
    mat->setSpecular(glm::vec3(0.5f));
    mat->setShininess(32.0f);
    return mat;
}

Material* Material::createSilk(const glm::vec3& diffuse) {
    Material* mat = new Material("SilkUBO");  // Use UBO-based shader
    mat->setDiffuse(diffuse);
    mat->setAmbient(diffuse * 0.2f);
    mat->setSpecular(glm::vec3(0.8f));
    mat->setShininess(64.0f);
    return mat;
}

Material* Material::createSilkPBR(const glm::vec3& diffuse) {
    Material* mat = new Material("SilkPBR_UBO");  // Use UBO-based shader
    mat->setDiffuse(diffuse);
    mat->setAmbient(diffuse * 0.2f);
    mat->setMetallic(0.1f);
    mat->setRoughness(0.3f);
    return mat;
}

Material* Material::createShadow() {
    Material* mat = new Material("Shadow");
    // Shadow material has minimal properties (depth-only)
    mat->setDiffuse(glm::vec3(0.0f));
    return mat;
}

} // namespace gfx
