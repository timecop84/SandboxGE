#pragma once

#include "../rendering/IRenderable.h"
#include "../materials/Material.h"
#include "../core/Types.h"
#include <glm/glm.hpp>

namespace gfx {

class SphereRenderable : public IRenderable {
public:
    enum class MaterialPreset {
        Phong,
        Silk,
        SilkPBR
    };

    SphereRenderable(float radius, const glm::vec3& position, const glm::vec3& color);
    ~SphereRenderable() override;
    
    // IRenderable interface
    void render(const RenderContext& context) override;
    void renderShadow(const RenderContext& context) override;
    uint64_t getSortKey(const RenderContext& context) const override;
    Material* getMaterial() const override { return m_material; }
    const glm::mat4& getTransform() const override { return m_transform; }
    
    // Configuration
    void setPosition(const glm::vec3& position);
    void setRadius(float radius);
    void setColor(const glm::vec3& color);
    void setWireframe(bool wireframe);
    void setMaterial(Material* material);
    void setMaterialPreset(MaterialPreset preset);
    
    const glm::vec3& getPosition() const { return m_position; }
    float getRadius() const { return m_radius; }
    
private:
    void createGeometry();
    void updateTransform();
    void rebuildMaterial();
    
    GeometryHandle m_geometry;
    Material* m_material;
    glm::mat4 m_transform;
    
    float m_radius;
    glm::vec3 m_position;
    glm::vec3 m_color;
    bool m_wireframe;
    MaterialPreset m_materialPreset = MaterialPreset::Phong;
};

} // namespace gfx
