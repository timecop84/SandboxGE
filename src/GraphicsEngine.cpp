#include "GraphicsEngine.h"
#include "RenderSettings.h"
#include "ShaderPathResolver.h"
#include "SSAORenderer.h"
#include "ShadowRenderer.h"

#include <Camera.h>
#include <Light.h>
#include <ShaderLib.h>
#include <TransformStack.h>
#include <GeometryFactory.h>
#include <cstdlib>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>

namespace {
glm::vec3 generateRandomClothColor() {
    float hue = static_cast<float>(rand()) / RAND_MAX;
    float saturation = 0.7f + 0.3f * (static_cast<float>(rand()) / RAND_MAX);
    float value = 0.8f + 0.2f * (static_cast<float>(rand()) / RAND_MAX);

    int hi = static_cast<int>(hue * 6.0f) % 6;
    float f = hue * 6.0f - hi;
    float p = value * (1.0f - saturation);
    float q = value * (1.0f - f * saturation);
    float t = value * (1.0f - (1.0f - f) * saturation);

    switch (hi) {
        case 0: return glm::vec3(value, t, p);
        case 1: return glm::vec3(q, value, p);
        case 2: return glm::vec3(p, value, t);
        case 3: return glm::vec3(p, q, value);
        case 4: return glm::vec3(t, p, value);
        default: return glm::vec3(value, p, q);
    }
}
}  // namespace

namespace gfx {

bool Engine::initialize(int width, int height) {
    Renderer::initGL();
    SSAO::init(width, height);
    Shadow::init(4096);
    return true;
}

void Engine::resize(int width, int height) {
    SSAO::resize(width, height);
}

void Engine::setShaderRoot(const std::string& rootDir) {
    ShaderPath::setRoot(rootDir);
}

void Engine::ensurePrimaryMeshes(size_t count, std::vector<glm::vec3>& clothColors) {
    while (m_primaryMeshes.size() < count) {
        GpuMesh mesh{};
        glGenVertexArrays(1, &mesh.VAO);
        glGenBuffers(1, &mesh.VBO);
        glGenBuffers(1, &mesh.EBO);
        m_primaryMeshes.push_back(mesh);
        clothColors.push_back(generateRandomClothColor());
    }
}

void Engine::ensureGenericMeshes(size_t count) {
    // Remove extras
    while (m_genericMeshes.size() > count) {
        auto& mesh = m_genericMeshes.back();
        if (mesh.VAO) {
            glDeleteVertexArrays(1, &mesh.VAO);
            glDeleteBuffers(1, &mesh.VBO);
            glDeleteBuffers(1, &mesh.EBO);
        }
        m_genericMeshes.pop_back();
    }
    while (m_genericColors.size() > count) {
        m_genericColors.pop_back();
    }

    while (m_genericMeshes.size() < count) {
        GpuMesh mesh{};
        glGenVertexArrays(1, &mesh.VAO);
        glGenBuffers(1, &mesh.VBO);
        glGenBuffers(1, &mesh.EBO);
        m_genericMeshes.push_back(mesh);
    }
    while (m_genericColors.size() < count) {
        m_genericColors.emplace_back(0.8f, 0.8f, 0.8f);
    }
}

void Engine::syncPrimaryMeshes(const std::vector<MeshSource>& meshes,
                               std::vector<glm::vec3>& meshColors) {
    // Trim extras
    while (m_primaryMeshes.size() > meshes.size()) {
        auto& mesh = m_primaryMeshes.back();
        if (mesh.VAO) {
            glDeleteVertexArrays(1, &mesh.VAO);
            glDeleteBuffers(1, &mesh.VBO);
            glDeleteBuffers(1, &mesh.EBO);
        }
        m_primaryMeshes.pop_back();
    }
    while (meshColors.size() > meshes.size()) meshColors.pop_back();

    ensurePrimaryMeshes(meshes.size(), meshColors);

    static std::vector<float> vertexData;

    for (size_t i = 0; i < meshes.size(); ++i) {
        const MeshSource& src = meshes[i];
        GpuMesh& mesh = m_primaryMeshes[i];

        if (!src.positions || !src.normals || src.vertexCount <= 0) {
            mesh.vertexCount = 0;
            mesh.triangleCount = 0;
            continue;
        }

        mesh.vertexCount = src.vertexCount;
        mesh.triangleCount = src.indexCount / 3;

        vertexData.assign(src.vertexCount * 8, 0.0f);
        for (int j = 0; j < src.vertexCount; j++) {
            int vi = j * 8;
            int pi = j * 3;
            int ui = j * 2;

            vertexData[vi + 0] = src.positions[pi + 0];
            vertexData[vi + 1] = src.positions[pi + 1];
            vertexData[vi + 2] = src.positions[pi + 2];
            vertexData[vi + 3] = src.normals[pi + 0];
            vertexData[vi + 4] = src.normals[pi + 1];
            vertexData[vi + 5] = src.normals[pi + 2];
            vertexData[vi + 6] = src.uvs ? src.uvs[ui + 0] : 0.0f;
            vertexData[vi + 7] = src.uvs ? src.uvs[ui + 1] : 0.0f;
        }

        glBindVertexArray(mesh.VAO);

        glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
        size_t requiredVertexBytes = vertexData.size() * sizeof(float);
        if (requiredVertexBytes > mesh.vertexCapacityBytes) {
            glBufferData(GL_ARRAY_BUFFER, requiredVertexBytes, vertexData.data(), GL_DYNAMIC_DRAW);
            mesh.vertexCapacityBytes = requiredVertexBytes;
        } else {
            glBufferSubData(GL_ARRAY_BUFFER, 0, requiredVertexBytes, vertexData.data());
        }

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(1);

        if (src.indices && src.indexCount > 0) {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.EBO);
            size_t requiredIndexBytes = static_cast<size_t>(src.indexCount) * sizeof(uint32_t);
            if (requiredIndexBytes > mesh.indexCapacityBytes) {
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, requiredIndexBytes, src.indices, GL_STATIC_DRAW);
                mesh.indexCapacityBytes = requiredIndexBytes;
            } else {
                glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, requiredIndexBytes, src.indices);
            }
            mesh.triangleCount = src.indexCount / 3;
        } else {
            mesh.triangleCount = 0;
        }

        glBindVertexArray(0);
    }
}

