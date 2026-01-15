#pragma once

#include "Types.h"
#include "rhi/Device.h"
#include "rhi/Types.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace sandbox {

class GpuBuffer {
public:
    virtual ~GpuBuffer() = default;

    GpuBuffer(const GpuBuffer&) = delete;
    GpuBuffer& operator=(const GpuBuffer&) = delete;

    virtual void bind(rhi::BufferBindTarget target) const = 0;
    virtual void upload(const void* data, size_t dataSize, size_t offset = 0) = 0;

protected:
    GpuBuffer() = default;
};

class Texture {
public:
    virtual ~Texture() = default;

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    virtual void bind(int slot = 0) const = 0;
    virtual void unbind() const = 0;

protected:
    Texture() = default;
};

// GPU resource manager with handle-based access
class ResourceManager {
public:
    static ResourceManager* instance();

    void setDevice(std::unique_ptr<rhi::Device> device);
    rhi::Device* getDevice() const { return m_device.get(); }
    
    // Geometry management (delegates to GeometryFactory for now)
    GeometryHandle getGeometry(const std::string& name);
    
    // Buffer management
    BufferHandle createBuffer(const std::string& name, size_t size, rhi::BufferUsage usage);
    BufferHandle getBuffer(const std::string& name);
    void removeBuffer(const std::string& name);
    
    // Texture management
    TextureHandle createTexture(const std::string& name, int width, int height, rhi::TextureFormat format,
                                rhi::TextureTarget target = rhi::TextureTarget::Texture2D);
    TextureHandle getTexture(const std::string& name);
    void removeTexture(const std::string& name);
    
    // Cleanup
    void clearAll();
    
private:
    ResourceManager();
    static ResourceManager* s_instance;
    
    std::unordered_map<std::string, BufferHandle> m_buffers;
    std::unordered_map<std::string, TextureHandle> m_textures;
    std::unique_ptr<rhi::Device> m_device;
};

} // namespace sandbox
