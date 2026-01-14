#pragma once

#include <string>
#include <vector>

namespace ShaderPath {

void setRoot(const std::string& rootDir);
std::string resolve(const std::string& filename);
std::string loadSource(const std::string& filename);
void clearCache();
std::string getExecutableDir();

} // namespace ShaderPath
