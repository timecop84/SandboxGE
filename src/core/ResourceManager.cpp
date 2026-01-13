#include "core/ResourceManager.h"
#include <GeometryFactory.h>
#include <iostream>

namespace gfx {

GpuBuffer::GpuBuffer(size_t bufferSize, GLenum bufferUsage) 
    : size(bufferSize), usage(bufferUsage) {
    glGenBuffers(1, &id);
    glBindBuffer(GL_ARRAY_BUFFER, id);
    glBufferData(GL_ARRAY_BUFFER, size, nullptr, usage);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

GpuBuffer::~GpuBuffer() {
    if (id != 0) {
        glDeleteBuffers(1, &id);
    }
}

void GpuBuffer::bind(GLenum target) const {
    glBindBuffer(target, id);
}

void GpuBuffer::upload(const void* data, size_t dataSize, size_t offset) {
    if (offset + dataSize > size) {
        std::cerr << "GpuBuffer::upload - data exceeds buffer size!" << std::endl;
        return;
    }
    glBindBuffer(GL_ARRAY_BUFFER, id);
    glBufferSubData(GL_ARRAY_BUFFER, offset, dataSize, data);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

Texture::Texture(int w, int h, GLenum fmt, GLenum tgt) 
    : width(w), height(h), format(fmt), target(tgt) {
    glGenTextures(1, &id);
    glBindTexture(target, id);
    
    // Setup default filtering and wrapping
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Allocate storage based on format
    if (format == GL_DEPTH_COMPONENT || format == GL_DEPTH_COMPONENT24 || format == GL_DEPTH_COMPONENT32F) {
        glTexImage2D(target, 0, format, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    } else {
        glTexImage2D(target, 0, format, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }
    
    glBindTexture(target, 0);
}

Texture::~Texture() {
    if (id != 0) {
        glDeleteTextures(1, &id);
    }
}

void Texture::bind(int slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(target, id);
}

void Texture::unbind() const {
    glBindTexture(target, 0);
}

ResourceManager* ResourceManager::s_instance = nullptr;

ResourceManager* ResourceManager::instance() {
    if (!s_instance) {
        s_instance = new ResourceManager();
    }
    return s_instance;
}

GeometryHandle ResourceManager::getGeometry(const std::string& name) {
    // Delegate to GeometryFactory for now (already has good caching)
    return FlockingGraphics::GeometryFactory::instance().getGeometry(name);
}

BufferHandle ResourceManager::createBuffer(const std::string& name, size_t size, GLenum usage) {
    auto it = m_buffers.find(name);
    if (it != m_buffers.end()) {
        return it->second; // Already exists
    }
    
    auto buffer = std::make_shared<GpuBuffer>(size, usage);
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

TextureHandle ResourceManager::createTexture(const std::string& name, int width, int height, GLenum format) {
    auto it = m_textures.find(name);
    if (it != m_textures.end()) {
        return it->second; // Already exists
    }
    
    auto texture = std::make_shared<Texture>(width, height, format);
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

} // namespace gfx
