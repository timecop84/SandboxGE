#include "rendering/UnifiedRenderer.h"
#include "rendering/UBOManager.h"
#include "rendering/UBOStructures.h"
#include "rendering/IRenderable.h"
#include "SSAORenderer.h"
#include "ShadowRenderer.h"
#include "ShaderPathResolver.h"
#include <Camera.h>
#include <ShaderLib.h>
#include <iostream>

namespace gfx {

UnifiedRenderer::UnifiedRenderer() {
}

UnifiedRenderer::~UnifiedRenderer() {
    cleanup();
}

bool UnifiedRenderer::initialize(int width, int height) {
    std::cout << "UnifiedRenderer: Initializing..." << std::endl;
    
    // Initialize OpenGL state
    initializeOpenGLState();
    
    // Initialize UBO system
    initializeUBOs();
    
    // Initialize render passes
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

void UnifiedRenderer::initializeOpenGLState() {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDisable(GL_CULL_FACE);
    glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
}

void UnifiedRenderer::initializeUBOs() {
    UBOManager* uboMgr = UBOManager::instance();
    
    // Create UBOs with fixed binding points
    uboMgr->createUBO("Matrices", sizeof(MatrixUBO), UBOBindingPoints::MATRICES);
    uboMgr->createUBO("Material", sizeof(MaterialUBO), UBOBindingPoints::MATERIAL);
    uboMgr->createUBO("Lighting", sizeof(LightingUBO), UBOBindingPoints::LIGHTING);
    uboMgr->createUBO("Instances", sizeof(InstancesUBO), UBOBindingPoints::INSTANCES);
    
    std::cout << "UnifiedRenderer: UBOs initialized at binding points 0-3" << std::endl;
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
    
    // Clear queues
    m_queue.clear();
    m_batcher.clear();
    
    // Reset stats
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
    
    // Submit to queue
    m_queue.submit(renderable, m_context, castsShadows);
}

void UnifiedRenderer::renderFrame(const RenderSettings& settings) {
    if (!m_frameActive || !m_context.camera) {
        std::cerr << "UnifiedRenderer: renderFrame called without valid frame!" << std::endl;
        return;
    }
    
    // Setup lighting UBO (shared across all passes)
    setupLighting(settings);
    
    // Render shadow pass
    if (settings.shadowEnabled) {
        renderShadowPass(settings);
    }
    
    // Render main scene pass
    renderScenePass(settings);
    
    // Render SSAO pass (if enabled)
    if (settings.ssaoEnabled) {
        renderSSAOPass(settings);
    }
    
    // Composite pass
    renderCompositePass(settings);
}

void UnifiedRenderer::setupLighting(const RenderSettings& settings) {
    LightingUBO lighting;
    
    // Main light
    glm::vec3 lightPos(settings.lightPosition[0], settings.lightPosition[1], settings.lightPosition[2]);
    lighting.positions[0] = glm::vec4(lightPos, 1.0f);
    lighting.colors[0] = glm::vec4(settings.lightDiffuse[0], settings.lightDiffuse[1], settings.lightDiffuse[2], 1.0f);
    lighting.count = 1;
    lighting.ambientStrength = 0.2f;
    
    // Upload to UBO
    UBOManager::instance()->updateUBO("Lighting", &lighting, sizeof(LightingUBO));
}

void UnifiedRenderer::renderShadowPass(const RenderSettings& settings) {
    m_context.currentPass = RenderContext::Pass::SHADOW;
    
    // Sort shadow queue
    m_queue.sortShadow();
    
    // TODO: Need to pass Light* and scene bounds from scene graph
    // For now, skip shadow pass until we integrate with scene management
    // Shadow::beginShadowPass(lightIndex, light, sceneCenter, sceneRadius);
    // m_queue.executeShadow(m_context);
    // Shadow::endShadowPass();
    
    // m_stats.drawCalls += m_queue.getShadowQueueSize();
}

void UnifiedRenderer::renderScenePass(const RenderSettings& settings) {
    m_context.currentPass = RenderContext::Pass::SCENE;
    
    // Sort main queue by shader/material/depth
    m_queue.sortMain();
    
    // Clear framebuffer
    glViewport(0, 0, m_context.viewportWidth, m_context.viewportHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Execute main queue
    m_queue.executeMain(m_context);
    
    m_stats.drawCalls += m_queue.getMainQueueSize();
}

void UnifiedRenderer::renderSSAOPass(const RenderSettings& settings) {
    m_context.currentPass = RenderContext::Pass::SSAO;
    
    SSAO::setRadius(settings.ssaoRadius);
    SSAO::setIntensity(settings.ssaoIntensity);
    SSAO::setBias(settings.ssaoBias);
    
    // SSAO applies as post-process
    // (This would integrate with existing SSAO renderer)
}

void UnifiedRenderer::renderCompositePass(const RenderSettings& settings) {
    m_context.currentPass = RenderContext::Pass::COMPOSITE;
    
    // Composite final image
    // (This would integrate with existing composite logic)
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
    
    // Cleanup UBOs
    UBOManager::instance()->cleanup();
}

} // namespace gfx
