#pragma once

#include <glm/glm.hpp>
#include <array>

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
};

} // namespace sandbox
