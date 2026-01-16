#include "rendering/ShadowRenderer.h"
#include "core/ResourceManager.h"
#include "rhi/Device.h"
#include <glad/gl.h>
#include <core/Light.h>
#include <core/Camera.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <array>
#include <limits>
#include <vector>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <iostream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace sandbox::Shadow {

namespace {
    // State
    bool s_initialized = false;
    bool s_enabled = true;
    int s_shadowMapSize = 4096;
    
    // Parameters
    float s_softness = 1.0f;
    float s_bias = 0.005f;
    
    // Multi-shadow map FBOs and textures
    GLuint s_shadowFBOs[MAX_SHADOW_LIGHTS] = {0};
    std::unique_ptr<rhi::Texture> s_shadowMapTexs[MAX_SHADOW_LIGHTS];
    std::unique_ptr<rhi::Sampler> s_shadowSampler;
    rhi::Device* s_device = nullptr;
    
    // Shader program
    GLuint s_shadowProgram = 0;
    
    // Light space matrices for each shadow map
    glm::mat4 s_lightSpaceMatrices[MAX_SHADOW_LIGHTS];
    
    // Current shadow light index
    int s_currentLightIndex = 0;
    
    // Previous viewport
    GLint s_prevViewport[4];
    
    std::string getExecutableDir() {
#ifdef _WIN32
        char path[MAX_PATH];
        DWORD len = GetModuleFileNameA(nullptr, path, MAX_PATH);
        if (len > 0 && len < MAX_PATH) {
            std::string exePath(path);
            size_t lastSlash = exePath.find_last_of("\\/");
            if (lastSlash != std::string::npos) {
                return exePath.substr(0, lastSlash + 1);
            }
        }
#endif
        return "";
    }
    
    std::string loadShaderSource(const std::string& filename) {
        std::string exeDir = getExecutableDir();
        std::vector<std::string> paths = {
            filename,
            "shaders/" + filename,
            "../shaders/" + filename,
            exeDir + filename,
            exeDir + "shaders/" + filename
        };
        
        for (const auto& path : paths) {
            std::ifstream file(path);
            if (file.is_open()) {
                std::stringstream buffer;
                buffer << file.rdbuf();
                if (!buffer.str().empty()) {
                    return buffer.str();
                }
            }
        }
        
        std::cerr << "Shadow: Failed to open shader: " << filename << std::endl;
        return "";
    }
    
