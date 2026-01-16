#include "materials/Material.h"
#include "core/ResourceManager.h"
#include "core/RenderContext.h"
#include "core/Camera.h"
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

void Material::setRefractionIor(float value) {
    m_refractionIor = value;
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
        prog->setUniform("numShadowLights", context.numShadowMaps);
        
        GLint shadowsLoc = glGetUniformLocation(prog->getProgramId(), "shadowsEnabled");
        if (shadowsLoc != -1) {
            glUniform1i(shadowsLoc, context.shadowsEnabled ? 1 : 0);
        }
        
        prog->setUniform("viewPos", context.viewPosition);

        prog->setUniform("shadowBias", Shadow::getBias());
        prog->setUniform("shadowSoftness", Shadow::getSoftness());
        prog->setUniform("shadowMapSize", static_cast<float>(Shadow::getMapSize()));

        prog->setUniform("useCascades", context.useCascades ? 1 : 0);
        prog->setUniform("cascadeCount", context.cascadeCount);
        prog->setUniform("debugCascades", context.debugCascades ? 1 : 0);
        GLint splitsLoc = glGetUniformLocation(prog->getProgramId(), "cascadeSplits");
        if (splitsLoc != -1) {
            glUniform1fv(splitsLoc, 4, context.cascadeSplits.data());
        }
        GLint castsLoc = glGetUniformLocation(prog->getProgramId(), "lightCastsShadow");
        if (castsLoc != -1) {
            glUniform1iv(castsLoc, 4, context.lightCastsShadow.data());
        }
        GLint mapIndexLoc = glGetUniformLocation(prog->getProgramId(), "lightShadowMapIndex");
        if (mapIndexLoc != -1) {
            glUniform1iv(mapIndexLoc, 4, context.lightShadowMapIndex.data());
        }

        prog->setUniform("iblEnabled", context.iblEnabled ? 1 : 0);
        prog->setUniform("iblIntensity", context.iblIntensity);
        if (context.iblIrradianceMap) {
            glActiveTexture(GL_TEXTURE12);
            glBindTexture(GL_TEXTURE_CUBE_MAP, context.iblIrradianceMap);
            prog->setUniform("irradianceMap", 12);
        }
        if (context.iblPrefilterMap) {
            glActiveTexture(GL_TEXTURE13);
            glBindTexture(GL_TEXTURE_CUBE_MAP, context.iblPrefilterMap);
            prog->setUniform("prefilterMap", 13);
        }
        if (context.iblBrdfLut) {
            glActiveTexture(GL_TEXTURE14);
            glBindTexture(GL_TEXTURE_2D, context.iblBrdfLut);
            prog->setUniform("brdfLUT", 14);
        }
        prog->setUniform("envMapIntensity", context.envMapIntensity);
        if (context.envMapTextureId) {
            glActiveTexture(GL_TEXTURE15);
            glBindTexture(GL_TEXTURE_2D, context.envMapTextureId);
            prog->setUniform("envMap", 15);
        }

        if (m_shaderName == "Refraction") {
            prog->setUniform("ior", m_refractionIor);
            prog->setUniform("cameraPos", context.viewPosition);
            prog->setUniform("lightWorldPos", context.mainLightPosition);
            prog->setUniform("lightColor", context.mainLightColor);
            if (context.camera) {
                prog->setUniform("view", context.camera->getViewMatrix());
            }
            GLint screenLoc = glGetUniformLocation(prog->getProgramId(), "screenSize");
            if (screenLoc != -1) {
                glUniform2f(screenLoc,
                           static_cast<float>(context.viewportWidth),
                           static_cast<float>(context.viewportHeight));
            }
            if (context.refractionSourceTexture) {
                glActiveTexture(GL_TEXTURE6);
                glBindTexture(GL_TEXTURE_2D, context.refractionSourceTexture);
                prog->setUniform("texture_diffuse", 6);
            }
        }
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
