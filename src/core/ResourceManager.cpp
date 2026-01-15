#include "core/ResourceManager.h"
#include "rhi/DeviceGL.h"
#include <utils/GeometryFactory.h>
#include <iostream>

namespace {

class DeviceBuffer final : public sandbox::GpuBuffer {
public:
    explicit DeviceBuffer(std::unique_ptr<sandbox::rhi::Buffer> buffer, size_t size)
        : m_buffer(std::move(buffer)), m_size(size) {}

    void bind(sandbox::rhi::BufferBindTarget target) const override {
        if (m_buffer) {
            m_buffer->bind(target);
        }
    }

    void upload(const void* data, size_t dataSize, size_t offset) override {
        if (offset + dataSize > m_size) {
            std::cerr << "GpuBuffer::upload - data exceeds buffer size!" << std::endl;
            return;
        }
        if (m_buffer) {
            m_buffer->update(data, dataSize, offset);
        }
    }

private:
    std::unique_ptr<sandbox::rhi::Buffer> m_buffer;
    size_t m_size = 0;
};

class DeviceTexture final : public sandbox::Texture {
public:
    explicit DeviceTexture(std::unique_ptr<sandbox::rhi::Texture> texture)
        : m_texture(std::move(texture)) {}

    void bind(int slot) const override {
        if (m_texture) {
            m_texture->bind(static_cast<std::uint32_t>(slot));
        }
    }

    void unbind() const override {
        if (m_texture) {
            m_texture->unbind();
        }
    }

private:
    std::unique_ptr<sandbox::rhi::Texture> m_texture;
};

} // namespace

namespace sandbox {

ResourceManager* ResourceManager::s_instance = nullptr;

ResourceManager* ResourceManager::instance() {
    if (!s_instance) {
        s_instance = new ResourceManager();
    }
    return s_instance;
}

ResourceManager::ResourceManager() {
    m_device = rhi::createOpenGLDevice();
}

void ResourceManager::setDevice(std::unique_ptr<rhi::Device> device) {
    if (device) {
        m_device = std::move(device);
    }
}

GeometryHandle ResourceManager::getGeometry(const std::string& name) {
    // Delegate to GeometryFactory for now (already has good caching)
    return FlockingGraphics::GeometryFactory::instance().getGeometry(name);
}

BufferHandle ResourceManager::createBuffer(const std::string& name, size_t size, rhi::BufferUsage usage) {
    auto it = m_buffers.find(name);
    if (it != m_buffers.end()) {
        return it->second; // Already exists
    }
    
    if (!m_device) {
        m_device = rhi::createOpenGLDevice();
    }

    auto buffer = std::make_shared<DeviceBuffer>(m_device->createBuffer(size, usage), size);
    m_buffers[name] = buffer;
    return buffer;
}

BufferHandle ResourceManager::getBuffer(const std::string& name) {
    auto it = m_buffers.find(name);
    return (it != m_buffers.end()) ? it->second : nullptr;
}

void ResourceManager::removeBuffer(const std::string& name) {
    m_buffers.erase(name);
}

TextureHandle ResourceManager::createTexture(const std::string& name, int width, int height,
                                             rhi::TextureFormat format, rhi::TextureTarget target) {
    auto it = m_textures.find(name);
    if (it != m_textures.end()) {
        return it->second; // Already exists
    }
    
    if (!m_device) {
        m_device = rhi::createOpenGLDevice();
    }

    auto texture = std::make_shared<DeviceTexture>(m_device->createTexture(width, height, format, target));
    m_textures[name] = texture;
    return texture;
}

TextureHandle ResourceManager::getTexture(const std::string& name) {
    auto it = m_textures.find(name);
    return (it != m_textures.end()) ? it->second : nullptr;
}

void ResourceManager::removeTexture(const std::string& name) {
    m_textures.erase(name);
}

void ResourceManager::clearAll() {
    m_buffers.clear();
    m_textures.clear();
}

} // namespace sandbox
