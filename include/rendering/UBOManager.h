#pragma once

#include "core/Types.h"
#include <cstdint>
#include <string>
#include <unordered_map>

namespace sandbox {

// Manages Uniform Buffer Objects at fixed binding points
class UBOManager {
public:
    static UBOManager* instance();
    
    std::uint32_t createUBO(const std::string& name, size_t size, uint32_t bindingPoint);
    void updateUBO(const std::string& name, const void* data, size_t size, size_t offset = 0);
    std::uint32_t getUBO(const std::string& name) const;
    void deleteUBO(const std::string& name);
    void bindUBO(const std::string& name);
    void bindUBOToPoint(const std::string& name, uint32_t bindingPoint);
    void cleanup();
    
private:
    UBOManager() = default;
    ~UBOManager();
    
    static UBOManager* s_instance;
    
    struct UBOData {
        std::uint32_t id = 0;
        size_t size = 0;
        uint32_t bindingPoint = 0;
    };
    
    std::unordered_map<std::string, UBOData> m_ubos;
};

} // namespace sandbox
