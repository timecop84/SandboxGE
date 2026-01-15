#pragma once

#include <vector>

namespace gfx {

enum class MaterialPreset : int {
    Phong = 0,
    Silk = 1,
    SilkPBR = 2,
};

struct ExtraLight {
    bool enabled = false;
    bool castsShadow = false;
    float position[3] = {0.0f, 0.0f, 0.0f};
    float diffuse[3] = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
};

struct RenderSettings {
    // Visibility
    bool clothVisibility = true;
    bool clothWireframe = false;
    bool sphereVisibility = true;
    bool floorVisibility = true;
    bool pointVisibility = false;
    bool pointWireframe = false;
    bool customMeshVisibility = true;
    bool customMeshWireframe = false;

    // Shader choice
    bool useSilkShader = true;
    bool usePBRSilk = true;

    // Per-primitive material overrides (preferred over useSilkShader/usePBRSilk when used)
    MaterialPreset floorMaterial = MaterialPreset::Phong;
    MaterialPreset sphereMaterial = MaterialPreset::Phong;
    MaterialPreset clothMaterial = MaterialPreset::SilkPBR;
    MaterialPreset customMeshMaterial = MaterialPreset::Phong;

    // Silk (classic)
    float anisotropyU = 1.2f;
    float anisotropyV = 0.6f;
    float sheenIntensity = 0.3f;
    float subsurfaceAmount = 0.3f;
    float weaveScale = 50.0f;

    // Silk PBR
    float pbrRoughness = 0.5f;
    float pbrMetallic = 0.0f;
    float pbrAnisotropy = 0.7f;
    float sheenColor[3] = {1.0f, 0.95f, 0.9f};
    float subsurfaceColor[3] = {1.0f, 0.8f, 0.7f};

    // Checker overlay
    bool useCheckerPattern = false;
    float checkerScale = 8.0f;
    float checkerColor1[3] = {1.0f, 1.0f, 1.0f};
    float checkerColor2[3] = {0.1f, 0.1f, 0.1f};

    // Ambient occlusion
    float aoStrength = 0.5f;
    float aoGroundColor[3] = {0.4f, 0.35f, 0.3f};

    // Shadows
    bool shadowEnabled = true;
    float shadowBias = 0.005f;
    float shadowSoftness = 2.0f;

    // SSAO
    bool ssaoEnabled = true;
    float ssaoRadius = 2.0f;
    float ssaoIntensity = 1.5f;
    float ssaoBias = 0.025f;

    // Primary light
    float lightPosition[3] = {25.0f, 90.0f, 45.0f};
    float lightAmbient[3] = {0.4f, 0.4f, 0.4f};
    float lightDiffuse[3] = {1.2f, 1.2f, 1.2f};
    float lightSpecular[3] = {1.1f, 1.1f, 1.1f};

    // Additional lights
    std::vector<ExtraLight> lights;
};

} // namespace gfx
