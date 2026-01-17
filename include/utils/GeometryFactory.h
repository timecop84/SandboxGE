#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <glm/glm.hpp>

namespace FlockingGraphics {

struct Geometry {
    unsigned int VAO = 0;
    unsigned int VBO = 0;
    unsigned int EBO = 0;
    size_t indexCount = 0;
    size_t vertexCount = 0;
    size_t vertexCapacityBytes = 0;
    size_t indexCapacityBytes = 0;
    
    ~Geometry();
    void bind() const;
    void render() const;
    void cleanup();
};

class GeometryFactory {
public:
    static GeometryFactory& instance();
    
    // Create geometries with caching
    std::shared_ptr<Geometry> createGeometry(const std::string& name,
                                            const std::vector<float>& vertices,
                                            const std::vector<unsigned int>& indices);

    // Update or create geometry (dynamic data)
    std::shared_ptr<Geometry> updateGeometry(const std::string& name,
                                            const std::vector<float>& vertices,
                                            const std::vector<unsigned int>& indices);
    
    // Get existing geometry
    std::shared_ptr<Geometry> getGeometry(const std::string& name);
    
    // Release geometry (reference counting)
    void releaseGeometry(const std::string& name);
    
    // Common geometry creators
    std::shared_ptr<Geometry> createSphere(float radius = 1.0f, int segments = 32);
    std::shared_ptr<Geometry> createCube(float size = 1.0f);
    std::shared_ptr<Geometry> createTriangle(float size = 1.0f);
    std::shared_ptr<Geometry> createBoundingBox();
    std::shared_ptr<Geometry> createOBJ(const std::string& name, const std::string& path, float scale = 1.0f);
    
    // Management
    void clear();
    size_t getGeometryCount() const;
    void printStats() const;
    
private:
    GeometryFactory() = default;
    ~GeometryFactory() = default;
    
    std::unordered_map<std::string, std::shared_ptr<Geometry>> m_geometries;
    
    // Helper methods
    void createVAO(Geometry* geometry, const std::vector<float>& vertices, const std::vector<unsigned int>& indices);
};

} // namespace FlockingGraphics
