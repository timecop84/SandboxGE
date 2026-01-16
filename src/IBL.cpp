#include "IBL.h"
#include "ShaderPathResolver.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <iostream>
#include <string>
#include <vector>

namespace gfx {

namespace {
GLuint compileShader(GLenum type, const std::string& source, const char* label) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
        std::cerr << "IBL shader compile failed (" << label << "): " << infoLog << std::endl;
    }
    return shader;
}

GLuint createProgram(const char* vsPath, const char* fsPath, const char* label) {
    std::string vsSource = ShaderPath::loadSource(vsPath);
    std::string fsSource = ShaderPath::loadSource(fsPath);
    if (vsSource.empty() || fsSource.empty()) {
        std::cerr << "IBL shader load failed (" << label << ")" << std::endl;
        return 0;
    }

    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSource, label);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSource, label);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetProgramInfoLog(program, 1024, nullptr, infoLog);
        std::cerr << "IBL program link failed (" << label << "): " << infoLog << std::endl;
    }
    return program;
}

void renderCube(GLuint vao) {
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

void renderQuad(GLuint vao) {
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}
} // namespace

bool IBLProcessor::initialize() {
    if (m_ready) return true;
    initCube();
    initQuad();
    initShaders();
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    m_ready = (m_progEquirect != 0 && m_progIrradiance != 0 && m_progPrefilter != 0 && m_progBrdf != 0);
    return m_ready;
}

void IBLProcessor::destroyTextures() {
    if (m_envCubemap) glDeleteTextures(1, &m_envCubemap);
    if (m_irradianceMap) glDeleteTextures(1, &m_irradianceMap);
    if (m_prefilterMap) glDeleteTextures(1, &m_prefilterMap);
    if (m_brdfLut) glDeleteTextures(1, &m_brdfLut);
    m_envCubemap = 0;
    m_irradianceMap = 0;
    m_prefilterMap = 0;
    m_brdfLut = 0;
}

bool IBLProcessor::build(GLuint hdrTexture) {
    if (!hdrTexture) return false;
    if (!initialize()) return false;

    destroyTextures();

    if (!m_captureFBO) glGenFramebuffers(1, &m_captureFBO);
    if (!m_captureRBO) glGenRenderbuffers(1, &m_captureRBO);

    glBindFramebuffer(GL_FRAMEBUFFER, m_captureFBO);

    const int envSize = 512;
    glGenTextures(1, &m_envCubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_envCubemap);
    for (int i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, envSize, envSize, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    std::array<glm::mat4, 6> captureViews = {
        glm::lookAt(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))
    };

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    glUseProgram(m_progEquirect);
    glUniform1i(glGetUniformLocation(m_progEquirect, "equirectMap"), 0);
    glUniformMatrix4fv(glGetUniformLocation(m_progEquirect, "projection"), 1, GL_FALSE,
                       &captureProjection[0][0]);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrTexture);

    glBindRenderbuffer(GL_RENDERBUFFER, m_captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, envSize, envSize);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_captureRBO);

    glViewport(0, 0, envSize, envSize);
    for (int i = 0; i < 6; ++i) {
        glUniformMatrix4fv(glGetUniformLocation(m_progEquirect, "view"), 1, GL_FALSE,
                           &captureViews[i][0][0]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_envCubemap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        renderCube(m_cubeVAO);
    }
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_envCubemap);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    const int irradianceSize = 32;
    glGenTextures(1, &m_irradianceMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_irradianceMap);
    for (int i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, irradianceSize, irradianceSize, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glUseProgram(m_progIrradiance);
    glUniform1i(glGetUniformLocation(m_progIrradiance, "environmentMap"), 0);
    glUniformMatrix4fv(glGetUniformLocation(m_progIrradiance, "projection"), 1, GL_FALSE,
                       &captureProjection[0][0]);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_envCubemap);

    glBindRenderbuffer(GL_RENDERBUFFER, m_captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, irradianceSize, irradianceSize);
    glViewport(0, 0, irradianceSize, irradianceSize);
    for (int i = 0; i < 6; ++i) {
        glUniformMatrix4fv(glGetUniformLocation(m_progIrradiance, "view"), 1, GL_FALSE,
                           &captureViews[i][0][0]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_irradianceMap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        renderCube(m_cubeVAO);
    }

    const int prefilterSize = 128;
    glGenTextures(1, &m_prefilterMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_prefilterMap);
    const int maxMipLevels = 5;
    for (int mip = 0; mip < maxMipLevels; ++mip) {
        int mipSize = prefilterSize >> mip;
        for (int i = 0; i < 6; ++i) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, mip, GL_RGB16F,
                         mipSize, mipSize, 0, GL_RGB, GL_FLOAT, nullptr);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glUseProgram(m_progPrefilter);
    glUniform1i(glGetUniformLocation(m_progPrefilter, "environmentMap"), 0);
    glUniformMatrix4fv(glGetUniformLocation(m_progPrefilter, "projection"), 1, GL_FALSE,
                       &captureProjection[0][0]);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_envCubemap);

    for (int mip = 0; mip < maxMipLevels; ++mip) {
        int mipSize = prefilterSize >> mip;
        float roughness = static_cast<float>(mip) / static_cast<float>(maxMipLevels - 1);
        glUniform1f(glGetUniformLocation(m_progPrefilter, "roughness"), roughness);
        glBindRenderbuffer(GL_RENDERBUFFER, m_captureRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipSize, mipSize);
        glViewport(0, 0, mipSize, mipSize);
        for (int i = 0; i < 6; ++i) {
            glUniformMatrix4fv(glGetUniformLocation(m_progPrefilter, "view"), 1, GL_FALSE,
                               &captureViews[i][0][0]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_prefilterMap, mip);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            renderCube(m_cubeVAO);
        }
    }

    const int brdfSize = 512;
    glGenTextures(1, &m_brdfLut);
    glBindTexture(GL_TEXTURE_2D, m_brdfLut);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, brdfSize, brdfSize, 0, GL_RG, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindRenderbuffer(GL_RENDERBUFFER, m_captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, brdfSize, brdfSize);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_brdfLut, 0);
    glViewport(0, 0, brdfSize, brdfSize);
    glUseProgram(m_progBrdf);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderQuad(m_quadVAO);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

    return true;
}

