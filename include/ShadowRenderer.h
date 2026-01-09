#pragma once
/// @file ShadowRenderer.h
/// @brief Shadow mapping renderer for directional/point light shadows with multi-light support

#include <glm/glm.hpp>

class Camera;
class Light;

namespace Shadow {

/// Maximum number of shadow-casting lights
constexpr int MAX_SHADOW_LIGHTS = 4;

/// Initialize shadow system
bool init(int shadowMapSize = 4096);

/// Cleanup shadow resources
void cleanup();

/// Begin shadow pass for a specific light index
void beginShadowPass(int lightIndex, Light* light, const glm::vec3& sceneCenter, float sceneRadius);

/// End shadow pass
void endShadowPass();

/// Get the light space matrix for shadow sampling (for specific light)
glm::mat4 getLightSpaceMatrix(int lightIndex);

/// Get shadow map texture ID (for specific light)
unsigned int getShadowMapTexture(int lightIndex);

/// Get shadow shader program (for setting model matrix)
unsigned int getShadowProgram();

/// Set model matrix for current object being rendered
void setModelMatrix(const glm::mat4& model);

/// Set shadow parameters
void setSoftness(float softness);     // PCF filter radius
void setBias(float bias);             // Depth bias
void setEnabled(bool enabled);

/// Get current state
bool isEnabled();
float getSoftness();
float getBias();
int getMapSize();

} // namespace Shadow
