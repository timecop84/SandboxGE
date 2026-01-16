#pragma once

#include <glad/gl.h>

namespace gfx {

class IBLProcessor {
public:
    bool initialize();
    bool build(GLuint hdrTexture);
    void cleanup();

    GLuint getEnvCubemap() const { return m_envCubemap; }
    GLuint getIrradianceMap() const { return m_irradianceMap; }
    GLuint getPrefilterMap() const { return m_prefilterMap; }
    GLuint getBrdfLut() const { return m_brdfLut; }
    bool isReady() const { return m_ready; }

private:
    GLuint m_envCubemap = 0;
    GLuint m_irradianceMap = 0;
    GLuint m_prefilterMap = 0;
    GLuint m_brdfLut = 0;
    GLuint m_captureFBO = 0;
    GLuint m_captureRBO = 0;
    GLuint m_cubeVAO = 0;
    GLuint m_cubeVBO = 0;
    GLuint m_quadVAO = 0;
    GLuint m_quadVBO = 0;

    GLuint m_progEquirect = 0;
    GLuint m_progIrradiance = 0;
    GLuint m_progPrefilter = 0;
    GLuint m_progBrdf = 0;

    bool m_ready = false;

    bool initShaders();
    void initCube();
    void initQuad();
    void destroyTextures();
};

} // namespace gfx
