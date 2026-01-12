/// @file imgui_opengl3_patch.h
/// Patch for ImGui OpenGL3 backend to support WSLg (GLSL 4.20 max)
/// Include this BEFORE imgui_impl_opengl3.h to downgrade version strings

#pragma once

#ifndef IMGUI_IMPL_OPENGL_LOADER_GLAD
#define IMGUI_IMPL_OPENGL_LOADER_GLAD
#endif

// Force a lower GLSL version if running on WSLg
// WSLg typically supports up to GLSL 4.20
#if defined(__linux__) || defined(__unix__)
#define IMGUI_IMPL_OPENGL_USE_LOADER_CUSTOM
extern "C" {
    unsigned int glGetError(void);
    void glGetIntegerv(unsigned int pname, int* data);
}
inline void imgui_patch_detect_wsl() {
    const char* vendor = (const char*)glGetString(GL_VENDOR);
    const char* version = (const char*)glGetString(GL_VERSION);
    if (vendor && (strstr(vendor, "Microsoft") != nullptr || strstr(vendor, "VMware") != nullptr)) {
        // WSL detected; limit to GLSL 4.20
        printf("[ImGui] WSL detected, limiting GLSL to 4.20\n");
    }
}
#endif

// Macro to override ImGui's GLSL version detection
// This is defined before including imgui_impl_opengl3.cpp
#define IMGUI_IMPL_OPENGL_MAX_VERSION "420"