    GLuint compileShader(GLenum type, const std::string& source) {
        GLuint shader = glCreateShader(type);
        const char* src = source.c_str();
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);
        
        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            std::cerr << "Shadow Shader compile error: " << infoLog << std::endl;
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }
    
    GLuint linkProgram(GLuint vs, GLuint fs) {
        GLuint program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glBindAttribLocation(program, 0, "inVert");
        glLinkProgram(program);
        
        GLint success;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(program, 512, nullptr, infoLog);
            std::cerr << "Shadow Program link error: " << infoLog << std::endl;
            glDeleteProgram(program);
            return 0;
        }
        return program;
    }
    
    GLuint createProgram(const std::string& vsFile, const std::string& fsFile) {
        std::string vsSource = loadShaderSource(vsFile);
        std::string fsSource = loadShaderSource(fsFile);
        if (vsSource.empty() || fsSource.empty()) return 0;
        
        GLuint vs = compileShader(GL_VERTEX_SHADER, vsSource);
        GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSource);
        if (!vs || !fs) {
            if (vs) glDeleteShader(vs);
            if (fs) glDeleteShader(fs);
            return 0;
        }
        
        GLuint prog = linkProgram(vs, fs);
        glDeleteShader(vs);
        glDeleteShader(fs);
        return prog;
    }

    std::array<glm::vec3, 8> getFrustumCornersWorld(Camera* camera, float nearPlane, float farPlane) {
        std::array<glm::vec3, 8> corners{};
        if (!camera) return corners;

        float fovY = glm::radians(camera->getFovY());
        float aspect = camera->getAspect();
        float tanHalfFov = tanf(fovY * 0.5f);

        float nearHeight = tanHalfFov * nearPlane;
        float nearWidth = nearHeight * aspect;
        float farHeight = tanHalfFov * farPlane;
        float farWidth = farHeight * aspect;

        glm::mat4 invView = glm::inverse(camera->getViewMatrix());

        std::array<glm::vec3, 8> viewCorners = {
            glm::vec3(-nearWidth,  nearHeight, -nearPlane),
            glm::vec3( nearWidth,  nearHeight, -nearPlane),
            glm::vec3( nearWidth, -nearHeight, -nearPlane),
            glm::vec3(-nearWidth, -nearHeight, -nearPlane),
            glm::vec3(-farWidth,   farHeight,  -farPlane),
            glm::vec3( farWidth,   farHeight,  -farPlane),
            glm::vec3( farWidth,  -farHeight,  -farPlane),
            glm::vec3(-farWidth,  -farHeight,  -farPlane)
        };

        for (size_t i = 0; i < viewCorners.size(); ++i) {
            glm::vec4 world = invView * glm::vec4(viewCorners[i], 1.0f);
            corners[i] = glm::vec3(world);
        }

        return corners;
    }

    glm::mat4 computeLightSpaceForCascade(Light* light, Camera* camera, float splitNear, float splitFar) {
        if (!light || !camera) return glm::mat4(1.0f);

        auto corners = getFrustumCornersWorld(camera, splitNear, splitFar);
        glm::vec3 center(0.0f);
        for (const auto& c : corners) center += c;
        center /= static_cast<float>(corners.size());

        float radius = 0.0f;
        for (const auto& c : corners) {
            radius = std::max(radius, glm::length(c - center));
        }
        radius = std::max(radius, 1.0f) * 1.5f;

        glm::vec3 lightPos = light->getPosition();
        glm::vec3 lightDir = -lightPos;
        if (glm::length(lightDir) < 0.0001f) {
            lightDir = glm::vec3(0.0f, -1.0f, 0.0f);
        } else {
            lightDir = glm::normalize(lightDir);
        }

        glm::vec3 up = (std::abs(lightDir.y) > 0.99f)
            ? glm::vec3(0.0f, 0.0f, 1.0f)
            : glm::vec3(0.0f, 1.0f, 0.0f);

        // Directional-style cascade fit using a bounding sphere to avoid clipping at split edges.
        glm::vec3 lightEye = center - lightDir * (radius * 2.5f);
        glm::mat4 lightView = glm::lookAt(lightEye, center, up);

        glm::vec3 minExt((std::numeric_limits<float>::max)());
        glm::vec3 maxExt((std::numeric_limits<float>::lowest)());
        for (const auto& c : corners) {
            glm::vec4 trf = lightView * glm::vec4(c, 1.0f);
            minExt = glm::min(minExt, glm::vec3(trf));
            maxExt = glm::max(maxExt, glm::vec3(trf));
        }

        // Stabilize cascades by snapping the light-space bounds to texel size.
        glm::vec2 extents = glm::vec2(maxExt.x - minExt.x, maxExt.y - minExt.y);
        if (extents.x < 1.0f) extents.x = 1.0f;
        if (extents.y < 1.0f) extents.y = 1.0f;
        glm::vec2 texelSize = extents / static_cast<float>(s_shadowMapSize);
        minExt.x = std::floor(minExt.x / texelSize.x) * texelSize.x;
        minExt.y = std::floor(minExt.y / texelSize.y) * texelSize.y;
        maxExt.x = minExt.x + texelSize.x * static_cast<float>(s_shadowMapSize);
        maxExt.y = minExt.y + texelSize.y * static_cast<float>(s_shadowMapSize);

        const float xyPadding = radius * 0.1f + 10.0f;
        minExt.x -= xyPadding;
        maxExt.x += xyPadding;
        minExt.y -= xyPadding;
        maxExt.y += xyPadding;

        const float zPadding = 80.0f;
        minExt.z -= zPadding;
        maxExt.z += zPadding;

        float nearPlane = -maxExt.z;
        float farPlane = -minExt.z;
        if (nearPlane < 0.1f) nearPlane = 0.1f;
        if (farPlane < nearPlane + 0.1f) farPlane = nearPlane + 0.1f;

        return glm::ortho(minExt.x, maxExt.x, minExt.y, maxExt.y, nearPlane, farPlane) * lightView;
    }
    
    GLuint toGlHandle(const rhi::Texture* tex) {
        if (!tex) return 0;
        return static_cast<GLuint>(tex->nativeHandle());
    }

    bool createShadowMap(int index) {
        glGenFramebuffers(1, &s_shadowFBOs[index]);
        glBindFramebuffer(GL_FRAMEBUFFER, s_shadowFBOs[index]);

        if (!s_device) {
            s_device = ResourceManager::instance()->getDevice();
        }

        s_shadowMapTexs[index] = s_device
            ? s_device->createTexture(s_shadowMapSize, s_shadowMapSize, rhi::TextureFormat::Depth32F)
            : nullptr;

        const GLuint texHandle = toGlHandle(s_shadowMapTexs[index].get());
        if (texHandle == 0) {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return false;
        }

        glBindTexture(GL_TEXTURE_2D, texHandle);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        
        float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, texHandle, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "Shadow: Framebuffer " << index << " not complete" << std::endl;
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return false;
        }
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return true;
    }
}

