#pragma once

#include <cstdint>
#include <memory>

// Forward declarations
namespace FlockingGraphics {
    struct Geometry;
}

namespace sandbox {

// Forward declarations
class Material;
class IRenderable;
class GpuBuffer;
class Texture;

// Type aliases for clarity and type safety
using MaterialID = uint32_t;
using ShaderHash = uint32_t;
using GeometryHandle = std::shared_ptr<FlockingGraphics::Geometry>;
using BufferHandle = std::shared_ptr<GpuBuffer>;
using TextureHandle = std::shared_ptr<Texture>;

// UBO binding points (must match shader layout bindings)
namespace UBOBindingPoints {
    constexpr uint32_t MATRICES = 0;
    constexpr uint32_t MATERIAL = 1;
    constexpr uint32_t LIGHTING = 2;
    constexpr uint32_t INSTANCES = 3;
}

// Instancing constants
namespace InstanceLimits {
    constexpr uint32_t MAX_INSTANCES_PER_BATCH = 256;
}

} // namespace sandbox
