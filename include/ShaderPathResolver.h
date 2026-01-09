#pragma once
/// @file ShaderPathResolver.h
/// @brief Shared shader path resolution with caching

#include <string>
#include <vector>

namespace ShaderPath {

/// Set a preferred shader root directory (searched first). Empty to clear.
void setRoot(const std::string& rootDir);

/// Resolve a shader filename to a full path. Caches the successful directory on first resolution for fast subsequent lookups.
std::string resolve(const std::string& filename);

/// Load shader source from file using cached path resolution
std::string loadSource(const std::string& filename);

/// Clear the cached shader directory (call if shaders are moved at runtime)
void clearCache();

/// Get the executable directory
std::string getExecutableDir();

} // namespace ShaderPath
