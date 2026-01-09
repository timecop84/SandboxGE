#include "Renderer.h"
#include "RenderSettings.h"
#include "Floor.h"
#include "SphereObstacle.h"
#include "ShadowRenderer.h"

#include <Camera.h>
#include <ShaderLib.h>
#include <TransformStack.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <GLFW/glfw3.h>

namespace Renderer {

void initGL() {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDisable(GL_CULL_FACE);
    glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
}

void setupLighting(Camera* camera, const gfx::RenderSettings& params) {
    ShaderLib* shader = ShaderLib::instance();
    glm::vec3 lightWorldPos(params.lightPosition[0],
                            params.lightPosition[1],
                            params.lightPosition[2]);
    glm::vec4 lightViewPos = camera->getViewMatrix() * glm::vec4(lightWorldPos, 1.0f);
    glm::vec3 camPos = camera->getEye();
    
    // Setup for Phong shader (must already be bound via use())
    ShaderLib::ProgramWrapper* phong = (*shader)["Phong"];
    if (phong) {
        phong->use();  // Ensure shader is active before setting uniforms
        phong->setUniform("light.position", lightViewPos);
        phong->setUniform("light.ambient", glm::vec4(params.lightAmbient[0], params.lightAmbient[1], params.lightAmbient[2], 1.0f));
        phong->setUniform("light.diffuse", glm::vec4(params.lightDiffuse[0], params.lightDiffuse[1], params.lightDiffuse[2], 1.0f));
        phong->setUniform("light.specular", glm::vec4(params.lightSpecular[0], params.lightSpecular[1], params.lightSpecular[2], 1.0f));
        phong->setUniform("light.constantAttenuation", 1.0f);
        phong->setUniform("light.linearAttenuation", 0.0f);
        phong->setUniform("light.quadraticAttenuation", 0.0f);
        phong->setUniform("viewerPos", camPos);
        phong->setUniform("Normalize", true);
        
        // Shadow uniforms
        GLuint programId = phong->getProgramId();
        struct ShadowUniformCache {
            GLuint programId = 0;
            bool initialized = false;
            GLint shadowEnabled = -1;
            GLint shadowBias = -1;
            GLint shadowSoftness = -1;
            GLint shadowMapSize = -1;
            GLint shadowStrength = -1;
            GLint lightSpaceMatrix = -1;
            GLint shadowMap = -1;
            GLint numShadowLights = -1;
            GLint lightIntensities[Shadow::MAX_SHADOW_LIGHTS] = {-1, -1, -1, -1};
            GLint lightSpaceMatrices[Shadow::MAX_SHADOW_LIGHTS] = {-1, -1, -1, -1};
            GLint shadowMaps[Shadow::MAX_SHADOW_LIGHTS] = {-1, -1, -1, -1};
        };
        static ShadowUniformCache cache;
        if (!cache.initialized || cache.programId != programId) {
            cache.programId = programId;
            cache.shadowEnabled = glGetUniformLocation(programId, "shadowEnabled");
            cache.shadowBias = glGetUniformLocation(programId, "shadowBias");
            cache.shadowSoftness = glGetUniformLocation(programId, "shadowSoftness");
            cache.shadowMapSize = glGetUniformLocation(programId, "shadowMapSize");
            cache.shadowStrength = glGetUniformLocation(programId, "shadowStrength");
            cache.lightSpaceMatrix = glGetUniformLocation(programId, "lightSpaceMatrix");
            cache.shadowMap = glGetUniformLocation(programId, "shadowMap");
            cache.numShadowLights = glGetUniformLocation(programId, "numShadowLights");
            for (int i = 0; i < Shadow::MAX_SHADOW_LIGHTS; ++i) {
                std::string idx = std::to_string(i);
                cache.lightIntensities[i] = glGetUniformLocation(programId, ("lightIntensities[" + idx + "]").c_str());
                cache.lightSpaceMatrices[i] = glGetUniformLocation(programId, ("lightSpaceMatrices[" + idx + "]").c_str());
                cache.shadowMaps[i] = glGetUniformLocation(programId, ("shadowMaps[" + idx + "]").c_str());
            }
            cache.initialized = true;
        }

        if (cache.shadowEnabled != -1) glUniform1i(cache.shadowEnabled, Shadow::isEnabled() ? 1 : 0);
        if (cache.shadowBias != -1) glUniform1f(cache.shadowBias, params.shadowBias);
        if (cache.shadowSoftness != -1) glUniform1f(cache.shadowSoftness, params.shadowSoftness);
        if (cache.shadowMapSize != -1) glUniform1f(cache.shadowMapSize, 4096.0f);
        if (cache.shadowStrength != -1) glUniform1f(cache.shadowStrength, 1.5f);
        if (cache.lightSpaceMatrix != -1) {
            glUniformMatrix4fv(cache.lightSpaceMatrix, 1, GL_FALSE,
                               glm::value_ptr(Shadow::getLightSpaceMatrix(0)));
        }
        
        // Bind shadow map to texture unit 5 (legacy single shadow)
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, Shadow::getShadowMapTexture(0));
        if (cache.shadowMap != -1) glUniform1i(cache.shadowMap, 5);
        
        // Multi-shadow support: count shadow-casting lights and pass intensities
        float lightIntensities[Shadow::MAX_SHADOW_LIGHTS] = {1.0f, 1.0f, 1.0f, 1.0f};
        int numShadowLights = 1;  // Main light always counts
        lightIntensities[0] = 1.0f;  // Main light intensity
        
        for (size_t i = 0; i < params.lights.size() && numShadowLights < Shadow::MAX_SHADOW_LIGHTS; ++i) {
            if (params.lights[i].enabled && params.lights[i].castsShadow) {
                lightIntensities[numShadowLights] = params.lights[i].intensity;
                numShadowLights++;
            }
        }
        if (cache.numShadowLights != -1) glUniform1i(cache.numShadowLights, numShadowLights);
        
        // Pass light intensities and bind all shadow maps
        const int SHADOW_TEX_START = 5;
        for (int s = 0; s < Shadow::MAX_SHADOW_LIGHTS; ++s) {
            if (cache.lightIntensities[s] != -1) glUniform1f(cache.lightIntensities[s], lightIntensities[s]);
            if (cache.lightSpaceMatrices[s] != -1) {
                glUniformMatrix4fv(cache.lightSpaceMatrices[s], 1, GL_FALSE,
                                   glm::value_ptr(Shadow::getLightSpaceMatrix(s)));
            }

            glActiveTexture(GL_TEXTURE0 + SHADOW_TEX_START + s);
            glBindTexture(GL_TEXTURE_2D, Shadow::getShadowMapTexture(s));
            if (cache.shadowMaps[s] != -1) glUniform1i(cache.shadowMaps[s], SHADOW_TEX_START + s);
        }
    }

