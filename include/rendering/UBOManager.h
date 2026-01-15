#pragma once

#include "core/Types.h"
#include <glad/gl.h>
#include <string>
#include <unordered_map>

namespace sandbox {

// Manages Uniform Buffer Objects at fixed binding points
class UBOManager {
public:
    static UBOManager* instance();
    
    GLuint createUBO(const std::string& name, size_t size, uint32_t bindingPoint);
    void updateUBO(const std::string& name, const void* data, size_t size, size_t offset = 0);
    GLuint getUBO(const std::string& name) const;
    void deleteUBO(const std::string& name);
    void bindUBO(const std::string& name);
    void bindUBOToPoint(const std::string& name, uint32_t bindingPoint);
    void cleanup();
    
private:
    UBOManager() = default;
    ~UBOManager();
    
    static UBOManager* s_instance;
    
    struct UBOData {
        GLuint id = 0;
        size_t size = 0;
        uint32_t bindingPoint = 0;
    };
    
    std::unordered_map<std::string, UBOData> m_ubos;
};

} // namespace sandbox
