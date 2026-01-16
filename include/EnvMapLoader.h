#pragma once

#include <string>
#include <glad/gl.h>

namespace gfx {

// Load an EXR equirectangular environment map into a 2D texture.
// Returns 0 on failure.
GLuint loadEnvironmentEXR(const std::string& path, int* width, int* height);

} // namespace gfx