void Engine::syncMeshes(const std::vector<MeshSource>& meshes) {
    ensureGenericMeshes(meshes.size());
    static std::vector<float> vertexData;

    for (size_t i = 0; i < meshes.size(); ++i) {
        const MeshSource& src = meshes[i];
        GpuMesh& mesh = m_genericMeshes[i];
        m_genericColors[i] = src.color;

        if (!src.positions || !src.normals || src.vertexCount <= 0) {
            mesh.vertexCount = 0;
            mesh.triangleCount = 0;
            continue;
        }

        vertexData.assign(src.vertexCount * 8, 0.0f);
        for (int j = 0; j < src.vertexCount; ++j) {
            int vi = j * 8;
            int pi = j * 3;
            int ui = j * 2;
            vertexData[vi + 0] = src.positions[pi + 0];
            vertexData[vi + 1] = src.positions[pi + 1];
            vertexData[vi + 2] = src.positions[pi + 2];
            vertexData[vi + 3] = src.normals[pi + 0];
            vertexData[vi + 4] = src.normals[pi + 1];
            vertexData[vi + 5] = src.normals[pi + 2];
            vertexData[vi + 6] = (src.uvs && src.vertexCount > 0) ? src.uvs[ui + 0] : 0.0f;
            vertexData[vi + 7] = (src.uvs && src.vertexCount > 0) ? src.uvs[ui + 1] : 0.0f;
        }

        glBindVertexArray(mesh.VAO);
        glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
        size_t requiredVertexBytes = vertexData.size() * sizeof(float);
        if (requiredVertexBytes > mesh.vertexCapacityBytes) {
            glBufferData(GL_ARRAY_BUFFER, requiredVertexBytes, vertexData.data(), GL_DYNAMIC_DRAW);
            mesh.vertexCapacityBytes = requiredVertexBytes;
        } else {
            glBufferSubData(GL_ARRAY_BUFFER, 0, requiredVertexBytes, vertexData.data());
        }

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(1);

        if (src.indices && src.indexCount > 0) {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.EBO);
            size_t requiredIndexBytes = static_cast<size_t>(src.indexCount) * sizeof(uint32_t);
            if (requiredIndexBytes > mesh.indexCapacityBytes) {
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, requiredIndexBytes, src.indices, GL_STATIC_DRAW);
                mesh.indexCapacityBytes = requiredIndexBytes;
            } else {
                glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, requiredIndexBytes, src.indices);
            }
            mesh.triangleCount = src.indexCount / 3;
        } else {
            mesh.triangleCount = 0;
        }

        mesh.vertexCount = src.vertexCount;
        glBindVertexArray(0);
    }
}

