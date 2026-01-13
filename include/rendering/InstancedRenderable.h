#pragma once

#include "rendering/IRenderable.h"
#include "core/Types.h"
#include "materials/Material.h"
#include <glm/glm.hpp>
#include <vector>

namespace gfx {

/**
 * @brief Instanced renderable for repeated geometry
 * 
 * Efficiently renders many copies of the same geometry with different transforms.
 * Uses glDrawElementsInstanced() with instance matrix UBO.
 */
class InstancedRenderable : public IRenderable {
public:
    InstancedRenderable(GeometryHandle geometry, Material* material);
    ~InstancedRenderable() override;
    
    // Add an instance with transform
    void addInstance(const glm::mat4& transform);
    
    // Clear all instances
    void clearInstances();
    
    // Get instance count
    size_t getInstanceCount() const { return m_instances.size(); }
    
    // IRenderable interface
    void render(const RenderContext& context) override;
    void renderShadow(const RenderContext& context) override;
    Material* getMaterial() const override { return m_material; }
    const glm::mat4& getTransform() const override;
    uint64_t getSortKey(const RenderContext& context) const override;
    bool isInstanced() const override { return true; }
    
private:
    GeometryHandle m_geometry;
    Material* m_material;
    std::vector<glm::mat4> m_instances;
    glm::mat4 m_dummyTransform; // For getTransform() interface
    
    void renderInternal(const RenderContext& context, bool shadowPass);
    void uploadInstanceMatrices(const RenderContext& context);
};

} // namespace gfx
