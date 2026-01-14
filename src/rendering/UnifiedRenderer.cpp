#include "rendering/UnifiedRenderer.h"
#include "rendering/UBOManager.h"
#include "rendering/UBOStructures.h"
#include "rendering/IRenderable.h"
#include "rendering/SSAORenderer.h"
#include "rendering/ShadowRenderer.h"
#include "utils/ShaderPathResolver.h"
#include <core/Camera.h>
#include <core/Light.h>
#include <core/Colour.h>
#include <utils/ShaderLib.h>
#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <chrono>

namespace gfx {

UnifiedRenderer::UnifiedRenderer() {
}

UnifiedRenderer::~UnifiedRenderer() {
    cleanup();
}

bool UnifiedRenderer::initialize(int width, int height) {
    std::cout << "UnifiedRenderer: Initializing..." << std::endl;
    
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDisable(GL_CULL_FACE);
    glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
    
    UBOManager* uboMgr = UBOManager::instance();
    uboMgr->createUBO("Matrices", sizeof(MatrixUBO), UBOBindingPoints::MATRICES);
    uboMgr->createUBO("Material", sizeof(MaterialUBO), UBOBindingPoints::MATERIAL);
    uboMgr->createUBO("Lighting", sizeof(LightingUBO), UBOBindingPoints::LIGHTING);
    uboMgr->createUBO("Instances", sizeof(InstancesUBO), UBOBindingPoints::INSTANCES);
    std::cout << "UnifiedRenderer: UBOs initialized at binding points 0-3" << std::endl;
    
    SSAO::init(width, height);
    
    int shadowSize = 4096;
    if (const char* env = std::getenv("CS_SHADOW_SIZE")) {
        int v = std::atoi(env);
        if (v >= 512 && v <= 8192) shadowSize = v;
    }
    Shadow::init(shadowSize);
    
    m_context.viewportWidth = width;
    m_context.viewportHeight = height;
    
    std::cout << "UnifiedRenderer: Initialized successfully" << std::endl;
    return true;
}

void UnifiedRenderer::resize(int width, int height) {
    m_context.viewportWidth = width;
    m_context.viewportHeight = height;
    SSAO::resize(width, height);
}

void UnifiedRenderer::setShaderRoot(const std::string& rootDir) {
    ShaderPath::setRoot(rootDir);
}

void UnifiedRenderer::beginFrame(Camera* camera, float time) {
    if (m_frameActive) {
        std::cerr << "UnifiedRenderer: beginFrame called without endFrame!" << std::endl;
    }
    
    m_frameActive = true;
    m_context.camera = camera;
    m_context.time = time;
    
    m_queue.clear();
    m_batcher.clear();
    
    m_stats = Stats();
}

void UnifiedRenderer::submit(IRenderable* renderable, bool castsShadows) {
    if (!m_frameActive) {
        std::cerr << "UnifiedRenderer: submit called outside of frame!" << std::endl;
        return;
    }
    
    if (!renderable) {
        return;
    }
    
    m_queue.submit(renderable, m_context, castsShadows);
}

void UnifiedRenderer::renderFrame(const RenderSettings& settings) {
    if (!m_frameActive || !m_context.camera) {
        std::cerr << "UnifiedRenderer: renderFrame called without valid frame!" << std::endl;
        return;
    }
    
    setupLighting(settings);
    
    Shadow::setEnabled(settings.shadowEnabled);
    
    if (settings.shadowEnabled) {
        renderShadowPass(settings);
    }
    
    renderScenePass(settings);
    
    SSAO::setEnabled(settings.ssaoEnabled);
    
    if (settings.ssaoEnabled) {
        renderSSAOPass(settings);
    }
    
    renderCompositePass(settings);
}

void UnifiedRenderer::setupLighting(const RenderSettings& settings) {
    LightingUBO lighting;
    
    // Main light (w component = intensity)
    glm::vec3 lightPos(settings.lightPosition[0], settings.lightPosition[1], settings.lightPosition[2]);
    glm::vec3 lightDiff(settings.lightDiffuse[0], settings.lightDiffuse[1], settings.lightDiffuse[2]);
    lighting.positions[0] = glm::vec4(lightPos, 1.0f);
    lighting.colors[0] = glm::vec4(lightDiff, 1.0f);
    lighting.count = 1;
    
    for (size_t i = 0; i < settings.lights.size() && lighting.count < 4; ++i) {
        const auto& extraLight = settings.lights[i];
        if (!extraLight.enabled) continue;
        
        glm::vec3 pos(extraLight.position[0], extraLight.position[1], extraLight.position[2]);
        glm::vec3 col(extraLight.diffuse[0], extraLight.diffuse[1], extraLight.diffuse[2]);
        
        lighting.positions[lighting.count] = glm::vec4(pos, extraLight.intensity);
        lighting.colors[lighting.count] = glm::vec4(col, 1.0f);
        lighting.count++;
    }
    
    lighting.ambientStrength = 0.2f;
    
    static auto lastDebugTime = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastDebugTime).count();
    
    if (elapsed >= 1) {
        lastDebugTime = now;
        std::cout << "[LightingUBO] count=" << lighting.count << ", ambient=" << lighting.ambientStrength << std::endl;
        for (int i = 0; i < lighting.count; ++i) {
            std::cout << "  Light[" << i << "]: pos=(" << lighting.positions[i].x << "," 
                      << lighting.positions[i].y << "," << lighting.positions[i].z 
                      << "), intensity=" << lighting.positions[i].w << std::endl;
        }
    }
    
