#include "rendering/InstanceBatcher.h"

namespace gfx {

InstanceBatcher::InstanceBatcher() {
}

void InstanceBatcher::add(GeometryHandle geometry, Material* material, const glm::mat4& transform) {
    if (!geometry || !material) {
        return;
    }
    
    BatchKey key{geometry, material};
    auto& batch = m_batches[key];
    
    // Create renderable if first time seeing this combo
    if (!batch.renderable) {
        batch.renderable = new InstancedRenderable(geometry, material);
    }
    
    batch.transforms.push_back(transform);
}

std::vector<InstancedRenderable*> InstanceBatcher::finalize() {
    m_finalizedRenderables.clear();
    
    for (auto& pair : m_batches) {
        auto& batch = pair.second;
        if (batch.renderable && !batch.transforms.empty()) {
            // Add all transforms to the instanced renderable
            for (const auto& transform : batch.transforms) {
                batch.renderable->addInstance(transform);
            }
            m_finalizedRenderables.push_back(batch.renderable);
        }
    }
    
    return m_finalizedRenderables;
}

void InstanceBatcher::clear() {
    // Clean up allocated renderables
    for (auto& pair : m_batches) {
        if (pair.second.renderable) {
            pair.second.renderable->clearInstances();
            delete pair.second.renderable;
        }
    }
    m_batches.clear();
    m_finalizedRenderables.clear();
}

size_t InstanceBatcher::getTotalInstances() const {
    size_t total = 0;
    for (const auto& pair : m_batches) {
        total += pair.second.transforms.size();
    }
    return total;
}

} // namespace gfx
