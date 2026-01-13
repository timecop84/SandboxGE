#pragma once

#include "core/RenderContext.h"
#include <glm/glm.hpp>

namespace gfx {

class Material;

/**
 * @brief Base interface for all renderable objects
 * 
 * Provides polymorphic rendering with virtual methods for:
 * - Main scene rendering
 * - Shadow pass rendering
 * - Material access
 * - Transform access
 * - Bounding box for culling (future)
 */
class IRenderable {
public:
    virtual ~IRenderable() = default;
    
    /**
     * @brief Render the object in the main scene pass
     */
    virtual void render(const RenderContext& context) = 0;
    
    /**
     * @brief Render the object in the shadow pass (depth-only)
     */
    virtual void renderShadow(const RenderContext& context) = 0;
    
    /**
     * @brief Get the material for this renderable
     */
    virtual Material* getMaterial() const = 0;
    
    /**
     * @brief Get the world transform matrix
     */
    virtual const glm::mat4& getTransform() const = 0;
    
    /**
     * @brief Get bounding box for frustum culling (future feature)
     */
    virtual void getBoundingBox(glm::vec3& min, glm::vec3& max) const {
        min = glm::vec3(-1.0f);
        max = glm::vec3(1.0f);
    }
    
    /**
     * @brief Get sort key for render queue ordering
     * Constructed from shader hash, material ID, and depth
     */
    virtual uint64_t getSortKey(const RenderContext& context) const = 0;
    
    /**
     * @brief Check if this renderable uses instancing
     */
    virtual bool isInstanced() const { return false; }
    
    /**
     * @brief Should this object cast shadows?
     */
    virtual bool castsShadows() const { return true; }
};

} // namespace gfx