    UBOManager::instance()->updateUBO("Lighting", &lighting, sizeof(LightingUBO));
}

void UnifiedRenderer::renderShadowPass(const RenderSettings& settings) {
    if (!Shadow::isEnabled()) return;
    
    m_context.currentPass = RenderContext::Pass::SHADOW;
    
    m_queue.sortShadow();
    
    static auto lastShadowPassDebugTime = std::chrono::steady_clock::now();
    auto nowShadowPass = std::chrono::steady_clock::now();
    auto elapsedShadowPass = std::chrono::duration_cast<std::chrono::seconds>(nowShadowPass - lastShadowPassDebugTime).count();
    
    if (elapsedShadowPass >= 1) {
        lastShadowPassDebugTime = nowShadowPass;
        std::cout << "[ShadowPass] Shadow queue size=" << m_queue.getShadowQueueSize() 
                  << ", Shadow::isEnabled()=" << (Shadow::isEnabled() ? "true" : "false") 
                  << ", extra lights=" << settings.lights.size() << std::endl;
    }
    
    glm::vec3 sceneCenter(0.0f, 0.0f, 0.0f);
    float sceneRadius = 100.0f;
    
    // Main light shadow (shadow index 0)
    glm::vec3 lightPos(settings.lightPosition[0], settings.lightPosition[1], settings.lightPosition[2]);
    glm::vec3 lightDiff(settings.lightDiffuse[0], settings.lightDiffuse[1], settings.lightDiffuse[2]);
    Light mainLight(lightPos, Colour(lightDiff.r, lightDiff.g, lightDiff.b, 1.0f));
    
    Shadow::beginShadowPass(0, &mainLight, sceneCenter, sceneRadius);
    Shadow::setBias(settings.shadowBias);
    Shadow::setSoftness(settings.shadowSoftness);
    m_queue.executeShadow(m_context);
    Shadow::endShadowPass();
    
    // Extra lights shadows (shadow indices 1-3)
    int shadowIndex = 1;
    for (const auto& extraLight : settings.lights) {
        if (shadowIndex >= 4) break; // Max 4 shadow maps
        if (!extraLight.enabled || !extraLight.castsShadow) continue;
        
        glm::vec3 pos(extraLight.position[0], extraLight.position[1], extraLight.position[2]);
        glm::vec3 diff(extraLight.diffuse[0], extraLight.diffuse[1], extraLight.diffuse[2]);
        Light light(pos, Colour(diff.r, diff.g, diff.b, 1.0f));
        Shadow::beginShadowPass(shadowIndex, &light, sceneCenter, sceneRadius);
        m_queue.executeShadow(m_context);
        Shadow::endShadowPass();
        
        shadowIndex++;
    }
    
    m_stats.drawCalls += m_queue.getShadowQueueSize();
}

void UnifiedRenderer::renderScenePass(const RenderSettings& settings) {
    m_context.currentPass = RenderContext::Pass::SCENE;
    
    m_queue.sortMain();
    
    SSAO::beginScenePass();
    
    glViewport(0, 0, m_context.viewportWidth, m_context.viewportHeight);
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    
    for (int i = 0; i < 4; ++i) {
        m_context.shadowMatrices[i] = Shadow::getLightSpaceMatrix(i);
    }
    
    m_context.viewPosition = m_context.camera ? m_context.camera->getEye() : glm::vec3(0.0f);
    
    m_context.shadowsEnabled = settings.shadowEnabled && Shadow::isEnabled();
    
    static auto lastShadowDebugTime = std::chrono::steady_clock::now();
    auto nowShadow = std::chrono::steady_clock::now();
    auto elapsedShadow = std::chrono::duration_cast<std::chrono::seconds>(nowShadow - lastShadowDebugTime).count();
    
    if (elapsedShadow >= 1) {
        lastShadowDebugTime = nowShadow;
        std::cout << "[RenderScenePass] shadowsEnabled=" << (m_context.shadowsEnabled ? "true" : "false")
                  << ", Shadow::isEnabled()=" << (Shadow::isEnabled() ? "true" : "false")
                  << ", settings.shadowEnabled=" << (settings.shadowEnabled ? "true" : "false") << std::endl;
    }
    
    for (int i = 0; i < 4; ++i) {
        glActiveTexture(GL_TEXTURE8 + i);  // Use high texture units to avoid conflicts
        glBindTexture(GL_TEXTURE_2D, Shadow::getShadowMapTexture(i));
    }
    
    m_queue.executeMain(m_context);
    
    SSAO::endScenePass();
    
    m_stats.drawCalls += m_queue.getMainQueueSize();
}

void UnifiedRenderer::renderSSAOPass(const RenderSettings& settings) {
    m_context.currentPass = RenderContext::Pass::SSAO;
    
    if (!SSAO::isEnabled()) return;
    
    SSAO::setRadius(settings.ssaoRadius);
    SSAO::setIntensity(settings.ssaoIntensity);
    SSAO::setBias(settings.ssaoBias);
}

void UnifiedRenderer::renderCompositePass(const RenderSettings& settings) {
    m_context.currentPass = RenderContext::Pass::COMPOSITE;
    
    SSAO::renderComposite(m_context.camera);
}

void UnifiedRenderer::endFrame() {
    if (!m_frameActive) {
        std::cerr << "UnifiedRenderer: endFrame called without beginFrame!" << std::endl;
    }
    
    m_frameActive = false;
    m_context.camera = nullptr;
}

void UnifiedRenderer::cleanup() {
    m_queue.clear();
    m_batcher.clear();
    
    UBOManager::instance()->cleanup();
}

} // namespace gfx
