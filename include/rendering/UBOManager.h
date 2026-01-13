#pragma once

#include "core/Types.h"
#include <glad/gl.h>
#include <string>
#include <unordered_map>

namespace gfx {

/**
 * @brief Manages Uniform Buffer Objects for efficient uniform data transfer
 * 
 * Singleton that creates, updates, and binds UBOs to fixed binding points.
 * UBOs reduce per-draw overhead by batching uniform uploads.
 */
class UBOManager {
public:
    static UBOManager* instance();
    
    /**
     * @brief Create a UBO with specified size and binding point
     * @param name Unique identifier for the UBO
     * @param size Size in bytes
     * @param bindingPoint Fixed binding point (0-3 as defined in Types.h)
     * @return OpenGL buffer ID
     */
    GLuint createUBO(const std::string& name, size_t size, uint32_t bindingPoint);
    
    /**
     * @brief Update UBO data
     * @param name UBO identifier
     * @param data Pointer to data
     * @param size Size of data in bytes
     * @param offset Offset in bytes (default 0)
     */
    void updateUBO(const std::string& name, const void* data, size_t size, size_t offset = 0);
    
    /**
     * @brief Get UBO ID by name
     */
    GLuint getUBO(const std::string& name) const;
    
    /**
     * @brief Delete a UBO
     */
    void deleteUBO(const std::string& name);
    
    /**
     * @brief Bind a UBO to its designated binding point
     * This is usually done once during initialization
     */
    void bindUBOToPoint(const std::string& name, uint32_t bindingPoint);
    
    /**
     * @brief Cleanup all UBOs
     */
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

} // namespace gfx
