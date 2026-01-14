#include "materials/ShaderHotReloader.h"
#include <utils/ShaderLib.h>
#include <iostream>

namespace gfx {

ShaderHotReloader* ShaderHotReloader::s_instance = nullptr;

ShaderHotReloader* ShaderHotReloader::instance() {
    if (!s_instance) {
        s_instance = new ShaderHotReloader();
    }
    return s_instance;
}

void ShaderHotReloader::registerShader(const std::string& shaderName, const std::vector<std::string>& filePaths) {
    ShaderFileInfo info;
    info.filePaths = filePaths;
    info.lastModified.resize(filePaths.size());
    
    // Get initial modification times
    for (size_t i = 0; i < filePaths.size(); ++i) {
        try {
            info.lastModified[i] = std::filesystem::last_write_time(filePaths[i]);
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "ShaderHotReloader: Failed to get modification time for " 
                      << filePaths[i] << ": " << e.what() << std::endl;
        }
    }
    
    m_shaders[shaderName] = info;
    std::cout << "ShaderHotReloader: Registered shader '" << shaderName << "' with " 
              << filePaths.size() << " files" << std::endl;
}

void ShaderHotReloader::unregisterShader(const std::string& shaderName) {
    m_shaders.erase(shaderName);
}

void ShaderHotReloader::checkForChanges() {
    if (!m_enabled) {
        return;
    }
    
    for (auto& pair : m_shaders) {
        const std::string& shaderName = pair.first;
        ShaderFileInfo& info = pair.second;
        
        // Check if any files have been modified
        bool modified = false;
        for (size_t i = 0; i < info.filePaths.size(); ++i) {
            try {
                auto currentTime = std::filesystem::last_write_time(info.filePaths[i]);
                if (currentTime != info.lastModified[i]) {
                    modified = true;
                    break;
                }
            } catch (const std::filesystem::filesystem_error& e) {
                // File might have been deleted or moved
                std::cerr << "ShaderHotReloader: Error checking " << info.filePaths[i] 
                          << ": " << e.what() << std::endl;
            }
        }
        
        if (modified) {
            std::cout << "ShaderHotReloader: Detected change in shader '" << shaderName << "', reloading..." << std::endl;
            
            if (reloadShader(shaderName, info)) {
                std::cout << "ShaderHotReloader: Successfully reloaded '" << shaderName << "'" << std::endl;
                info.lastError.clear();
            } else {
                std::cerr << "ShaderHotReloader: Failed to reload '" << shaderName << "'" << std::endl;
                
                // Trigger error callback if set
                if (m_errorCallback) {
                    m_errorCallback(shaderName, info.lastError);
                }
            }
            
            // Update modification times regardless of success
            // (prevents constant retry on compile errors)
            updateModificationTimes(info);
        }
    }
}

bool ShaderHotReloader::reloadShader(const std::string& shaderName, ShaderFileInfo& info) {
    ShaderLib* shaderLib = ShaderLib::instance();
    
    // For now, we don't have a built-in "reload" function in ShaderLib
    // We would need to:
    // 1. Delete the old program
    // 2. Recompile and relink
    // 3. Keep old program active if compilation fails
    
    // This is a placeholder - full implementation would require
    // extending ShaderLib with reload capability
    
    std::cout << "ShaderHotReloader: Shader reload not yet fully implemented" << std::endl;
    info.lastError = "Reload functionality pending ShaderLib extension";
    
    // TODO: Implement actual shader recompilation
    // - Load source files
    // - Compile shaders
    // - Link program
    // - Replace old program only if successful
    // - Capture and store error messages
    
    return false; // Placeholder
}

void ShaderHotReloader::updateModificationTimes(ShaderFileInfo& info) {
    for (size_t i = 0; i < info.filePaths.size(); ++i) {
        try {
            info.lastModified[i] = std::filesystem::last_write_time(info.filePaths[i]);
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "ShaderHotReloader: Error updating time for " << info.filePaths[i] 
                      << ": " << e.what() << std::endl;
        }
    }
}

std::string ShaderHotReloader::getLastError(const std::string& shaderName) const {
    auto it = m_shaders.find(shaderName);
    return (it != m_shaders.end()) ? it->second.lastError : "";
}

} // namespace gfx
