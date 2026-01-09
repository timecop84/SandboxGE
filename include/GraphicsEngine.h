#ifndef GRAPHICS_ENGINE_H
#define GRAPHICS_ENGINE_H

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>

#include "Renderer.h"
#include "RenderSettings.h"
#include "Floor.h"
#include "SphereObstacle.h"
#include "ClothAPI.h"

class Camera;
class TransformStack;

namespace gfx {

// GPU-side mesh buffers
struct GpuMesh {
    GLuint VAO = 0;
    GLuint VBO = 0;
    GLuint EBO = 0;
    int vertexCount = 0;
    int triangleCount = 0;
    size_t vertexCapacityBytes = 0;
    size_t indexCapacityBytes = 0;
};

struct MeshSource {
    const float* positions = nullptr;
    const float* normals = nullptr;
    const float* uvs = nullptr;
    const uint32_t* indices = nullptr;
    int vertexCount = 0;
    int indexCount = 0;
    glm::vec3 color{0.8f, 0.8f, 0.8f};
};

class Engine {
public:
    bool initialize(int width, int height);
    void resize(int width, int height);
    void setShaderRoot(const std::string& rootDir);

    // Sync GPU meshes with cloth:: data (creates/deletes as needed)
    void syncClothMeshes(cloth::Scene* scene,
                         const std::vector<cloth::ClothHandle>& cloths,
                         std::vector<glm::vec3>& clothColors);
    // Upload arbitrary meshes (positions/normals/uvs/indices)
    void syncMeshes(const std::vector<MeshSource>& meshes);

    // Render full scene (shadows + SSAO + main pass + particles)
    void renderScene(Camera* camera,
                     Floor* floor,
                     SphereObstacle* sphere,
                     Renderer::ClothRenderData& renderData,
                     cloth::Scene* clothScene,
                     const std::vector<cloth::ClothHandle>& cloths,
                     const std::vector<glm::vec3>& clothColors,
                     const gfx::RenderSettings& settings,
                     TransformStack& transformStack);

    void cleanup(Renderer::ClothRenderData& renderData);

private:
    std::vector<GpuMesh> m_clothMeshes;
    std::vector<GpuMesh> m_genericMeshes;
    std::vector<glm::vec3> m_genericColors;

    void ensureClothMeshes(size_t count, std::vector<glm::vec3>& clothColors);
    void ensureGenericMeshes(size_t count);
    void renderParticles(Camera* camera,
                         Renderer::ClothRenderData& renderData,
                         cloth::Scene* scene,
                         const std::vector<cloth::ClothHandle>& cloths,
                         const gfx::RenderSettings& settings);
};

} // namespace gfx

#endif // GRAPHICS_ENGINE_H
