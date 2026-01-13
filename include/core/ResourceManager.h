#pragma once

#include "Types.h"
#include <string>
#include <unordered_map>
#include <glad/gl.h>

namespace gfx {

/**
 * @brief RAII wrapper for OpenGL buffer objects
 */
struct GpuBuffer {
    GLuint id = 0;
    size_t size = 0;
    GLenum usage = GL_STATIC_DRAW;
    
    GpuBuffer(size_t bufferSize, GLenum bufferUsage);
    ~GpuBuffer();
    
    // No copy
    GpuBuffer(const GpuBuffer&) = delete;
    GpuBuffer& operator=(const GpuBuffer&) = delete;
    
    void bind(GLenum target) const;
    void upload(const void* data, size_t dataSize, size_t offset = 0);
};

/**
 * @brief RAII wrapper for OpenGL textures
 */
struct Texture {
    GLuint id = 0;
    int width = 0;
    int height = 0;
    GLenum format = GL_RGBA8;
    GLenum target = GL_TEXTURE_2D;
    
    Texture(int w, int h, GLenum fmt, GLenum tgt = GL_TEXTURE_2D);
    ~Texture();
    
    // No copy
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    
    void bind(int slot = 0) const;
    void unbind() const;
};

/**
 * @brief Centralized manager for GPU resources with handle-based access
 * 
 * Provides named lookup for geometry, buffers, and textures.
 * Uses shared_ptr for automatic reference counting and cleanup.
 */
class ResourceManager {
public:
    static ResourceManager* instance();
    
    // Geometry management (delegates to GeometryFactory for now)
    GeometryHandle getGeometry(const std::string& name);
    
    // Buffer management
    BufferHandle createBuffer(const std::string& name, size_t size, GLenum usage);
    BufferHandle getBuffer(const std::string& name);
    void removeBuffer(const std::string& name);
    
    // Texture management
    TextureHandle createTexture(const std::string& name, int width, int height, GLenum format);
    TextureHandle getTexture(const std::string& name);
    void removeTexture(const std::string& name);
    
    // Cleanup
    void clearAll();
    
private:
    ResourceManager() = default;
    static ResourceManager* s_instance;
    
    std::unordered_map<std::string, BufferHandle> m_buffers;
    std::unordered_map<std::string, TextureHandle> m_textures;
};

} // namespace gfx
