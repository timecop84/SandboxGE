#pragma once

#include <glm/glm.hpp>
#include <array>
#include <cstdint>

namespace sandbox {

// Forward declarations
class Camera;

// Per-frame context passed into render calls.
struct RenderContext {
    // Camera for view/projection matrices
    Camera* camera = nullptr;
    
    // Current frame time (seconds)
    float time = 0.0f;
    
    // Shadow matrices for shadow pass (one per light)
    std::array<glm::mat4, 4> shadowMatrices;
    
    // Viewport dimensions
    int viewportWidth = 1280;
    int viewportHeight = 720;

    // Cascaded shadow settings
    bool useCascades = false;
    int cascadeCount = 0;
    std::array<float, 4> cascadeSplits{};
    bool debugCascades = false;

    // Shadow mapping per light
    int numShadowMaps = 0;
    std::array<int, 4> lightCastsShadow{};
    std::array<int, 4> lightShadowMapIndex{};
    
    // Current render pass (for conditional logic in renderables)
    enum class Pass {
        SHADOW,
        SCENE,
        SSAO,
        COMPOSITE
    } currentPass = Pass::SCENE;
    
    // Light index for shadow pass (which light we're rendering from)
    int currentLightIndex = 0;
    
    // Whether shadows are enabled for this frame
    bool shadowsEnabled = false;
    
    // Camera/view position for specular calculations
    glm::vec3 viewPosition = glm::vec3(0.0f);

    // Main light properties (for non-UBO shaders like Refraction)
    glm::vec3 mainLightPosition = glm::vec3(0.0f);
    glm::vec3 mainLightColor = glm::vec3(1.0f);

    // Screen-space refraction source (GL texture handle)
    std::uint32_t refractionSourceTexture = 0;

    // Environment/IBL textures (GL texture handles)
    std::uint32_t envMapTextureId = 0;
    float envMapIntensity = 0.0f;
    bool iblEnabled = false;
    float iblIntensity = 0.0f;
    std::uint32_t iblIrradianceMap = 0;
    std::uint32_t iblPrefilterMap = 0;
    std::uint32_t iblBrdfLut = 0;
};

} // namespace sandbox
