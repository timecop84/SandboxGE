#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>

namespace gfx {

/**
 * @brief Watches shader files and triggers recompilation on changes
 * 
 * Monitors shader source files and automatically reloads them when modified.
 * Provides visual feedback via callbacks for compilation errors.
 */
class ShaderHotReloader {
public:
    static ShaderHotReloader* instance();
    
    /**
     * @brief Register a shader for hot-reloading
     * @param shaderName Shader program name in ShaderLib
     * @param filePaths List of shader files (vertex, fragment, etc.)
     */
    void registerShader(const std::string& shaderName, const std::vector<std::string>& filePaths);
    
    /**
     * @brief Unregister a shader
     */
    void unregisterShader(const std::string& shaderName);
    
    /**
     * @brief Check all registered shaders for changes
     * Call this each frame to detect modifications
     */
    void checkForChanges();
    
    /**
     * @brief Enable/disable hot-reloading
     */
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }
    
    /**
     * @brief Set callback for compilation errors
     */
    using ErrorCallback = std::function<void(const std::string& shaderName, const std::string& error)>;
    void setErrorCallback(ErrorCallback callback) { m_errorCallback = callback; }
    
    /**
     * @brief Get last error for a shader
     */
    std::string getLastError(const std::string& shaderName) const;
    
private:
    ShaderHotReloader() = default;
    static ShaderHotReloader* s_instance;
    
    struct ShaderFileInfo {
        std::vector<std::string> filePaths;
        std::vector<std::filesystem::file_time_type> lastModified;
        std::string lastError;
    };
    
    std::unordered_map<std::string, ShaderFileInfo> m_shaders;
    ErrorCallback m_errorCallback;
    bool m_enabled = true;
    
    // Attempt to reload a shader
    bool reloadShader(const std::string& shaderName, ShaderFileInfo& info);
    
    // Update modification times
    void updateModificationTimes(ShaderFileInfo& info);
};

} // namespace gfx
