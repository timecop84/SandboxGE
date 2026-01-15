#include "materials/Material.h"
#include "core/ResourceManager.h"
#include "core/RenderContext.h"
#include "rendering/UBOManager.h"
#include "rendering/ShadowRenderer.h"
#include <utils/ShaderLib.h>
#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include <functional>
#include <iostream>
#include <memory>
#include <unordered_map>

namespace sandbox {

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

void Material::bind(const RenderContext& context) {
    auto* prog = (*ShaderLib::instance())[m_shaderName];
    if (!prog) {
        std::cerr << "Material::bind() - Failed to get shader program: " << m_shaderName << std::endl;
        return;
    }

    prog->use();

    UBOManager::instance()->updateUBO("Material", &m_uboData, sizeof(MaterialUBO));
    
    if (context.currentPass == RenderContext::Pass::SCENE) {
        GLint loc = glGetUniformLocation(prog->getProgramId(), "shadowMatrices");
        
        if (loc != -1) {
            glUniformMatrix4fv(loc, 4, GL_FALSE, glm::value_ptr(context.shadowMatrices[0]));
        }
        
        prog->setUniform("shadowMap0", 8);
        prog->setUniform("shadowMap1", 9);
        prog->setUniform("shadowMap2", 10);
        prog->setUniform("shadowMap3", 11);
        
        GLint shadowsLoc = glGetUniformLocation(prog->getProgramId(), "shadowsEnabled");
        if (shadowsLoc != -1) {
            glUniform1i(shadowsLoc, context.shadowsEnabled ? 1 : 0);
        }
        
        prog->setUniform("viewPos", context.viewPosition);

        prog->setUniform("shadowBias", Shadow::getBias());
        prog->setUniform("shadowSoftness", Shadow::getSoftness());
    }
    
    int slot = 0;
    for (const auto& pair : m_textures) {
        if (pair.second) {
            (*pair.second).bind(slot);
            prog->setUniform(pair.first, slot);
            slot++;
        }
    }
}

void Material::unbind() {
    for (const auto& pair : m_textures) {
        if (pair.second) {
            (*pair.second).unbind();
        }
    }
}

void Material::generateMaterialID() {
    std::hash<std::string> hasher;
    m_materialID = static_cast<MaterialID>(hasher(m_shaderName) & 0x00FFFFFF);
}

Material* Material::createPhong(const glm::vec3& diffuse) {
    return createPhongUnique(diffuse).release();
}

Material* Material::createSilk(const glm::vec3& diffuse) {
    return createSilkUnique(diffuse).release();
}

Material* Material::createSilkPBR(const glm::vec3& diffuse) {
    return createSilkPBRUnique(diffuse).release();
}

Material* Material::createShadow() {
    return createShadowUnique().release();
}

std::unique_ptr<Material> Material::createPhongUnique(const glm::vec3& diffuse) {
    auto mat = std::make_unique<Material>("PhongUBO");
    mat->setDiffuse(diffuse);
    mat->setAmbient(diffuse * 0.3f);
    mat->setSpecular(glm::vec3(0.5f));
    mat->setShininess(32.0f);
    return mat;
}

std::unique_ptr<Material> Material::createSilkUnique(const glm::vec3& diffuse) {
    auto mat = std::make_unique<Material>("SilkUBO");
    mat->setDiffuse(diffuse);
    mat->setAmbient(diffuse * 0.2f);
    mat->setSpecular(glm::vec3(0.8f));
    mat->setShininess(64.0f);
    return mat;
}

std::unique_ptr<Material> Material::createSilkPBRUnique(const glm::vec3& diffuse) {
    auto mat = std::make_unique<Material>("SilkPBR_UBO");
    mat->setDiffuse(diffuse);
    mat->setAmbient(diffuse * 0.2f);
    mat->setMetallic(0.1f);
    mat->setRoughness(0.3f);
    return mat;
}

std::unique_ptr<Material> Material::createShadowUnique() {
    auto mat = std::make_unique<Material>("Shadow");
    mat->setDiffuse(glm::vec3(0.0f));
    return mat;
}

} // namespace sandbox
