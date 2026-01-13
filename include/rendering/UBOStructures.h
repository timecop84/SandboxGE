#pragma once

#include <glm/glm.hpp>

namespace gfx {

/**
 * @brief UBO for matrix transforms (binding point 0)
 * 
 * std140 layout: matrices are naturally aligned (16 bytes * 4 = 64 bytes each)
 * Total size: 320 bytes
 */
struct MatrixUBO {
    alignas(16) glm::mat4 model;         // 64 bytes
    alignas(16) glm::mat4 view;          // 64 bytes
    alignas(16) glm::mat4 projection;    // 64 bytes
    alignas(16) glm::mat4 MVP;           // 64 bytes
    alignas(16) glm::mat4 normalMatrix;  // 64 bytes (mat3 stored as mat4 for alignment)
};

/**
 * @brief UBO for material properties (binding point 1)
 * 
 * std140 layout: vec4 are 16-byte aligned, scalars follow vec4 packing rules
 * Total size: 80 bytes
 */
struct MaterialUBO {
    alignas(16) glm::vec4 ambient;   // 16 bytes (rgb + padding)
    alignas(16) glm::vec4 diffuse;   // 16 bytes (rgb + padding)
    alignas(16) glm::vec4 specular;  // 16 bytes (rgb + padding)
    alignas(16) float shininess;     // 4 bytes
    alignas(16) float metallic;      // 4 bytes (for PBR)
    alignas(16) float roughness;     // 4 bytes (for PBR)
    alignas(16) int useTexture;      // 4 bytes (bool as int)
    // Total: 64 bytes (4 vec4s + 4 aligned floats/ints)
};

/**
 * @brief UBO for lighting data (binding point 2)
 * 
 * std140 layout: arrays have 16-byte stride for vec4
 * Total size: 144 bytes
 */
struct LightingUBO {
    alignas(16) glm::vec4 positions[4];    // 64 bytes (4 * 16)
    alignas(16) glm::vec4 colors[4];       // 64 bytes (4 * 16)
    alignas(16) int count;                 // 4 bytes
    alignas(16) float ambientStrength;     // 4 bytes
    // Total: 136 bytes
};

/**
 * @brief UBO for instanced rendering (binding point 3)
 * 
 * std140 layout: array of mat4 with 16-byte alignment
 * Total size: 16384 bytes (256 instances * 64 bytes)
 */
struct InstancesUBO {
    alignas(16) glm::mat4 instanceMatrices[256];  // 16384 bytes
};

} // namespace gfx
