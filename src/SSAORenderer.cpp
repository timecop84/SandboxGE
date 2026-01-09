/// @file SSAORenderer.cpp
/// @brief Screen-Space Ambient Occlusion implementation

#include "SSAORenderer.h"
#include "ShaderPathResolver.h"
#include <glad/gl.h>
#include <Camera.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <random>
#include <cmath>
#include <iostream>

namespace SSAO {

namespace {
    // State
    bool s_initialized = false;
    bool s_enabled = true;
    int s_width = 0;
    int s_height = 0;
    
    // Parameters
    float s_radius = 2.0f;
    float s_bias = 0.025f;
    float s_intensity = 1.5f;
    
    // Shader programs (we manage these ourselves)
    GLuint s_ssaoProgram = 0;
    GLuint s_blurProgram = 0;
    GLuint s_compositeProgram = 0;
    
    // Framebuffers
    GLuint s_sceneFBO = 0;
    GLuint s_sceneColorTex = 0;
    GLuint s_sceneDepthTex = 0;
    
    GLuint s_ssaoFBO = 0;
    GLuint s_ssaoColorTex = 0;
    
    GLuint s_ssaoBlurFBO = 0;
    GLuint s_ssaoBlurTex = 0;
    
    // Noise texture for random rotation
    GLuint s_noiseTex = 0;
    
    // Sample kernel
    std::vector<glm::vec3> s_kernel;
    
    // Fullscreen quad VAO
    GLuint s_quadVAO = 0;
    GLuint s_quadVBO = 0;
    
    // Compile shader
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
            std::cerr << "SSAO Shader compile error: " << infoLog << std::endl;
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }
    
    // Link program
    GLuint linkProgram(GLuint vs, GLuint fs) {
        GLuint program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);
        
