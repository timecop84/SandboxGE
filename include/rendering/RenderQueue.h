#pragma once

#include "rendering/IRenderable.h"
#include "core/RenderContext.h"
#include <vector>
#include <cstdint>

namespace gfx {

/**
 * @brief Render command for queue sorting
 */
struct RenderCommand {
    IRenderable* renderable = nullptr;
    uint64_t sortKey = 0;
    bool castsShadows = true;
    
    RenderCommand() = default;
    RenderCommand(IRenderable* r, uint64_t key, bool shadows)
        : renderable(r), sortKey(key), castsShadows(shadows) {}
};

/**
 * @brief Dual render queue system for main and shadow passes
 * 
 * Manages submission, sorting, and execution of render commands.
 * Main queue: sorted by shader → material → depth (minimize state changes)
 * Shadow queue: sorted by shader → depth (no materials needed)
 */
class RenderQueue {
public:
    RenderQueue();
    
    /**
     * @brief Submit a renderable to the queue
     * @param renderable Object to render
     * @param context Render context for sort key generation
     * @param castsShadows Whether to add to shadow queue
     */
    void submit(IRenderable* renderable, const RenderContext& context, bool castsShadows = true);
    
    /**
     * @brief Sort main queue by shader-material-depth
     */
    void sortMain();
    
    /**
     * @brief Sort shadow queue by shader-depth
     */
    void sortShadow();
    
    /**
     * @brief Execute main queue rendering
     */
    void executeMain(RenderContext& context);
    
    /**
     * @brief Execute shadow queue rendering
     */
    void executeShadow(RenderContext& context);
    
    /**
     * @brief Clear all queues
     */
    void clear();
    
    /**
     * @brief Get queue sizes for debugging
     */
    size_t getMainQueueSize() const { return m_mainQueue.size(); }
    size_t getShadowQueueSize() const { return m_shadowQueue.size(); }
    
private:
    std::vector<RenderCommand> m_mainQueue;
    std::vector<RenderCommand> m_shadowQueue;
};

} // namespace gfx
