#ifndef RENDERER_H
#define RENDERER_H

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <string>

class Floor;
class SphereObstacle;
class Camera;
class TransformStack;

namespace FlockingGraphics {
    struct Geometry;
}
namespace gfx { struct RenderSettings; }

// --- Scene rendering ---
namespace Renderer {

// Light position constant (used by all shaders)
constexpr glm::vec3 LIGHT_WORLD_POS{25.0f, 80.0f, 40.0f};

// OpenGL resources for cloth rendering
struct ClothRenderData {
    GLuint VAO = 0;
    GLuint VBO = 0;
    std::vector<GLfloat> vertexData;
    std::shared_ptr<FlockingGraphics::Geometry> particleSphere;
};

// Initialize OpenGL state
void initGL();

// Setup shader uniforms for lighting (and shadows)
void setupLighting(Camera* camera, const gfx::RenderSettings& settings);

// Load model/view/projection matrices to shader
void loadMatricesToShader(const TransformStack& stack, Camera* camera);
void loadMatricesToShader(const std::string& shaderName, const TransformStack& stack, Camera* camera);

// Render only floor and sphere (cloth is rendered by main.cpp using cloth:: API)
void renderFloorAndSphere(Floor* floor, SphereObstacle* sphere,
                          Camera* camera, TransformStack& transformStack,
                          const gfx::RenderSettings& settings);

// Cleanup OpenGL resources
void cleanup(ClothRenderData& renderData);

} // namespace Renderer

#endif // RENDERER_H
