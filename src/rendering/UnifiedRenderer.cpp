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
#include <rhi/Device.h>
#include <utils/ShaderLib.h>
#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <chrono>
#include <cstdlib>
#include <string>
#include <cmath>

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

static GLuint toGlHandle(const rhi::Texture* tex) {
    if (!tex) return 0;
    return static_cast<GLuint>(tex->nativeHandle());
}

static std::array<float, 4> computeCascadeSplits(const Camera* camera, const RenderSettings& settings) {
    std::array<float, 4> splits{};
    if (!camera) {
        return splits;
    }
    const float nearPlane = camera->getNear();
    const float farPlane = std::min(camera->getFar(), settings.cascadeMaxDistance);
    const float clipRange = farPlane - nearPlane;
    const float minZ = nearPlane;
    const float maxZ = nearPlane + clipRange;
    const float range = maxZ - minZ;
    const float ratio = maxZ / minZ;
    const int count = std::max(1, std::min(settings.cascadeCount, 4));

    for (int i = 0; i < count; ++i) {
        float p = (i + 1) / static_cast<float>(count);
        float log = minZ * std::pow(ratio, p);
        float uniform = minZ + range * p;
        float d = settings.cascadeSplitLambda * (log - uniform) + uniform;
        splits[i] = d;
    }
    for (int i = count; i < 4; ++i) {
        splits[i] = maxZ;
    }
    return splits;
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
    m_refractionWidth = width;
    m_refractionHeight = height;
    if (auto* device = ResourceManager::instance()->getDevice()) {
        m_refractionSource = device->createTexture(width, height, rhi::TextureFormat::RGB16F);
    }
    
    return true;
}

void UnifiedRenderer::resize(int width, int height) {
    m_context.viewportWidth = width;
    m_context.viewportHeight = height;
    SSAO::resize(width, height);
    if (width != m_refractionWidth || height != m_refractionHeight) {
        m_refractionWidth = width;
        m_refractionHeight = height;
        if (auto* device = ResourceManager::instance()->getDevice()) {
            m_refractionSource = device->createTexture(width, height, rhi::TextureFormat::RGB16F);
        } else {
            m_refractionSource.reset();
        }
    }
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

    m_context.useCascades = settings.useCascadedShadows;
    m_context.debugCascades = settings.debugCascades;
    m_context.cascadeCount = settings.useCascadedShadows ? std::max(1, std::min(settings.cascadeCount, 4)) : 0;
    m_context.cascadeSplits = settings.useCascadedShadows ? computeCascadeSplits(m_context.camera, settings)
                                                          : std::array<float, 4>{};
    m_context.envMapTextureId = settings.envMapTextureId;
    m_context.envMapIntensity = settings.envMapIntensity;
    m_context.iblEnabled = settings.iblEnabled;
    m_context.iblIntensity = settings.iblIntensity;
    m_context.iblIrradianceMap = settings.iblIrradianceMap;
    m_context.iblPrefilterMap = settings.iblPrefilterMap;
    m_context.iblBrdfLut = settings.iblBrdfLut;
    m_context.numShadowMaps = 0;
    m_context.lightCastsShadow = {0, 0, 0, 0};
    m_context.lightShadowMapIndex = {-1, -1, -1, -1};
    
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

    m_context.mainLightPosition = lightPos;
    m_context.mainLightColor = lightDiff;
    
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

    m_context.numShadowMaps = 0;
    m_context.lightCastsShadow = {0, 0, 0, 0};
    m_context.lightShadowMapIndex = {-1, -1, -1, -1};

    int shadowMapIndex = 0;

    if (settings.useCascadedShadows && m_lightSlotCount > 0 && m_lightSlots[0].castsShadow) {
        const auto& slot = m_lightSlots[0];
        Light light(slot.position, Colour(slot.color.r, slot.color.g, slot.color.b, 1.0f));
        const int cascadeCount = std::max(1, std::min(settings.cascadeCount, Shadow::MAX_SHADOW_LIGHTS));

        float splitNear = m_context.camera ? m_context.camera->getNear() : 0.1f;
        for (int c = 0; c < cascadeCount; ++c) {
            float splitFar = m_context.cascadeSplits[c];
            Shadow::beginShadowCascade(c, &light, m_context.camera, splitNear, splitFar);
            m_queue.executeShadow(m_context);
            Shadow::endShadowPass();
            splitNear = splitFar;
        }

        m_context.lightCastsShadow[0] = 1;
        m_context.lightShadowMapIndex[0] = 0;
        shadowMapIndex = cascadeCount;
        m_context.numShadowMaps = cascadeCount;
    }

    for (int li = 0; li < m_lightSlotCount && shadowMapIndex < Shadow::MAX_SHADOW_LIGHTS; ++li) {
        if (li == 0 && settings.useCascadedShadows) continue;
        if (!m_lightSlots[li].castsShadow) continue;

        const auto& slot = m_lightSlots[li];
        Light light(slot.position, Colour(slot.color.r, slot.color.g, slot.color.b, 1.0f));
        Shadow::beginShadowPass(shadowMapIndex, &light, sceneCenter, sceneRadius);
        m_queue.executeShadow(m_context);
        Shadow::endShadowPass();

        m_context.lightCastsShadow[li] = 1;
        m_context.lightShadowMapIndex[li] = shadowMapIndex;
        shadowMapIndex++;
        m_context.numShadowMaps = shadowMapIndex;
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
    
    for (int i = 0; i < 4; ++i) {
        Shadow::bindShadowMap(i, 8 + i);  // Use high texture units to avoid conflicts
    }
    
    m_context.refractionSourceTexture = 0;

    m_queue.executeMain(m_context, RenderQueue::MainPassFilter::OmitRefraction);

    if (m_queue.hasRefraction()) {
        GLuint destTex = toGlHandle(m_refractionSource.get());
        if (destTex) {
            GLenum readBuffer = SSAO::isEnabled() ? GL_COLOR_ATTACHMENT0 : GL_BACK;
            glReadBuffer(readBuffer);
            glBindTexture(GL_TEXTURE_2D, destTex);
            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0,
                                m_context.viewportWidth, m_context.viewportHeight);
            glBindTexture(GL_TEXTURE_2D, 0);
            m_context.refractionSourceTexture = destTex;
        }
        m_queue.executeMain(m_context, RenderQueue::MainPassFilter::OnlyRefraction);
    }
    
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
