#include "rendering/ShadowRenderer.h"
#include "core/ResourceManager.h"
#include "rhi/Device.h"
#include <glad/gl.h>
#include <core/Light.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#ifdef _WIN32
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
