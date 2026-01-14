#pragma once

#include "../core/Types.h"
#include "../rendering/UBOStructures.h"
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>

namespace gfx {

// Forward declarations
struct RenderContext;

// shader + params + textures for an object
class Material {
public:
    Material(const std::string& shaderName);
    
    // Parameter setters
    void setAmbient(const glm::vec3& color);
    void setDiffuse(const glm::vec3& color);
    void setSpecular(const glm::vec3& color);
    void setShininess(float value);
    void setMetallic(float value);
    void setRoughness(float value);
    void setTexture(const std::string& name, TextureHandle texture);
    
    // Get MaterialUBO data
    const MaterialUBO& getUBOData() const { return m_uboData; }
    
    // Bind material state to OpenGL
    void bind(const RenderContext& context);
    void unbind();
    
    // Get unique ID for sorting
    MaterialID getMaterialID() const { return m_materialID; }
    const std::string& getShaderName() const { return m_shaderName; }
    
    // Factory methods for common materials
    static Material* createPhong(const glm::vec3& diffuse = glm::vec3(0.8f));
    static Material* createSilk(const glm::vec3& diffuse = glm::vec3(0.8f));
    static Material* createSilkPBR(const glm::vec3& diffuse = glm::vec3(0.8f));
    static Material* createShadow(); // Depth-only material for shadow pass
    
private:
    std::string m_shaderName;
    MaterialID m_materialID;
    MaterialUBO m_uboData;
    std::unordered_map<std::string, TextureHandle> m_textures;
    
    // Generate hash from shader name for sorting
    void generateMaterialID();
};

} // namespace gfx
