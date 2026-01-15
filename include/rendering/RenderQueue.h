#pragma once

#include "rendering/IRenderable.h"
#include "core/RenderContext.h"
#include <vector>
#include <cstdint>

namespace sandbox {

struct RenderCommand {
    IRenderable* renderable = nullptr;
    uint64_t sortKey = 0;
    bool castsShadows = true;
    
    RenderCommand() = default;
    RenderCommand(IRenderable* r, uint64_t key, bool shadows)
        : renderable(r), sortKey(key), castsShadows(shadows) {}
};

// Dual queue system: main pass (shader/material/depth), shadow pass (shader/depth)
class RenderQueue {
public:
    RenderQueue() = default;
    
    void submit(IRenderable* renderable, const RenderContext& context, bool castsShadows = true);
    
    void sortMain();
    void sortShadow();

    // Execute main queue rendering.
    void executeMain(RenderContext& context);

    // Execute shadow queue rendering.
    void executeShadow(RenderContext& context);

    // Clear all queues.
    void clear();

    // Queue sizes (mostly for debugging).
    size_t getMainQueueSize() const { return m_mainQueue.size(); }
    size_t getShadowQueueSize() const { return m_shadowQueue.size(); }
    
private:
    std::vector<RenderCommand> m_mainQueue;
    std::vector<RenderCommand> m_shadowQueue;
};

} // namespace sandbox
