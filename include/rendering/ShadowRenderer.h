#pragma once

#include <glm/glm.hpp>

class Camera;
class Light;

namespace Shadow {

constexpr int MAX_SHADOW_LIGHTS = 4;

bool init(int shadowMapSize = 4096);
void cleanup();

void beginShadowPass(int lightIndex, Light* light, const glm::vec3& sceneCenter, float sceneRadius);
void endShadowPass();

glm::mat4 getLightSpaceMatrix(int lightIndex);
unsigned int getShadowMapTexture(int lightIndex);
unsigned int getShadowProgram();
void setModelMatrix(const glm::mat4& model);

void setSoftness(float softness);     // PCF filter radius
void setBias(float bias);             // Depth bias
void setEnabled(bool enabled);

bool isEnabled();
float getSoftness();
float getBias();
int getMapSize();

} // namespace Shadow
