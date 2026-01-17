#pragma once

#include "rendering/IRenderable.h"
#include "core/Types.h"
#include "materials/Material.h"
#include <glm/glm.hpp>
#include <memory>

namespace sandbox {

class MeshRenderable : public IRenderable {
public:
    MeshRenderable(GeometryHandle geometry, Material* material, const glm::mat4& transform);
    ~MeshRenderable() override;
    
    // IRenderable interface
    void render(const RenderContext& context) override;
    void renderShadow(const RenderContext& context) override;
    Material* getMaterial() const override { return m_material; }
    const glm::mat4& getTransform() const override { return m_transform; }
    uint64_t getSortKey(const RenderContext& context) const override;
    
    // Update transform
    void setTransform(const glm::mat4& transform) { m_transform = transform; }
    
    // Update material
    void setMaterial(Material* material) { m_material = material; }

    // Update geometry
    void setGeometry(GeometryHandle geometry) { m_geometry = geometry; }

    // Toggle wireframe rendering
    void setWireframe(bool wireframe) { m_wireframe = wireframe; }
    
private:
    GeometryHandle m_geometry;
    Material* m_material;
    glm::mat4 m_transform;
    bool m_wireframe = false;
    
    void renderInternal(const RenderContext& context, bool shadowPass);
};

} // namespace sandbox
