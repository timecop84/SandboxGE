#pragma once

#include <glm/glm.hpp>
#include <utils/ShaderLib.h>

namespace Lighting {

struct LightParams {
    glm::vec3 position{0.0f, 50.0f, 0.0f};
    glm::vec3 ambient{0.4f, 0.4f, 0.4f};
    glm::vec3 diffuse{1.0f, 1.0f, 1.0f};
    glm::vec3 specular{1.0f, 1.0f, 1.0f};
    float constantAttenuation = 1.0f;
    float linearAttenuation = 0.0f;
    float quadraticAttenuation = 0.0f;
};

struct ShadowParams {
    bool enabled = true;
    float bias = 0.005f;
    float softness = 1.0f;
    float strength = 1.0f;
    int mapSize = 4096;
};

void setLightUniforms(ShaderLib::ProgramWrapper* prog, const LightParams& light, const glm::mat4& viewMatrix);
void setShadowUniforms(ShaderLib::ProgramWrapper* prog, const ShadowParams& shadow);

LightParams fromUIParams(const float* position, const float* ambient, 
                         const float* diffuse, const float* specular);

} // namespace Lighting
