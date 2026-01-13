#pragma once

#include "rendering/IRenderable.h"
#include "materials/Material.h"
#include "core/Types.h"
#include <glm/glm.hpp>

namespace gfx {

class FloorRenderable : public IRenderable {
public:
    FloorRenderable(float width, float length, const glm::vec3& position, const glm::vec3& color);
    ~FloorRenderable() override;
    
    // IRenderable interface
    void render(const RenderContext& context) override;
    void renderShadow(const RenderContext& context) override;
    uint64_t getSortKey(const RenderContext& context) const override;
    Material* getMaterial() const override { return m_material; }
    const glm::mat4& getTransform() const override { return m_transform; }
    
    // Configuration
    void setPosition(const glm::vec3& position);
    void setColor(const glm::vec3& color);
    void setWireframe(bool wireframe);
    
    const glm::vec3& getPosition() const { return m_position; }
    
private:
    void createGeometry();
    void updateTransform();
    
    GeometryHandle m_geometry;
    Material* m_material;
    glm::mat4 m_transform;
    
    float m_width;
    float m_length;
    glm::vec3 m_position;
    glm::vec3 m_color;
    bool m_wireframe;
};

} // namespace gfx