        GLint success;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(program, 512, nullptr, infoLog);
            std::cerr << "SSAO Program link error: " << infoLog << std::endl;
            glDeleteProgram(program);
            return 0;
        }
        return program;
    }
    
    // Create shader program from files
    GLuint createProgram(const std::string& vsFile, const std::string& fsFile) {
        std::string vsSource = ShaderPath::loadSource(vsFile);
        std::string fsSource = ShaderPath::loadSource(fsFile);
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

    // Generate hemisphere sample kernel
    void generateKernel() {
        std::uniform_real_distribution<float> randomFloats(0.0f, 1.0f);
        std::default_random_engine generator(42);
        
        s_kernel.clear();
        s_kernel.reserve(64);
        
        for (int i = 0; i < 64; ++i) {
            glm::vec3 sample(
                randomFloats(generator) * 2.0f - 1.0f,
                randomFloats(generator) * 2.0f - 1.0f,
                randomFloats(generator)  // Hemisphere, so z is [0,1]
            );
            sample = glm::normalize(sample);
            sample *= randomFloats(generator);
            
            // Scale samples to be closer to origin (more samples near fragment)
            float scale = float(i) / 64.0f;
            scale = 0.1f + scale * scale * 0.9f;  // lerp(0.1, 1.0, scale^2)
            sample *= scale;
            
            s_kernel.push_back(sample);
        }
    }
    
    // Generate noise texture for random rotation
    void generateNoiseTexture() {
        std::uniform_real_distribution<float> randomFloats(0.0f, 1.0f);
        std::default_random_engine generator(123);
        
        std::vector<glm::vec3> noise;
        noise.reserve(16);
        for (int i = 0; i < 16; ++i) {
            glm::vec3 n(
                randomFloats(generator) * 2.0f - 1.0f,
                randomFloats(generator) * 2.0f - 1.0f,
                0.0f
            );
            noise.push_back(n);
        }
        
        glGenTextures(1, &s_noiseTex);
        glBindTexture(GL_TEXTURE_2D, s_noiseTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 4, 4, 0, GL_RGB, GL_FLOAT, noise.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }
    
    // Create fullscreen quad
    void createQuad() {
        float quadVertices[] = {
            // positions   // texcoords
            -1.0f,  1.0f, 0.0f,  0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f,  0.0f, 0.0f,
             1.0f, -1.0f, 0.0f,  1.0f, 0.0f,
            -1.0f,  1.0f, 0.0f,  0.0f, 1.0f,
             1.0f, -1.0f, 0.0f,  1.0f, 0.0f,
             1.0f,  1.0f, 0.0f,  1.0f, 1.0f
        };
        
        glGenVertexArrays(1, &s_quadVAO);
        glGenBuffers(1, &s_quadVBO);
        glBindVertexArray(s_quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, s_quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
        
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        
        glBindVertexArray(0);
    }
    
    void createFramebuffers(int width, int height) {
        // Scene FBO with color and depth
        glGenFramebuffers(1, &s_sceneFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, s_sceneFBO);
        
        // Color texture
        glGenTextures(1, &s_sceneColorTex);
        glBindTexture(GL_TEXTURE_2D, s_sceneColorTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_sceneColorTex, 0);
        
        // Depth texture (for SSAO sampling)
        glGenTextures(1, &s_sceneDepthTex);
        glBindTexture(GL_TEXTURE_2D, s_sceneDepthTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, s_sceneDepthTex, 0);
        
        // SSAO FBO (single channel)
        glGenFramebuffers(1, &s_ssaoFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, s_ssaoFBO);
        
        glGenTextures(1, &s_ssaoColorTex);
        glBindTexture(GL_TEXTURE_2D, s_ssaoColorTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_ssaoColorTex, 0);
        
        // SSAO Blur FBO
        glGenFramebuffers(1, &s_ssaoBlurFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, s_ssaoBlurFBO);
        
        glGenTextures(1, &s_ssaoBlurTex);
        glBindTexture(GL_TEXTURE_2D, s_ssaoBlurTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_ssaoBlurTex, 0);
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    
    void deleteFramebuffers() {
        if (s_sceneFBO) { glDeleteFramebuffers(1, &s_sceneFBO); s_sceneFBO = 0; }
        if (s_sceneColorTex) { glDeleteTextures(1, &s_sceneColorTex); s_sceneColorTex = 0; }
        if (s_sceneDepthTex) { glDeleteTextures(1, &s_sceneDepthTex); s_sceneDepthTex = 0; }
        if (s_ssaoFBO) { glDeleteFramebuffers(1, &s_ssaoFBO); s_ssaoFBO = 0; }
        if (s_ssaoColorTex) { glDeleteTextures(1, &s_ssaoColorTex); s_ssaoColorTex = 0; }
        if (s_ssaoBlurFBO) { glDeleteFramebuffers(1, &s_ssaoBlurFBO); s_ssaoBlurFBO = 0; }
        if (s_ssaoBlurTex) { glDeleteTextures(1, &s_ssaoBlurTex); s_ssaoBlurTex = 0; }
    }
    
    void renderQuad() {
        glBindVertexArray(s_quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }
}

bool init(int width, int height) {
    if (s_initialized) return true;
    
    s_width = width;
    s_height = height;
    
    // Load SSAO shaders (use our own shader loading)
    s_ssaoProgram = createProgram("shaders/Fullscreen.vs", "shaders/SSAO.fs");
    s_blurProgram = createProgram("shaders/Fullscreen.vs", "shaders/SSAOBlur.fs");
    s_compositeProgram = createProgram("shaders/Fullscreen.vs", "shaders/Composite.fs");
    
    if (!s_ssaoProgram || !s_blurProgram || !s_compositeProgram) {
        std::cerr << "SSAO: Failed to create shaders, SSAO disabled" << std::endl;
        s_enabled = false;
        return false;
    }
    
    generateKernel();
    generateNoiseTexture();
    createQuad();
    createFramebuffers(width, height);
    
    s_initialized = true;
    return true;
}

void cleanup() {
    deleteFramebuffers();
    
    if (s_ssaoProgram) { glDeleteProgram(s_ssaoProgram); s_ssaoProgram = 0; }
    if (s_blurProgram) { glDeleteProgram(s_blurProgram); s_blurProgram = 0; }
    if (s_compositeProgram) { glDeleteProgram(s_compositeProgram); s_compositeProgram = 0; }
    if (s_noiseTex) { glDeleteTextures(1, &s_noiseTex); s_noiseTex = 0; }
    if (s_quadVAO) { glDeleteVertexArrays(1, &s_quadVAO); s_quadVAO = 0; }
    if (s_quadVBO) { glDeleteBuffers(1, &s_quadVBO); s_quadVBO = 0; }
    
    s_kernel.clear();
    s_initialized = false;
}

void resize(int width, int height) {
    if (width == s_width && height == s_height) return;
    
    s_width = width;
    s_height = height;
    
    deleteFramebuffers();
    createFramebuffers(width, height);
}

void beginScenePass() {
    if (!s_initialized || !s_enabled) {
        // When disabled, render directly to default framebuffer
        return;
    }
    
    // Bind scene FBO - scene will be rendered here
    glBindFramebuffer(GL_FRAMEBUFFER, s_sceneFBO);
}

void endScenePass() {
    if (!s_initialized || !s_enabled) {
        // Nothing to do when disabled
        return;
    }
    
    // Unbind scene FBO, SSAO passes will happen in renderComposite
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void renderComposite(Camera* camera) {
    if (!s_initialized) return;
    
    if (!s_enabled) {
        // SSAO disabled - just blit scene to screen
        return;
    }
    
    glm::mat4 proj = camera->getProjectionMatrix();
    glm::mat4 invProj = glm::inverse(proj);
    
    // Disable depth test for fullscreen passes
    glDisable(GL_DEPTH_TEST);
    
    // Pass 1: SSAO calculation
    glBindFramebuffer(GL_FRAMEBUFFER, s_ssaoFBO);
    glClear(GL_COLOR_BUFFER_BIT);
    
    if (s_ssaoProgram) {
        glUseProgram(s_ssaoProgram);
        
        // Bind depth texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, s_sceneDepthTex);
        glUniform1i(glGetUniformLocation(s_ssaoProgram, "depthTexture"), 0);
        
        // Bind noise texture
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, s_noiseTex);
        glUniform1i(glGetUniformLocation(s_ssaoProgram, "noiseTexture"), 1);
        
        // Upload kernel samples
        for (int i = 0; i < 64 && i < (int)s_kernel.size(); ++i) {
            char name[32];
            snprintf(name, sizeof(name), "samples[%d]", i);
            glUniform3fv(glGetUniformLocation(s_ssaoProgram, name), 1, glm::value_ptr(s_kernel[i]));
        }
        
        glUniformMatrix4fv(glGetUniformLocation(s_ssaoProgram, "projection"), 1, GL_FALSE, glm::value_ptr(proj));
        glUniformMatrix4fv(glGetUniformLocation(s_ssaoProgram, "invProjection"), 1, GL_FALSE, glm::value_ptr(invProj));
        glUniform2f(glGetUniformLocation(s_ssaoProgram, "screenSize"), (float)s_width, (float)s_height);
        glUniform1f(glGetUniformLocation(s_ssaoProgram, "radius"), s_radius);
        glUniform1f(glGetUniformLocation(s_ssaoProgram, "bias"), s_bias);
        glUniform1f(glGetUniformLocation(s_ssaoProgram, "intensity"), s_intensity);
        
        renderQuad();
    }
    
    // Pass 2: Blur SSAO
    glBindFramebuffer(GL_FRAMEBUFFER, s_ssaoBlurFBO);
    glClear(GL_COLOR_BUFFER_BIT);
    
    if (s_blurProgram) {
        glUseProgram(s_blurProgram);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, s_ssaoColorTex);
        glUniform1i(glGetUniformLocation(s_blurProgram, "ssaoTexture"), 0);
        glUniform2f(glGetUniformLocation(s_blurProgram, "texelSize"), 1.0f / s_width, 1.0f / s_height);
        
        renderQuad();
    }
    
    // Pass 3: Composite scene with SSAO
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    if (s_compositeProgram) {
        glUseProgram(s_compositeProgram);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, s_sceneColorTex);
        glUniform1i(glGetUniformLocation(s_compositeProgram, "sceneTexture"), 0);
        
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, s_ssaoBlurTex);
        glUniform1i(glGetUniformLocation(s_compositeProgram, "ssaoTexture"), 1);
        
        glUniform1f(glGetUniformLocation(s_compositeProgram, "ssaoStrength"), s_intensity > 0.0f ? 1.0f : 0.0f);
        
        renderQuad();
    }
    
    // Re-enable depth test for next frame
    glEnable(GL_DEPTH_TEST);
}

void setRadius(float radius) { s_radius = radius; }
void setBias(float bias) { s_bias = bias; }
void setIntensity(float intensity) { s_intensity = intensity; }
void setEnabled(bool enabled) { s_enabled = enabled; }

bool isEnabled() { return s_enabled; }
float getRadius() { return s_radius; }
float getBias() { return s_bias; }
float getIntensity() { return s_intensity; }

} // namespace SSAO