bool init(int shadowMapSize) {
    if (s_initialized) return true;
    
    s_shadowMapSize = shadowMapSize;

    if (!s_device) {
        s_device = ResourceManager::instance()->getDevice();
    }
    if (s_device && !s_shadowSampler) {
        rhi::SamplerDesc shadowSampler;
        shadowSampler.minFilter = rhi::SamplerFilter::Linear;
        shadowSampler.magFilter = rhi::SamplerFilter::Linear;
        shadowSampler.wrapS = rhi::SamplerWrap::ClampToBorder;
        shadowSampler.wrapT = rhi::SamplerWrap::ClampToBorder;
        shadowSampler.compareEnabled = true;
        shadowSampler.compareOp = rhi::SamplerCompareOp::Lequal;
        shadowSampler.borderColor[0] = 1.0f;
        shadowSampler.borderColor[1] = 1.0f;
        shadowSampler.borderColor[2] = 1.0f;
        shadowSampler.borderColor[3] = 1.0f;
        s_shadowSampler = s_device->createSampler(shadowSampler);
    }
    
    // Create shadow shader
    s_shadowProgram = createProgram("shaders/Shadow.vs", "shaders/Shadow.fs");
    if (!s_shadowProgram) {
        std::cerr << "Shadow: Failed to create shader, shadows disabled" << std::endl;
        s_enabled = false;
        return false;
    }
    
    // Create shadow maps for all lights
    for (int i = 0; i < MAX_SHADOW_LIGHTS; ++i) {
        if (!createShadowMap(i)) {
            cleanup();
            return false;
        }
        s_lightSpaceMatrices[i] = glm::mat4(1.0f);
    }
    
    s_initialized = true;
    return true;
}

void cleanup() {
    for (int i = 0; i < MAX_SHADOW_LIGHTS; ++i) {
        if (s_shadowFBOs[i]) { glDeleteFramebuffers(1, &s_shadowFBOs[i]); s_shadowFBOs[i] = 0; }
        s_shadowMapTexs[i].reset();
    }
    s_shadowSampler.reset();
    if (s_shadowProgram) { glDeleteProgram(s_shadowProgram); s_shadowProgram = 0; }
    s_initialized = false;
}

void setDevice(rhi::Device* device) {
    s_device = device;
}

