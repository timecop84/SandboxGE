#pragma once

#include "core/Types.h"
#include "materials/Material.h"
#include "rendering/InstancedRenderable.h"
#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>

namespace gfx {

// Batches repeated geometry+material combos into instanced draw calls
class InstanceBatcher {
public:
    InstanceBatcher();
    
    void add(GeometryHandle geometry, Material* material, const glm::mat4& transform);
    std::vector<InstancedRenderable*> finalize();
    void clear();
    
    size_t getBatchCount() const { return m_batches.size(); }
    size_t getTotalInstances() const;
    
private:
    // Key for identifying unique geometry+material combinations
    struct BatchKey {
        GeometryHandle geometry;
        Material* material;
        
        bool operator==(const BatchKey& other) const {
            return geometry == other.geometry && material == other.material;
        }
    };
    
    // Hash function for BatchKey
    struct BatchKeyHash {
        size_t operator()(const BatchKey& key) const {
            size_t h1 = std::hash<void*>{}(key.geometry.get());
            size_t h2 = std::hash<void*>{}(key.material);
            return h1 ^ (h2 << 1);
        }
    };
    
    // Batch data: list of transforms for each geometry+material combo
    struct BatchData {
        std::vector<glm::mat4> transforms;
        InstancedRenderable* renderable = nullptr;
    };
    
    std::unordered_map<BatchKey, BatchData, BatchKeyHash> m_batches;
    std::vector<InstancedRenderable*> m_finalizedRenderables;
};

} // namespace gfx
