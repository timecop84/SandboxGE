#pragma once

#include "core/RenderContext.h"
#include "core/Types.h"
#include "rendering/RenderQueue.h"
#include "rendering/InstanceBatcher.h"
#include <core/RenderSettings.h>
#include <glm/vec3.hpp>
#include <array>
#include <string>
#include <memory>

namespace sandbox {

class IRenderable;
namespace rhi {
class Texture;
}

// Rendering pipeline using Material system and RenderQueue
class UnifiedRenderer {
public:
    UnifiedRenderer();
    ~UnifiedRenderer();
    
    bool initialize(int width, int height);
    void resize(int width, int height);
    void setShaderRoot(const std::string& rootDir);
    
    void beginFrame(Camera* camera, float time);
    void submit(IRenderable* renderable, bool castsShadows = true);
    void renderFrame(const RenderSettings& settings);
    void endFrame();
    void cleanup();
    
    struct Stats {
        size_t drawCalls = 0;
        size_t triangles = 0;
        size_t instances = 0;
        size_t stateChanges = 0;
    };
    const Stats& getStats() const { return m_stats; }
    
private:
    RenderContext m_context;
    RenderQueue m_queue;
    InstanceBatcher m_batcher;
    Stats m_stats;
    bool m_frameActive = false;
    std::unique_ptr<rhi::Texture> m_refractionSource;
    int m_refractionWidth = 0;
    int m_refractionHeight = 0;
    
    // Rendering passes
    void renderShadowPass(const RenderSettings& settings);
    void renderScenePass(const RenderSettings& settings);
    void renderSSAOPass(const RenderSettings& settings);
    void renderCompositePass(const RenderSettings& settings);
    
    void setupLighting(const RenderSettings& settings);

    struct ShadowLightSlot {
        bool castsShadow = false;
        glm::vec3 position{0.0f};
        glm::vec3 color{1.0f};
    };

    std::array<ShadowLightSlot, 4> m_lightSlots{};
    int m_lightSlotCount = 0;
};

} // namespace sandbox
