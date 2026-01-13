#include "rendering/RenderQueue.h"
#include <algorithm>

namespace gfx {

RenderQueue::RenderQueue() {
    // Reserve space for typical frame
    m_mainQueue.reserve(256);
    m_shadowQueue.reserve(256);
}

void RenderQueue::submit(IRenderable* renderable, const RenderContext& context, bool castsShadows) {
    if (!renderable) {
        return;
    }
    
    // Generate sort key
    uint64_t sortKey = renderable->getSortKey(context);
    
    // Add to main queue
    m_mainQueue.emplace_back(renderable, sortKey, castsShadows);
    
    // Add to shadow queue if enabled
    if (castsShadows && renderable->castsShadows()) {
        m_shadowQueue.emplace_back(renderable, sortKey, true);
    }
}

void RenderQueue::sortMain() {
    // Sort by sort key (shader → material → depth)
    std::sort(m_mainQueue.begin(), m_mainQueue.end(),
        [](const RenderCommand& a, const RenderCommand& b) {
            return a.sortKey < b.sortKey;
        });
}

void RenderQueue::sortShadow() {
    // Sort shadow queue (shader → depth, no material)
    // For shadow pass, we primarily care about shader batching
    std::sort(m_shadowQueue.begin(), m_shadowQueue.end(),
        [](const RenderCommand& a, const RenderCommand& b) {
            // Extract shader part (top 24 bits) and depth (bottom 16 bits)
            uint64_t shaderA = a.sortKey >> 40;
            uint64_t shaderB = b.sortKey >> 40;
            if (shaderA != shaderB) {
                return shaderA < shaderB;
            }
            // Same shader, sort by depth
            uint16_t depthA = static_cast<uint16_t>(a.sortKey & 0xFFFF);
            uint16_t depthB = static_cast<uint16_t>(b.sortKey & 0xFFFF);
            return depthA < depthB;
        });
}

void RenderQueue::executeMain(RenderContext& context) {
    context.currentPass = RenderContext::Pass::SCENE;
    
    for (const auto& cmd : m_mainQueue) {
        if (cmd.renderable) {
            cmd.renderable->render(context);
        }
    }
}

void RenderQueue::executeShadow(RenderContext& context) {
    context.currentPass = RenderContext::Pass::SHADOW;
    
    for (const auto& cmd : m_shadowQueue) {
        if (cmd.renderable) {
            cmd.renderable->renderShadow(context);
        }
    }
}

void RenderQueue::clear() {
    m_mainQueue.clear();
    m_shadowQueue.clear();
}

} // namespace gfx
