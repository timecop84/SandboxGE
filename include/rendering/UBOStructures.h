#pragma once

#include <glm/glm.hpp>

namespace gfx {

// Matrix transforms (binding point 0) - std140 layout, 320 bytes total
struct MatrixUBO {
    alignas(16) glm::mat4 model;         // 64 bytes
    alignas(16) glm::mat4 view;          // 64 bytes
    alignas(16) glm::mat4 projection;    // 64 bytes
    alignas(16) glm::mat4 MVP;           // 64 bytes
    alignas(16) glm::mat4 normalMatrix;  // 64 bytes (mat3 stored as mat4 for alignment)
};

// Material properties (binding point 1) - std140 layout, 80 bytes total
struct MaterialUBO {
    alignas(16) glm::vec4 ambient;   // 16 bytes (rgb + padding)
    alignas(16) glm::vec4 diffuse;   // 16 bytes (rgb + padding)
    alignas(16) glm::vec4 specular;  // 16 bytes (rgb + padding)
    alignas(16) float shininess;     // 4 bytes
    alignas(16) float metallic;      // 4 bytes (for PBR)
    alignas(16) float roughness;     // 4 bytes (for PBR)
    alignas(16) int useTexture;      // 4 bytes (bool as int)
};

// Lighting data (binding point 2) - std140 layout, 144 bytes total
struct LightingUBO {
    alignas(16) glm::vec4 positions[4];    // 64 bytes (4 * 16)
    alignas(16) glm::vec4 colors[4];       // 64 bytes (4 * 16)
    alignas(16) int count;                 // 4 bytes
    alignas(16) float ambientStrength;     // 4 bytes
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
