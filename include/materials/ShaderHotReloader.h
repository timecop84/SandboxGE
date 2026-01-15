#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <functional>

namespace sandbox {

// watches shader files, triggers recompile on changes
class ShaderHotReloader {
public:
    static ShaderHotReloader* instance();
    
    void registerShader(const std::string& shaderName, const std::vector<std::string>& filePaths);
    void unregisterShader(const std::string& shaderName);
    
    void checkForChanges(); // call each frame to detect file modifications
    
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

    using ErrorCallback = std::function<void(const std::string& shaderName, const std::string& error)>;
    void setErrorCallback(ErrorCallback callback) { m_errorCallback = callback; }

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

} // namespace sandbox