void Engine::renderScene(Camera* camera,
                         Floor* floor,
                         SphereObstacle* sphere,
                         Renderer::ClothRenderData& renderData,
                         const std::vector<glm::vec3>& primaryColors,
                         const gfx::RenderSettings& params,
                         TransformStack& transformStack) {
    // Shadow pass
    glm::vec3 lightWorldPos(params.lightPosition[0], params.lightPosition[1], params.lightPosition[2]);
    glm::vec3 lightAmbient(params.lightAmbient[0], params.lightAmbient[1], params.lightAmbient[2]);
    glm::vec3 lightDiffuse(params.lightDiffuse[0], params.lightDiffuse[1], params.lightDiffuse[2]);
    glm::vec3 lightSpecular(params.lightSpecular[0], params.lightSpecular[1], params.lightSpecular[2]);
    glm::vec3 sceneCenter(0.0f, 0.0f, 0.0f);
    float sceneRadius = 100.0f;
    auto drawShadowMeshes = [](const std::vector<GpuMesh>& meshes) {
        for (const GpuMesh& mesh : meshes) {
            if (mesh.VAO == 0 || mesh.triangleCount == 0) continue;
            glBindVertexArray(mesh.VAO);
            glDrawElements(GL_TRIANGLES, mesh.triangleCount * 3, GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
        }
    };

    // Multi-shadow pass: render shadow map for each shadow-casting light
    if (Shadow::isEnabled()) {
        int shadowIndex = 0;
        
        // Main light shadow (always first)
        {
            Light shadowLight(lightWorldPos, Colour(lightDiffuse.r, lightDiffuse.g, lightDiffuse.b, 1.0f));
            Shadow::beginShadowPass(shadowIndex, &shadowLight, sceneCenter, sceneRadius);

            // Cloth casters
            if (!m_primaryMeshes.empty() && params.clothVisibility) {
                glm::mat4 model = glm::mat4(1.0f);
                Shadow::setModelMatrix(model);
                drawShadowMeshes(m_primaryMeshes);
            }

            // Generic mesh casters
            if (!m_genericMeshes.empty() && params.customMeshVisibility) {
                glm::mat4 model = glm::mat4(1.0f);
                Shadow::setModelMatrix(model);
                drawShadowMeshes(m_genericMeshes);
            }

            // Sphere caster
            if (sphere && params.sphereVisibility) {
                Vector spherePos = sphere->getPosition();
                glm::mat4 sphereModel = glm::translate(glm::mat4(1.0f), glm::vec3(spherePos.m_x, spherePos.m_y, spherePos.m_z));
                sphereModel = glm::scale(sphereModel, glm::vec3(sphere->getRadius()));
                Shadow::setModelMatrix(sphereModel);
                sphere->renderGeometryOnly();
            }

            Shadow::endShadowPass();
            shadowIndex++;
        }
        
        // Additional lights from multi-light array
        for (size_t i = 0; i < params.lights.size() && shadowIndex < Shadow::MAX_SHADOW_LIGHTS; ++i) {
            const auto& lightData = params.lights[i];
            if (!lightData.enabled || !lightData.castsShadow) continue;
            
            glm::vec3 lPos(lightData.position[0], lightData.position[1], lightData.position[2]);
            glm::vec3 lDiff(lightData.diffuse[0], lightData.diffuse[1], lightData.diffuse[2]);
            Light shadowLight(lPos, Colour(lDiff.r, lDiff.g, lDiff.b, 1.0f));
            Shadow::beginShadowPass(shadowIndex, &shadowLight, sceneCenter, sceneRadius);

            // Cloth casters
            if (!m_primaryMeshes.empty() && params.clothVisibility) {
                glm::mat4 model = glm::mat4(1.0f);
                Shadow::setModelMatrix(model);
                drawShadowMeshes(m_primaryMeshes);
            }

            if (!m_genericMeshes.empty() && params.customMeshVisibility) {
                glm::mat4 model = glm::mat4(1.0f);
                Shadow::setModelMatrix(model);
                drawShadowMeshes(m_genericMeshes);
            }

            // Sphere caster
            if (sphere && params.sphereVisibility) {
                Vector spherePos = sphere->getPosition();
                glm::mat4 sphereModel = glm::translate(glm::mat4(1.0f), glm::vec3(spherePos.m_x, spherePos.m_y, spherePos.m_z));
                sphereModel = glm::scale(sphereModel, glm::vec3(sphere->getRadius()));
                Shadow::setModelMatrix(sphereModel);
                sphere->renderGeometryOnly();
            }

            Shadow::endShadowPass();
            shadowIndex++;
        }
    }

    // Scene pass to SSAO buffer
    SSAO::beginScenePass();
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    // Floor + sphere
    Renderer::renderFloorAndSphere(floor, sphere, camera, transformStack, params);

    // Light gizmo (visible position marker)
    {
        ShaderLib* shader = ShaderLib::instance();
        ShaderLib::ProgramWrapper* phong = (*shader)["Phong"];
        if (phong) {
            phong->use();
            glm::mat4 view = camera->getViewMatrix();
            glm::mat4 proj = camera->getProjectionMatrix();
            glm::vec3 gizmoColor(1.5f, 1.3f, 0.0f); // bright yellow marker

            glm::mat4 model = glm::translate(glm::mat4(1.0f), lightWorldPos);
            model = glm::scale(model, glm::vec3(3.5f)); // larger gizmo

            phong->setUniform("MVP", proj * view * model);
            phong->setUniform("M", model);
            phong->setUniform("MV", view * model);
            phong->setUniform("normalMatrix", glm::mat3(glm::transpose(glm::inverse(view * model))));

            // Overpower lighting to keep gizmo visible
            glm::vec3 gizmoLightColor(6.0f, 6.0f, 6.0f); // overpower lighting to keep gizmo visible
            phong->setUniform("light.position", view * glm::vec4(lightWorldPos, 1.0f));
            phong->setUniform("light.ambient", glm::vec4(gizmoLightColor, 1.0f));
            phong->setUniform("light.diffuse", glm::vec4(gizmoLightColor, 1.0f));
            phong->setUniform("light.specular", glm::vec4(gizmoLightColor, 1.0f));
            phong->setUniform("light.constantAttenuation", 1.0f);
            phong->setUniform("light.linearAttenuation", 0.0f);
            phong->setUniform("light.quadraticAttenuation", 0.0f);
            phong->setUniform("viewerPos", camera->getEye());
            phong->setUniform("Normalize", false);

            GLuint programId = phong->getProgramId();
            glUniform1i(glGetUniformLocation(programId, "shadowEnabled"), 0); // gizmo unshadowed

            // Boost material to stay bright
            phong->setUniform("material.ambient", glm::vec4(gizmoColor * 6.0f, 1.0f));
            phong->setUniform("material.diffuse", glm::vec4(gizmoColor * 6.0f, 1.0f));
            phong->setUniform("material.specular", glm::vec4(2.5f, 2.5f, 2.0f, 1.0f));
            phong->setUniform("material.shininess", 2.0f);

            if (!renderData.particleSphere) {
                renderData.particleSphere = FlockingGraphics::GeometryFactory::instance().createSphere(1.0f, 12);
            }
            renderData.particleSphere->render();
            
            // Render additional light gizmos
            for (size_t li = 0; li < params.lights.size(); ++li) {
                const auto& lightData = params.lights[li];
                if (!lightData.enabled) continue;
                
                glm::vec3 lPos(lightData.position[0], lightData.position[1], lightData.position[2]);
                glm::vec3 lColor(lightData.diffuse[0], lightData.diffuse[1], lightData.diffuse[2]);
                
                glm::mat4 lModel = glm::translate(glm::mat4(1.0f), lPos);
                lModel = glm::scale(lModel, glm::vec3(lightData.castsShadow ? 4.0f : 2.5f));
                
                phong->setUniform("MVP", proj * view * lModel);
                phong->setUniform("M", lModel);
                phong->setUniform("MV", view * lModel);
                phong->setUniform("normalMatrix", glm::mat3(glm::transpose(glm::inverse(view * lModel))));
                
                // Use light's own color for gizmo
                glm::vec3 gizmoCol = lColor * 4.0f * lightData.intensity;
                phong->setUniform("material.ambient", glm::vec4(gizmoCol, 1.0f));
                phong->setUniform("material.diffuse", glm::vec4(gizmoCol, 1.0f));
                
                renderData.particleSphere->render();
            }
        }
    }

    // Primary meshes (e.g., cloth) and auxiliary meshes
    if (!m_primaryMeshes.empty() || (!m_genericMeshes.empty() && params.customMeshVisibility)) {
        ShaderLib* shader = ShaderLib::instance();
        // Select shader: Phong, Silk, or SilkPBR
        std::string shaderName = "Phong";
        if (params.useSilkShader) {
            shaderName = params.usePBRSilk ? "SilkPBR" : "Silk";
        }
        ShaderLib::ProgramWrapper* prog = (*shader)[shaderName];
        if (prog) {
            prog->use();
            glm::mat4 view = camera->getViewMatrix();
            glm::vec4 lightViewPos = view * glm::vec4(lightWorldPos, 1.0f);
            prog->setUniform("light.position", lightViewPos);
            prog->setUniform("light.ambient", glm::vec4(lightAmbient, 1.0f));
            prog->setUniform("light.diffuse", glm::vec4(lightDiffuse, 1.0f));
            prog->setUniform("light.specular", glm::vec4(lightSpecular, 1.0f));
            prog->setUniform("light.constantAttenuation", 1.0f);
            prog->setUniform("light.linearAttenuation", 0.0f);
            prog->setUniform("light.quadraticAttenuation", 0.0f);
            prog->setUniform("viewerPos", camera->getEye());
            prog->setUniform("Normalize", true);

            GLuint programId = prog->getProgramId();
            glUniform1i(glGetUniformLocation(programId, "shadowEnabled"), Shadow::isEnabled() ? 1 : 0);
            glUniform1f(glGetUniformLocation(programId, "shadowBias"), params.shadowBias);
            glUniform1f(glGetUniformLocation(programId, "shadowSoftness"), params.shadowSoftness);
            glUniform1f(glGetUniformLocation(programId, "shadowMapSize"), 4096.0f);
            glUniform1f(glGetUniformLocation(programId, "shadowStrength"), 1.5f);
            glUniformMatrix4fv(glGetUniformLocation(programId, "lightSpaceMatrix"), 1, GL_FALSE,
                               glm::value_ptr(Shadow::getLightSpaceMatrix(0)));
            glActiveTexture(GL_TEXTURE5);
            glBindTexture(GL_TEXTURE_2D, Shadow::getShadowMapTexture(0));
            glUniform1i(glGetUniformLocation(programId, "shadowMap"), 5);
            
            // Multi-shadow uniforms for cloth
            float lightIntensities[Shadow::MAX_SHADOW_LIGHTS] = {1.0f, 1.0f, 1.0f, 1.0f};
            int numShadowLights = 1;
            lightIntensities[0] = 1.0f;
            
            for (size_t li = 0; li < params.lights.size() && numShadowLights < Shadow::MAX_SHADOW_LIGHTS; ++li) {
                if (params.lights[li].enabled && params.lights[li].castsShadow) {
                    lightIntensities[numShadowLights] = params.lights[li].intensity;
                    numShadowLights++;
                }
            }
            glUniform1i(glGetUniformLocation(programId, "numShadowLights"), numShadowLights);
            
            const int SHADOW_TEX_START = 5;
            for (int s = 0; s < Shadow::MAX_SHADOW_LIGHTS; ++s) {
                std::string intensityName = "lightIntensities[" + std::to_string(s) + "]";
                glUniform1f(glGetUniformLocation(programId, intensityName.c_str()), lightIntensities[s]);
                
                std::string matName = "lightSpaceMatrices[" + std::to_string(s) + "]";
                glUniformMatrix4fv(glGetUniformLocation(programId, matName.c_str()), 1, GL_FALSE,
                                   glm::value_ptr(Shadow::getLightSpaceMatrix(s)));
                
                glActiveTexture(GL_TEXTURE0 + SHADOW_TEX_START + s);
                glBindTexture(GL_TEXTURE_2D, Shadow::getShadowMapTexture(s));
                std::string samplerName = "shadowMaps[" + std::to_string(s) + "]";
                glUniform1i(glGetUniformLocation(programId, samplerName.c_str()), SHADOW_TEX_START + s);
            }

            prog->setUniform("material.ambient", glm::vec4(0.25f, 0.1f, 0.1f, 1.0f));
            prog->setUniform("material.diffuse", glm::vec4(0.8f, 0.2f, 0.2f, 1.0f));
            prog->setUniform("material.specular", glm::vec4(0.5f, 0.4f, 0.4f, 1.0f));
            prog->setUniform("material.shininess", 32.0f);

            if (params.useSilkShader) {
                if (params.usePBRSilk) {
                    // PBR-specific uniforms
                    prog->setUniform("lightWorldPos", lightWorldPos);  // World-space light for PBR
                    prog->setUniform("roughness", params.pbrRoughness);
                    prog->setUniform("metallic", params.pbrMetallic);
                    prog->setUniform("anisotropy", params.pbrAnisotropy);
                    prog->setUniform("sheenIntensity", params.sheenIntensity);
                    prog->setUniform("sheenColor", glm::vec3(params.sheenColor[0], params.sheenColor[1], params.sheenColor[2]));
                    prog->setUniform("subsurfaceAmount", params.subsurfaceAmount);
                    prog->setUniform("subsurfaceColor", glm::vec3(params.subsurfaceColor[0], params.subsurfaceColor[1], params.subsurfaceColor[2]));
                    prog->setUniform("weaveScale", params.weaveScale);
                } else {
                    // Classic Silk shader uniforms
                    prog->setUniform("anisotropyU", params.anisotropyU);
                    prog->setUniform("anisotropyV", params.anisotropyV);
                    prog->setUniform("sheenIntensity", params.sheenIntensity);
                    prog->setUniform("subsurfaceAmount", params.subsurfaceAmount);
                    prog->setUniform("subsurfaceColor", glm::vec3(0.9f, 0.5f, 0.4f));
                    prog->setUniform("weaveScale", params.weaveScale);
                    prog->setUniform("time", static_cast<float>(glfwGetTime()));
                }
            }

            prog->setUniform("checkerScale", params.useCheckerPattern ? params.checkerScale : 0.0f);
            prog->setUniform("checkerColor1", glm::vec3(params.checkerColor1[0], params.checkerColor1[1], params.checkerColor1[2]));
            prog->setUniform("checkerColor2", glm::vec3(params.checkerColor2[0], params.checkerColor2[1], params.checkerColor2[2]));

            prog->setUniform("aoStrength", params.aoStrength);
            prog->setUniform("aoGroundColor", glm::vec3(params.aoGroundColor[0], params.aoGroundColor[1], params.aoGroundColor[2]));

            glm::mat4 model = glm::mat4(1.0f);
            glm::mat4 projection = camera->getProjectionMatrix();
            prog->setUniform("MVP", projection * view * model);
            prog->setUniform("M", model);
            prog->setUniform("MV", view * model);
            prog->setUniform("normalMatrix", glm::mat3(glm::transpose(glm::inverse(view * model))));

            auto renderMeshList = [&](const std::vector<GpuMesh>& meshes,
                                      const std::vector<glm::vec3>& colors) {
                for (size_t i = 0; i < meshes.size(); ++i) {
                    const GpuMesh& mesh = meshes[i];
                    if (mesh.VAO == 0 || mesh.triangleCount == 0) continue;
                    glm::vec3 color = (i < colors.size()) ? colors[i] : glm::vec3(0.8f, 0.2f, 0.2f);
                    prog->setUniform("material.ambient", glm::vec4(color * 0.3f, 1.0f));
                    prog->setUniform("material.diffuse", glm::vec4(color, 1.0f));
                    prog->setUniform("material.specular", glm::vec4(0.5f, 0.5f, 0.5f, 1.0f));
                    if (params.useSilkShader) {
                        prog->setUniform("subsurfaceColor", color * 0.8f);
                    }
                    glBindVertexArray(mesh.VAO);
                    glDrawElements(GL_TRIANGLES, mesh.triangleCount * 3, GL_UNSIGNED_INT, 0);
                    glBindVertexArray(0);
                }
            };

            if (params.clothVisibility && !m_primaryMeshes.empty()) {
                if (params.clothWireframe) {
                    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                }
                renderMeshList(m_primaryMeshes, primaryColors);
                if (params.clothWireframe) {
                    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                }
            }

            if (params.customMeshVisibility && !m_genericMeshes.empty()) {
                if (params.customMeshWireframe) {
                    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                }
                renderMeshList(m_genericMeshes, m_genericColors);
                if (params.customMeshWireframe) {
                    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                }
            }
        }
    }

    SSAO::endScenePass();
    SSAO::renderComposite(camera);
}

void Engine::cleanup(Renderer::ClothRenderData& renderData) {
    for (auto& mesh : m_primaryMeshes) {
        if (mesh.VAO) {
            glDeleteVertexArrays(1, &mesh.VAO);
            glDeleteBuffers(1, &mesh.VBO);
            glDeleteBuffers(1, &mesh.EBO);
        }
    }
    m_primaryMeshes.clear();
    for (auto& mesh : m_genericMeshes) {
        if (mesh.VAO) {
            glDeleteVertexArrays(1, &mesh.VAO);
            glDeleteBuffers(1, &mesh.VBO);
            glDeleteBuffers(1, &mesh.EBO);
        }
    }
    m_genericMeshes.clear();
    m_genericColors.clear();
    SSAO::cleanup();
    Shadow::cleanup();
    Renderer::cleanup(renderData);
}

} // namespace gfx
