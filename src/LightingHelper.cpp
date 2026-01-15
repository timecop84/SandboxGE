#include "utils/LightingHelper.h"
#include "rendering/ShadowRenderer.h"
#include <utils/ShaderLib.h>
#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>

namespace sandbox::Lighting {

void setLightUniforms(ShaderLib::ProgramWrapper* prog, const LightParams& light, const glm::mat4& viewMatrix) {
    if (!prog) return;
    
    glm::vec4 lightViewPos = viewMatrix * glm::vec4(light.position, 1.0f);
    
    prog->setUniform("light.position", lightViewPos);
    prog->setUniform("light.ambient", glm::vec4(light.ambient, 1.0f));
    prog->setUniform("light.diffuse", glm::vec4(light.diffuse, 1.0f));
    prog->setUniform("light.specular", glm::vec4(light.specular, 1.0f));
    prog->setUniform("light.constantAttenuation", light.constantAttenuation);
    prog->setUniform("light.linearAttenuation", light.linearAttenuation);
    prog->setUniform("light.quadraticAttenuation", light.quadraticAttenuation);
}

void setShadowUniforms(ShaderLib::ProgramWrapper* prog, const ShadowParams& shadow) {
    if (!prog) return;
    
    GLuint programId = prog->getProgramId();
    
    prog->setUniform("lightSpaceMatrix", Shadow::getLightSpaceMatrix(0));
    glUniform1i(glGetUniformLocation(programId, "shadowEnabled"), shadow.enabled ? 1 : 0);
    glUniform1f(glGetUniformLocation(programId, "shadowBias"), shadow.bias);
    glUniform1f(glGetUniformLocation(programId, "shadowSoftness"), shadow.softness);
    glUniform1f(glGetUniformLocation(programId, "shadowStrength"), shadow.strength);
    glUniform1f(glGetUniformLocation(programId, "shadowMapSize"), static_cast<float>(shadow.mapSize));
    
    Shadow::bindShadowMap(0, 5);
    glUniform1i(glGetUniformLocation(programId, "shadowMap"), 5);
}

LightParams fromUIParams(const float* position, const float* ambient, 
                         const float* diffuse, const float* specular) {
    LightParams params;
    if (position) params.position = glm::vec3(position[0], position[1], position[2]);
    if (ambient) params.ambient = glm::vec3(ambient[0], ambient[1], ambient[2]);
    if (diffuse) params.diffuse = glm::vec3(diffuse[0], diffuse[1], diffuse[2]);
    if (specular) params.specular = glm::vec3(specular[0], specular[1], specular[2]);
    return params;
}

} // namespace sandbox::Lighting
