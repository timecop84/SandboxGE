#pragma once
/// @file SSAORenderer.h
/// @brief Screen-Space Ambient Occlusion post-process renderer

class Camera;

namespace SSAO {

/// Initialize SSAO system - call after OpenGL context and shaders loaded
bool init(int width, int height);

/// Cleanup SSAO resources
void cleanup();

/// Resize framebuffers when window size changes
void resize(int width, int height);

/// Begin scene rendering (binds scene FBO)
void beginScenePass();

/// End scene rendering and run SSAO
void endScenePass();

/// Render final composite to screen
void renderComposite(Camera* camera);

/// Set SSAO parameters
void setRadius(float radius);
void setBias(float bias);
void setIntensity(float intensity);
void setEnabled(bool enabled);

/// Get current state
bool isEnabled();
float getRadius();
float getBias();
float getIntensity();

} // namespace SSAO
