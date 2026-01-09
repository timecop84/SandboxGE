/// @file LightingHelper.h
/// @brief Consolidated light and shadow uniform setup to reduce code duplication

#pragma once

#include <glm/glm.hpp>
#include <ShaderLib.h>

namespace Lighting {

/// Light properties for uniform setup
struct LightParams {
    glm::vec3 position{0.0f, 50.0f, 0.0f};
    glm::vec3 ambient{0.4f, 0.4f, 0.4f};
    glm::vec3 diffuse{1.0f, 1.0f, 1.0f};
    glm::vec3 specular{1.0f, 1.0f, 1.0f};
    float constantAttenuation = 1.0f;
    float linearAttenuation = 0.0f;
    float quadraticAttenuation = 0.0f;
};

/// Shadow properties for uniform setup
struct ShadowParams {
    bool enabled = true;
    float bias = 0.005f;
    float softness = 1.0f;
    float strength = 1.0f;
    int mapSize = 4096;
};

/// Set all light uniforms on a shader program (position is transformed to view space)
void setLightUniforms(ShaderLib::ProgramWrapper* prog, const LightParams& light, const glm::mat4& viewMatrix);

/// Set all shadow uniforms on a shader program
void setShadowUniforms(ShaderLib::ProgramWrapper* prog, const ShadowParams& shadow);

/// Convenience: set both light and shadow uniforms
void setLightingUniforms(ShaderLib::ProgramWrapper* prog, 
                         const LightParams& light, 
                         const ShadowParams& shadow,
                         const glm::mat4& viewMatrix);

/// Build LightParams from UI arrays (lightPosition[3], lightAmbient[3], etc.)
LightParams fromUIParams(const float* position, const float* ambient, 
                         const float* diffuse, const float* specular);

} // namespace Lighting
