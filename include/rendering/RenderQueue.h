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

    enum class MainPassFilter {
        All,
        OmitRefraction,
        OnlyRefraction
    };

    // Execute main queue rendering.
    void executeMain(RenderContext& context, MainPassFilter filter = MainPassFilter::All);

    // Execute shadow queue rendering.
    void executeShadow(RenderContext& context);

    // Clear all queues.
    void clear();

    // Queue sizes (mostly for debugging).
    size_t getMainQueueSize() const { return m_mainQueue.size(); }
    size_t getShadowQueueSize() const { return m_shadowQueue.size(); }

    bool hasRefraction() const { return m_hasRefraction; }
    
private:
    std::vector<RenderCommand> m_mainQueue;
    std::vector<RenderCommand> m_shadowQueue;
    bool m_hasRefraction = false;
};

} // namespace sandbox
