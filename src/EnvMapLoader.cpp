#include "EnvMapLoader.h"

#include <tinyexr.h>

#include <iostream>
#include <vector>

namespace gfx {

GLuint loadEnvironmentEXR(const std::string& path, int* width, int* height) {
    float* rgba = nullptr;
    int w = 0;
    int h = 0;
    const char* err = nullptr;

    int ret = LoadEXR(&rgba, &w, &h, path.c_str(), &err);
    if (ret != TINYEXR_SUCCESS) {
        if (err) {
            std::cerr << "Failed to load EXR: " << path << " (" << err << ")\n";
            FreeEXRErrorMessage(err);
        } else {
            std::cerr << "Failed to load EXR: " << path << "\n";
        }
        return 0;
    }

    std::vector<float> rgb(static_cast<size_t>(w) * static_cast<size_t>(h) * 3);
    for (int i = 0; i < w * h; ++i) {
        rgb[i * 3 + 0] = rgba[i * 4 + 0];
        rgb[i * 3 + 1] = rgba[i * 4 + 1];
        rgb[i * 3 + 2] = rgba[i * 4 + 2];
    }

    free(rgba);

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, rgb.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (width) *width = w;
    if (height) *height = h;
    return tex;
}

} // namespace gfx
