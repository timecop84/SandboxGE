#pragma once

class Camera;

namespace SSAO {

bool init(int width, int height);

void cleanup();
void resize(int width, int height);

void beginScenePass();
void endScenePass();
void renderComposite(Camera* camera);

void setRadius(float radius);
void setBias(float bias);
void setIntensity(float intensity);
void setEnabled(bool enabled);

bool isEnabled();
float getRadius();
float getBias();
float getIntensity();

} // namespace SSAO
