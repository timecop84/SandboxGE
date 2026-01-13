#pragma once

#include "core/RenderContext.h"
#include <glm/glm.hpp>

namespace gfx {

class Material;

class IRenderable {
public:
    virtual ~IRenderable() = default;
    
    virtual void render(const RenderContext& context) = 0;
    virtual void renderShadow(const RenderContext& context) = 0;
    virtual Material* getMaterial() const = 0;
    virtual const glm::mat4& getTransform() const = 0;
    
    virtual void getBoundingBox(glm::vec3& min, glm::vec3& max) const {
        min = glm::vec3(-1.0f);
        max = glm::vec3(1.0f);
    }
    
    // sort key for render queue (shader hash + material + depth)
    virtual uint64_t getSortKey(const RenderContext& context) const = 0;
    
    virtual bool isInstanced() const { return false; }
    
    /**
     * @brief Should this object cast shadows?
     */
    virtual bool castsShadows() const { return true; }
};

} // namespace gfx
