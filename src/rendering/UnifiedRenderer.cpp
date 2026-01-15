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
#include <core/ResourceManager.h>
#include <utils/ShaderLib.h>
#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <chrono>
#include <cstdlib>
#include <string>

namespace sandbox {

static std::string getEnvString(const char* name) {
#if defined(_MSC_VER)
    char* value = nullptr;
    size_t len = 0;
    if (_dupenv_s(&value, &len, name) != 0 || !value) return {};
    std::string out(value);
    std::free(value);
    return out;
#else
    const char* value = std::getenv(name);
    return value ? std::string(value) : std::string{};
#endif
}

UnifiedRenderer::UnifiedRenderer() {
}

UnifiedRenderer::~UnifiedRenderer() {
    cleanup();
}

bool UnifiedRenderer::initialize(int width, int height) {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDisable(GL_CULL_FACE);
    glClearColor(0.3f, 0.3f, 0.3f, 1.0f);

    auto* device = ResourceManager::instance()->getDevice();
    SSAO::setDevice(device);
    Shadow::setDevice(device);
    ShaderLib::instance()->setDevice(device);
    
    UBOManager* uboMgr = UBOManager::instance();
    uboMgr->createUBO("Matrices", sizeof(MatrixUBO), UBOBindingPoints::MATRICES);
    uboMgr->createUBO("Material", sizeof(MaterialUBO), UBOBindingPoints::MATERIAL);
    uboMgr->createUBO("Lighting", sizeof(LightingUBO), UBOBindingPoints::LIGHTING);
    uboMgr->createUBO("Instances", sizeof(InstancesUBO), UBOBindingPoints::INSTANCES);
    
    SSAO::init(width, height);
    
    int shadowSize = 4096;
    {
        const std::string env = getEnvString("CS_SHADOW_SIZE");
        int v = env.empty() ? 0 : std::atoi(env.c_str());
        if (v >= 512 && v <= 8192) shadowSize = v;
    }
    Shadow::init(shadowSize);
    
    m_context.viewportWidth = width;
    m_context.viewportHeight = height;
    
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

    m_lightSlotCount = 0;
    for (auto& s : m_lightSlots) s = ShadowLightSlot{};
    
    // Main light (w component = intensity)
    glm::vec3 lightPos(settings.lightPosition[0], settings.lightPosition[1], settings.lightPosition[2]);
    glm::vec3 lightDiff(settings.lightDiffuse[0], settings.lightDiffuse[1], settings.lightDiffuse[2]);
    lighting.positions[0] = glm::vec4(lightPos, 1.0f);
    lighting.colors[0] = glm::vec4(lightDiff, 1.0f); // a=castsShadow
    lighting.count = 1;

    m_lightSlots[0].position = lightPos;
    m_lightSlots[0].color = lightDiff;
    m_lightSlots[0].castsShadow = true;
    m_lightSlotCount = 1;
    
    for (size_t i = 0; i < settings.lights.size() && lighting.count < 4; ++i) {
        const auto& extraLight = settings.lights[i];
        if (!extraLight.enabled) continue;
        
        glm::vec3 pos(extraLight.position[0], extraLight.position[1], extraLight.position[2]);
        glm::vec3 col(extraLight.diffuse[0], extraLight.diffuse[1], extraLight.diffuse[2]);
        
        lighting.positions[lighting.count] = glm::vec4(pos, extraLight.intensity);
        lighting.colors[lighting.count] = glm::vec4(col, extraLight.castsShadow ? 1.0f : 0.0f);

        m_lightSlots[lighting.count].position = pos;
        m_lightSlots[lighting.count].color = col;
        m_lightSlots[lighting.count].castsShadow = extraLight.castsShadow;

        lighting.count++;
    }

    m_lightSlotCount = lighting.count;
    
    lighting.ambientStrength = 0.2f;
    
    UBOManager::instance()->updateUBO("Lighting", &lighting, sizeof(LightingUBO));
}

void UnifiedRenderer::renderShadowPass(const RenderSettings& settings) {
    if (!Shadow::isEnabled()) return;
    
    m_context.currentPass = RenderContext::Pass::SHADOW;
    
    m_queue.sortShadow();
    
    glm::vec3 sceneCenter(0.0f, 0.0f, 0.0f);
    float sceneRadius = 100.0f;
    
    Shadow::setBias(settings.shadowBias);
    Shadow::setSoftness(settings.shadowSoftness);

    // Shadow maps are indexed to match LightingUBO slots (0..3)
    for (int li = 0; li < m_lightSlotCount && li < Shadow::MAX_SHADOW_LIGHTS; ++li) {
        if (!m_lightSlots[li].castsShadow) continue;

        const auto& slot = m_lightSlots[li];
        Light light(slot.position, Colour(slot.color.r, slot.color.g, slot.color.b, 1.0f));
        Shadow::beginShadowPass(li, &light, sceneCenter, sceneRadius);
        m_queue.executeShadow(m_context);
        Shadow::endShadowPass();
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
        const bool castsShadow = (i < m_lightSlotCount) ? m_lightSlots[i].castsShadow : false;
        m_context.shadowMatrices[i] = castsShadow ? Shadow::getLightSpaceMatrix(i) : glm::mat4(1.0f);
    }
    
    m_context.viewPosition = m_context.camera ? m_context.camera->getEye() : glm::vec3(0.0f);
    
    m_context.shadowsEnabled = settings.shadowEnabled && Shadow::isEnabled();
    
    for (int i = 0; i < 4; ++i) {
        Shadow::bindShadowMap(i, 8 + i);  // Use high texture units to avoid conflicts
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
    (void)settings;
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

} // namespace sandbox