void beginShadowPass(int lightIndex, Light* light, const glm::vec3& sceneCenter, float sceneRadius) {
    if (!s_initialized || !s_enabled || !light) return;
    if (lightIndex < 0 || lightIndex >= MAX_SHADOW_LIGHTS) return;
    
    s_currentLightIndex = lightIndex;
    
    glGetIntegerv(GL_VIEWPORT, s_prevViewport);
    glBindFramebuffer(GL_FRAMEBUFFER, s_shadowFBOs[lightIndex]);
    glViewport(0, 0, s_shadowMapSize, s_shadowMapSize);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glCullFace(GL_FRONT);
    
    glm::vec3 lightPos = light->getPosition();
    float orthoSize = sceneRadius * 1.5f;
    glm::mat4 lightProjection = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize,
                                            0.1f, sceneRadius * 4.0f);
    glm::mat4 lightView = glm::lookAt(lightPos, sceneCenter, glm::vec3(0.0f, 1.0f, 0.0f));
    
    s_lightSpaceMatrices[lightIndex] = lightProjection * lightView;
    
    glUseProgram(s_shadowProgram);
    glUniformMatrix4fv(glGetUniformLocation(s_shadowProgram, "lightSpaceMatrix"),
                       1, GL_FALSE, glm::value_ptr(s_lightSpaceMatrices[lightIndex]));
}

void beginShadowCascade(int cascadeIndex, Light* light, Camera* camera, float splitNear, float splitFar) {
    if (!s_initialized || !s_enabled || !light || !camera) return;
    if (cascadeIndex < 0 || cascadeIndex >= MAX_SHADOW_LIGHTS) return;

    s_currentLightIndex = cascadeIndex;

    glGetIntegerv(GL_VIEWPORT, s_prevViewport);
    glBindFramebuffer(GL_FRAMEBUFFER, s_shadowFBOs[cascadeIndex]);
    glViewport(0, 0, s_shadowMapSize, s_shadowMapSize);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glCullFace(GL_FRONT);

    s_lightSpaceMatrices[cascadeIndex] = computeLightSpaceForCascade(light, camera, splitNear, splitFar);

    glUseProgram(s_shadowProgram);
    glUniformMatrix4fv(glGetUniformLocation(s_shadowProgram, "lightSpaceMatrix"),
                       1, GL_FALSE, glm::value_ptr(s_lightSpaceMatrices[cascadeIndex]));
}

void endShadowPass() {
    if (!s_initialized || !s_enabled) return;
    glCullFace(GL_BACK);
    glViewport(s_prevViewport[0], s_prevViewport[1], s_prevViewport[2], s_prevViewport[3]);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

glm::mat4 getLightSpaceMatrix(int lightIndex) {
    if (lightIndex >= 0 && lightIndex < MAX_SHADOW_LIGHTS)
        return s_lightSpaceMatrices[lightIndex];
    return glm::mat4(1.0f);
}

const rhi::Texture* getShadowMapTexture(int lightIndex) {
    if (lightIndex >= 0 && lightIndex < MAX_SHADOW_LIGHTS) {
        return s_shadowMapTexs[lightIndex].get();
    }
    return nullptr;
}

void bindShadowMap(int lightIndex, int slot) {
    const rhi::Texture* texture = getShadowMapTexture(lightIndex);
    if (texture) {
        texture->bind(static_cast<std::uint32_t>(slot));
        if (s_shadowSampler) {
            s_shadowSampler->bind(static_cast<std::uint32_t>(slot));
        }
    }
}

GLuint getShadowProgram() {
    return s_shadowProgram;
}

void setModelMatrix(const glm::mat4& model) {
    if (s_shadowProgram) {
        glUniformMatrix4fv(glGetUniformLocation(s_shadowProgram, "model"),
                           1, GL_FALSE, glm::value_ptr(model));
    }
}

void setSoftness(float softness) { s_softness = softness; }
void setBias(float bias) { s_bias = bias; }
void setEnabled(bool enabled) { s_enabled = enabled; }

bool isEnabled() { return s_enabled && s_initialized; }
float getSoftness() { return s_softness; }
float getBias() { return s_bias; }
int getMapSize() { return s_shadowMapSize; }

} // namespace sandbox::Shadow
