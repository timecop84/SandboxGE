#pragma once

#include <glm/glm.hpp>
#include <cstdint>

namespace sandbox {
class Camera;
class Light;
namespace rhi {
class Device;
class Texture;
}

namespace Shadow {

constexpr int MAX_SHADOW_LIGHTS = 4;

bool init(int shadowMapSize = 4096);
void cleanup();
void setDevice(rhi::Device* device);

void beginShadowPass(int lightIndex, Light* light, const glm::vec3& sceneCenter, float sceneRadius);
/// Begin shadow pass for a cascade slice derived from camera frustum
void beginShadowCascade(int cascadeIndex, Light* light, Camera* camera, float splitNear, float splitFar);

void endShadowPass();

glm::mat4 getLightSpaceMatrix(int lightIndex);
const rhi::Texture* getShadowMapTexture(int lightIndex);
void bindShadowMap(int lightIndex, int slot);
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
} // namespace sandbox