void IBLProcessor::cleanup() {
    destroyTextures();
    if (m_captureFBO) glDeleteFramebuffers(1, &m_captureFBO);
    if (m_captureRBO) glDeleteRenderbuffers(1, &m_captureRBO);
    if (m_cubeVAO) glDeleteVertexArrays(1, &m_cubeVAO);
    if (m_cubeVBO) glDeleteBuffers(1, &m_cubeVBO);
    if (m_quadVAO) glDeleteVertexArrays(1, &m_quadVAO);
    if (m_quadVBO) glDeleteBuffers(1, &m_quadVBO);
    if (m_progEquirect) glDeleteProgram(m_progEquirect);
    if (m_progIrradiance) glDeleteProgram(m_progIrradiance);
    if (m_progPrefilter) glDeleteProgram(m_progPrefilter);
    if (m_progBrdf) glDeleteProgram(m_progBrdf);
    m_captureFBO = 0;
    m_captureRBO = 0;
    m_cubeVAO = 0;
    m_cubeVBO = 0;
    m_quadVAO = 0;
    m_quadVBO = 0;
    m_progEquirect = 0;
    m_progIrradiance = 0;
    m_progPrefilter = 0;
    m_progBrdf = 0;
    m_ready = false;
}

bool IBLProcessor::initShaders() {
    if (m_progEquirect && m_progIrradiance && m_progPrefilter && m_progBrdf) return true;
    m_progEquirect = createProgram("IBL_Cubemap.vs", "IBL_Equirect.fs", "IBL_Equirect");
    m_progIrradiance = createProgram("IBL_Cubemap.vs", "IBL_Irradiance.fs", "IBL_Irradiance");
    m_progPrefilter = createProgram("IBL_Cubemap.vs", "IBL_Prefilter.fs", "IBL_Prefilter");
    m_progBrdf = createProgram("IBL_BRDF.vs", "IBL_BRDF.fs", "IBL_BRDF");
    return (m_progEquirect && m_progIrradiance && m_progPrefilter && m_progBrdf);
}

void IBLProcessor::initCube() {
    if (m_cubeVAO) return;
    float vertices[] = {
        -1.0f,  1.0f, -1.0f,  -1.0f, -1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,  -1.0f, -1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,   1.0f, -1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,   1.0f,  1.0f, -1.0f,   1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,   1.0f, -1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,   1.0f,  1.0f, -1.0f,   1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f,  1.0f
    };
    glGenVertexArrays(1, &m_cubeVAO);
    glGenBuffers(1, &m_cubeVBO);
    glBindVertexArray(m_cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);
}

void IBLProcessor::initQuad() {
    if (m_quadVAO) return;
    float quadVerts[] = {
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 1.0f
    };
    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);
    glBindVertexArray(m_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
}

} // namespace gfx
