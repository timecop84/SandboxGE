#pragma once

#include "core/RenderContext.h"
#include "core/Types.h"
#include "rendering/RenderQueue.h"
#include "rendering/InstanceBatcher.h"
#include <RenderSettings.h>

namespace gfx {

class IRenderable;

/**
 * @brief Unified rendering pipeline orchestrator
 * 
 * Modern renderer that uses Material system, RenderQueue, and IRenderable interface.
 * Replaces the old GraphicsEngine with a cleaner submission-based API.
 */
class UnifiedRenderer {
public:
    UnifiedRenderer();
    ~UnifiedRenderer();
    
    /**
     * @brief Initialize rendering system
     * @param width Viewport width
     * @param height Viewport height
     * @return true if successful
     */
    bool initialize(int width, int height);
    
    /**
     * @brief Resize viewport
     */
    void resize(int width, int height);
    
    /**
     * @brief Set shader root directory
     */
    void setShaderRoot(const std::string& rootDir);
    
    /**
     * @brief Begin a new frame
     * @param camera Active camera
     * @param time Current time in seconds
     */
    void beginFrame(Camera* camera, float time);
    
    /**
     * @brief Submit a renderable object for this frame
     * @param renderable Object to render
     * @param castsShadows Whether object should cast shadows
     */
    void submit(IRenderable* renderable, bool castsShadows = true);
    
    /**
     * @brief Render the complete frame with all passes
     * @param settings Rendering configuration
     */
    void renderFrame(const RenderSettings& settings);
    
    /**
     * @brief End the current frame
     */
    void endFrame();
    
    /**
     * @brief Cleanup resources
     */
    void cleanup();
    
    /**
     * @brief Get render statistics
     */
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
    
    // Rendering passes
    void renderShadowPass(const RenderSettings& settings);
    void renderScenePass(const RenderSettings& settings);
    void renderSSAOPass(const RenderSettings& settings);
    void renderCompositePass(const RenderSettings& settings);
    
    void setupLighting(const RenderSettings& settings);
};

} // namespace gfx
