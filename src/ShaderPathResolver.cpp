#include "utils/ShaderPathResolver.h"
#include <fstream>
#include <sstream>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ShaderPath {

namespace {
    // Cached shader directory (set on first successful load)
    std::string s_cachedShaderDir;
    bool s_cacheInitialized = false;
    std::string s_rootDir;
    
    std::string tryLoadFromPath(const std::string& path) {
        std::ifstream file(path);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string content = buffer.str();
            if (!content.empty()) {
                return content;
            }
        }
        return "";
    }
}

std::string getExecutableDir() {
#ifdef _WIN32
    char path[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, path, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        std::string exePath(path);
        size_t lastSlash = exePath.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            return exePath.substr(0, lastSlash + 1);
        }
    }
#endif
    return "";
}

void setRoot(const std::string& rootDir) {
    s_rootDir = rootDir;
    // Do not clear cache; root simply takes precedence
}

std::string resolve(const std::string& filename) {
    // If we have a cached directory, try it first
    if (s_cacheInitialized && !s_cachedShaderDir.empty()) {
        std::string cachedPath = s_cachedShaderDir + filename;
        std::ifstream file(cachedPath);
        if (file.is_open()) {
            return cachedPath;
        }
    }
    
    std::string exeDir = getExecutableDir();
    
    // Search paths in priority order
    std::vector<std::string> searchDirs = {
        s_rootDir.empty() ? "" : s_rootDir,
        "",                                      // Current directory
        "shaders/",
        "../shaders/",
        "../../shaders/",
        "SandboxGE/shaders/",
        "../SandboxGE/shaders/",
        "modules/graphics_engine/shaders/",      // legacy layout support
        "../modules/graphics_engine/shaders/",
        exeDir + "shaders/",
        exeDir + "SandboxGE/shaders/",
        exeDir + "modules/graphics_engine/shaders/"
    };
    
    for (const auto& dir : searchDirs) {
        std::string fullPath = dir + filename;
        std::ifstream file(fullPath);
        if (file.is_open()) {
            // Cache this directory for future lookups
            if (!s_cacheInitialized) {
                s_cachedShaderDir = dir;
                s_cacheInitialized = true;
            }
            return fullPath;
        }
    }
    
    return "";  // Not found
}

std::string loadSource(const std::string& filename) {
    // Try cached path first
    if (s_cacheInitialized && !s_cachedShaderDir.empty()) {
        std::string content = tryLoadFromPath(s_cachedShaderDir + filename);
        if (!content.empty()) {
            return content;
        }
    }
    
    std::string exeDir = getExecutableDir();
    
    // Search paths
    std::vector<std::string> searchDirs = {
        s_rootDir.empty() ? "" : s_rootDir,
        "",
        "shaders/",
        "../shaders/",
        "../../shaders/",
        "modules/graphics_engine/shaders/",
        "../modules/graphics_engine/shaders/",
        exeDir + "shaders/",
        exeDir + "modules/graphics_engine/shaders/"
    };
    
    for (const auto& dir : searchDirs) {
        std::string fullPath = dir + filename;
        std::string content = tryLoadFromPath(fullPath);
        if (!content.empty()) {
            // Cache this directory
            if (!s_cacheInitialized) {
                s_cachedShaderDir = dir;
                s_cacheInitialized = true;
            }
            return content;
        }
    }
    
    std::cerr << "ShaderPath: Failed to load shader: " << filename << std::endl;
    return "";
}

void clearCache() {
    s_cachedShaderDir.clear();
    s_cacheInitialized = false;
}

} // namespace ShaderPath