    // Extra light array (Phong shader has these uniforms; others will ignore)
    {
        const int MAX_LIGHTS = 8;
        int numLights = static_cast<int>(std::min<size_t>(params.lights.size(), MAX_LIGHTS));
        glm::vec3 positions[MAX_LIGHTS]{};
        glm::vec3 colors[MAX_LIGHTS]{};
        float intensities[MAX_LIGHTS]{};
        for (int i = 0; i < numLights; ++i) {
            positions[i] = glm::vec3(params.lights[i].position[0],
                                     params.lights[i].position[1],
                                     params.lights[i].position[2]);
            colors[i] = glm::vec3(params.lights[i].diffuse[0],
                                  params.lights[i].diffuse[1],
                                  params.lights[i].diffuse[2]);
            intensities[i] = params.lights[i].intensity;
        }

        GLint numLoc = glGetUniformLocation(phong->getProgramId(), "numLights");
        GLint posLoc = glGetUniformLocation(phong->getProgramId(), "lightPositions");
        GLint colLoc = glGetUniformLocation(phong->getProgramId(), "lightColors");
        GLint intenLoc = glGetUniformLocation(phong->getProgramId(), "lightIntensitiesExtra");
        if (numLoc >= 0) glUniform1i(numLoc, numLights);
        if (posLoc >= 0 && numLights > 0) glUniform3fv(posLoc, numLights, glm::value_ptr(positions[0]));
        if (colLoc >= 0 && numLights > 0) glUniform3fv(colLoc, numLights, glm::value_ptr(colors[0]));
        if (intenLoc >= 0 && numLights > 0) glUniform1fv(intenLoc, numLights, intensities);
    }
}

void loadMatricesToShader(const TransformStack& stack, Camera* camera) {
    ShaderLib* shader = ShaderLib::instance();
    glm::mat4 M = stack.getCurrentTransform();
    glm::mat4 V = camera->getViewMatrix();
    glm::mat4 P = camera->getProjectionMatrix();
    glm::mat4 MV = V * M;
    glm::mat4 MVP = P * MV;
    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(MV)));
    
    auto wrapper = (*shader)["Phong"];
    wrapper->setUniform("MVP", MVP);
    wrapper->setUniform("MV", MV);
    wrapper->setUniform("M", M);
    wrapper->setUniform("normalMatrix", normalMatrix);
}

void loadMatricesToShader(const std::string& shaderName, const TransformStack& stack, Camera* camera) {
    ShaderLib* shader = ShaderLib::instance();
    glm::mat4 M = stack.getCurrentTransform();
    glm::mat4 V = camera->getViewMatrix();
    glm::mat4 P = camera->getProjectionMatrix();
    glm::mat4 MV = V * M;
    glm::mat4 MVP = P * MV;
    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(MV)));
    
    auto wrapper = (*shader)[shaderName];
    if (wrapper) {
        wrapper->setUniform("MVP", MVP);
        wrapper->setUniform("MV", MV);
        wrapper->setUniform("M", M);
        wrapper->setUniform("normalMatrix", normalMatrix);
    }
}

void renderFloorAndSphere(Floor* floor, SphereObstacle* sphere,
                          Camera* camera, TransformStack& transformStack,
                          const gfx::RenderSettings& params) {
    ShaderLib* shader = ShaderLib::instance();
    ShaderLib::ProgramWrapper* phong = (*shader)["Phong"];
    if (!phong) return;
    phong->use();
    transformStack.setGlobal(glm::mat4(1.0f));
    
    setupLighting(camera, params);
    
    // Render floor
    if (params.floorVisibility && floor) {
        transformStack.pushTransform();
        loadMatricesToShader(transformStack, camera);
        floor->draw("Phong", transformStack, camera);
        transformStack.popTransform();
    }
    
    // Render sphere
    if (params.sphereVisibility && sphere) {
        transformStack.pushTransform();
        loadMatricesToShader(transformStack, camera);
        sphere->draw("Phong", transformStack, camera);
        transformStack.popTransform();
    }
}

void cleanup(ClothRenderData& renderData) {
    if (renderData.VAO != 0) {
        glDeleteVertexArrays(1, &renderData.VAO);
        glDeleteBuffers(1, &renderData.VBO);
        renderData.VAO = 0;
        renderData.VBO = 0;
    }
}

} // namespace Renderer
