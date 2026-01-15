#include "rendering/UBOManager.h"
#include <iostream>

namespace sandbox {

UBOManager* UBOManager::s_instance = nullptr;

UBOManager* UBOManager::instance() {
    if (!s_instance) {
        s_instance = new UBOManager();
    }
    return s_instance;
}

UBOManager::~UBOManager() {
    cleanup();
}

GLuint UBOManager::createUBO(const std::string& name, size_t size, uint32_t bindingPoint) {
    // Check if already exists
    auto it = m_ubos.find(name);
    if (it != m_ubos.end()) {
        std::cerr << "UBOManager: UBO '" << name << "' already exists!" << std::endl;
        return it->second.id;
    }
    
    // Create the UBO
    GLuint uboID = 0;
    glGenBuffers(1, &uboID);
    glBindBuffer(GL_UNIFORM_BUFFER, uboID);
    glBufferData(GL_UNIFORM_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    
    // Bind to the designated binding point
    glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, uboID);
    
    // Store metadata
    UBOData data;
    data.id = uboID;
    data.size = size;
    data.bindingPoint = bindingPoint;
    m_ubos[name] = data;
    
    std::cout << "UBOManager: Created UBO '" << name << "' (ID: " << uboID 
              << ", Size: " << size << " bytes, Binding: " << bindingPoint << ")" << std::endl;
    
    return uboID;
}

void UBOManager::updateUBO(const std::string& name, const void* data, size_t size, size_t offset) {
    auto it = m_ubos.find(name);
    if (it == m_ubos.end()) {
        std::cerr << "UBOManager: UBO '" << name << "' not found!" << std::endl;
        return;
    }
    
    const UBOData& ubo = it->second;
    
    if (offset + size > ubo.size) {
        std::cerr << "UBOManager: Data exceeds UBO '" << name << "' size!" << std::endl;
        return;
    }
    
    glBindBuffer(GL_UNIFORM_BUFFER, ubo.id);
    glBufferSubData(GL_UNIFORM_BUFFER, offset, size, data);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

GLuint UBOManager::getUBO(const std::string& name) const {
    auto it = m_ubos.find(name);
    return (it != m_ubos.end()) ? it->second.id : 0;
}

void UBOManager::deleteUBO(const std::string& name) {
    auto it = m_ubos.find(name);
    if (it != m_ubos.end()) {
        glDeleteBuffers(1, &it->second.id);
        m_ubos.erase(it);
    }
}

void UBOManager::bindUBOToPoint(const std::string& name, uint32_t bindingPoint) {
    auto it = m_ubos.find(name);
    if (it == m_ubos.end()) {
        std::cerr << "UBOManager: UBO '" << name << "' not found!" << std::endl;
        return;
    }
    
    glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, it->second.id);
    it->second.bindingPoint = bindingPoint;
}

void UBOManager::cleanup() {
    for (auto& pair : m_ubos) {
        glDeleteBuffers(1, &pair.second.id);
    }
    m_ubos.clear();
}

} // namespace sandbox
