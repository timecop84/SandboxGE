#include "rendering/UnifiedRenderer.h"
#include "rendering/MeshRenderable.h"
#include "renderables/FloorRenderable.h"
#include "renderables/SphereRenderable.h"
#include "materials/Material.h"
#include "core/RenderSettings.h"
#include "core/Camera.h"
#include "core/ResourceManager.h"
#include "utils/ShaderPathResolver.h"
#include "utils/ShaderLib.h"
#include "utils/GeometryFactory.h"
#include "io/ObjParser.h"
#include "EnvMapLoader.h"
#include "IBL.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif
#define GLFW_EXPOSE_NATIVE_WIN32
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <vector>
#include <memory>
#include <cmath>
#include <cctype>
#include <limits>
#ifdef _WIN32
#include <windows.h>
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
#endif

using namespace sandbox;
using namespace FlockingGraphics;

namespace {

std::string findShaderRoot() {
    namespace fs = std::filesystem;
    std::vector<fs::path> candidates = {
        fs::path(ShaderPath::getExecutableDir()) / "shaders",
        fs::current_path() / "shaders",
        fs::current_path().parent_path() / "shaders"};

    for (const auto& candidate : candidates) {
        if (fs::exists(candidate)) {
            std::string path = candidate.string();
            if (!path.empty()) {
                char last = path.back();
                if (last != '/' && last != '\\') {
                    path += fs::path::preferred_separator;
                }
            }
            return path;
        }
    }
    return "shaders/";
}

std::string findEnvMapPath() {
    namespace fs = std::filesystem;
    const fs::path filename = fs::path("assets") / "hdri" / "glass_passage_4k.exr";
    std::vector<fs::path> candidates;
    auto addCandidates = [&](fs::path base) {
        for (int i = 0; i < 4; ++i) {
            if (base.empty()) break;
            candidates.push_back(base / filename);
            fs::path parent = base.parent_path();
            if (parent == base) break;
            base = parent;
        }
    };

    addCandidates(fs::path(ShaderPath::getExecutableDir()));
    addCandidates(fs::current_path());

    for (const auto& candidate : candidates) {
        if (fs::exists(candidate)) {
            return candidate.string();
        }
    }

    return filename.string();
}

std::string findIconPath() {
    namespace fs = std::filesystem;
    const fs::path filename = fs::path("icons") / "sandbox_icon.jpg";
    std::vector<fs::path> candidates;
    auto addCandidates = [&](fs::path base) {
        for (int i = 0; i < 4; ++i) {
            if (base.empty()) break;
            candidates.push_back(base / filename);
            fs::path parent = base.parent_path();
            if (parent == base) break;
            base = parent;
        }
    };

    addCandidates(fs::path(ShaderPath::getExecutableDir()));
    addCandidates(fs::current_path());

    for (const auto& candidate : candidates) {
        if (fs::exists(candidate)) {
            return candidate.string();
        }
    }

    return filename.string();
}

std::string resolveTexturePath(const std::string& inputPath) {
    namespace fs = std::filesystem;
    if (inputPath.empty()) {
        return {};
    }

    fs::path path(inputPath);
    if (!fs::exists(path)) {
        return {};
    }

    if (fs::is_directory(path)) {
        std::vector<std::string> preferred = {
            "albedo", "basecolor", "base_color", "diffuse", "color"
        };
        fs::path bestMatch;
        for (const auto& entry : fs::directory_iterator(path)) {
            if (!entry.is_regular_file()) continue;
            fs::path filePath = entry.path();
            std::string ext = filePath.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
            if (ext != ".png" && ext != ".jpg" && ext != ".jpeg" && ext != ".tga" && ext != ".bmp") continue;

            std::string name = filePath.stem().string();
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
            for (const auto& key : preferred) {
                if (name.find(key) != std::string::npos) {
                    return filePath.string();
                }
            }
            if (bestMatch.empty()) {
                bestMatch = filePath;
            }
        }
        return bestMatch.empty() ? std::string{} : bestMatch.string();
    }

    return path.string();
}

TextureHandle loadTexture2D(const std::string& texturePath, const std::string& name, std::string& error) {
    if (texturePath.empty()) {
        error = "Texture path is empty.";
        return nullptr;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_set_flip_vertically_on_load(1);
    unsigned char* pixels = stbi_load(texturePath.c_str(), &width, &height, &channels, 4);
    stbi_set_flip_vertically_on_load(0);
    if (!pixels) {
        error = "Failed to load texture: " + texturePath;
        return nullptr;
    }

    ResourceManager::instance()->removeTexture(name);
    auto texture = ResourceManager::instance()->createTexture(name, width, height, rhi::TextureFormat::RGBA8);
    if (!texture) {
        stbi_image_free(pixels);
        error = "Failed to create GPU texture.";
        return nullptr;
    }

    texture->bind(0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(pixels);
    return texture;
}

void setWindowIcon(GLFWwindow* window) {
    if (!window) return;
    std::string iconPath = findIconPath();
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* pixels = stbi_load(iconPath.c_str(), &width, &height, &channels, 4);
    if (!pixels) {
        std::cerr << "Failed to load window icon: " << iconPath << "\n";
        return;
    }

    GLFWimage image;
    image.width = width;
    image.height = height;
    image.pixels = pixels;
    glfwSetWindowIcon(window, 1, &image);
    stbi_image_free(pixels);
}

void setTaskbarIcon(GLFWwindow* window) {
#ifdef _WIN32
    if (!window) return;
    HWND hwnd = glfwGetWin32Window(window);
    if (!hwnd) return;
    HICON icon = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr),
                                               MAKEINTRESOURCEW(101),
                                               IMAGE_ICON,
                                               0,
                                               0,
                                               LR_DEFAULTSIZE));
    if (!icon) {
        icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(101));
    }
    if (!icon) return;
    SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon));
    SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon));
    SetClassLongPtrW(hwnd, GCLP_HICON, reinterpret_cast<LONG_PTR>(icon));
    SetClassLongPtrW(hwnd, GCLP_HICONSM, reinterpret_cast<LONG_PTR>(icon));
#endif
}

struct Ray {
    glm::vec3 origin;
    glm::vec3 direction;
};

struct AABB {
    glm::vec3 min;
    glm::vec3 max;
};

Ray buildPickRay(const Camera& camera, double mouseX, double mouseY, int width, int height) {
    float x = (2.0f * static_cast<float>(mouseX)) / static_cast<float>(width) - 1.0f;
    float y = 1.0f - (2.0f * static_cast<float>(mouseY)) / static_cast<float>(height);
    glm::vec4 rayClipNear(x, y, -1.0f, 1.0f);
    glm::vec4 rayClipFar(x, y, 1.0f, 1.0f);

    glm::mat4 invVP = glm::inverse(camera.getProjectionMatrix() * camera.getViewMatrix());
    glm::vec4 rayNear = invVP * rayClipNear;
    glm::vec4 rayFar = invVP * rayClipFar;
    rayNear /= rayNear.w;
    rayFar /= rayFar.w;

    glm::vec3 origin = camera.getEye();
    glm::vec3 direction = glm::normalize(glm::vec3(rayFar - rayNear));
    return {origin, direction};
}

bool intersectRaySphere(const Ray& ray, const glm::vec3& center, float radius, float& tOut) {
    glm::vec3 oc = ray.origin - center;
    float b = glm::dot(oc, ray.direction);
    float c = glm::dot(oc, oc) - radius * radius;
    float h = b * b - c;
    if (h < 0.0f) return false;
    h = std::sqrt(h);
    float t = -b - h;
    if (t < 0.0f) t = -b + h;
    if (t < 0.0f) return false;
    tOut = t;
    return true;
}

bool intersectRayAABB(const Ray& ray, const AABB& box, float& tOut) {
    float tmin = 0.0f;
    float tmax = std::numeric_limits<float>::max();
    auto updateSlab = [&](float origin, float dir, float minB, float maxB) -> bool {
        const float eps = 1e-6f;
        if (std::fabs(dir) < eps) {
            return origin >= minB && origin <= maxB;
        }
        float inv = 1.0f / dir;
        float t1 = (minB - origin) * inv;
        float t2 = (maxB - origin) * inv;
        if (t1 > t2) std::swap(t1, t2);
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
        return tmin <= tmax;
    };

    if (!updateSlab(ray.origin.x, ray.direction.x, box.min.x, box.max.x)) return false;
    if (!updateSlab(ray.origin.y, ray.direction.y, box.min.y, box.max.y)) return false;
    if (!updateSlab(ray.origin.z, ray.direction.z, box.min.z, box.max.z)) return false;

    if (tmax < 0.0f) return false;
    tOut = tmin >= 0.0f ? tmin : tmax;
    return tOut >= 0.0f;
}

AABB transformAABB(const glm::mat4& transform, const glm::vec3& localMin, const glm::vec3& localMax) {
    glm::vec3 corners[8] = {
        {localMin.x, localMin.y, localMin.z},
        {localMax.x, localMin.y, localMin.z},
        {localMin.x, localMax.y, localMin.z},
        {localMax.x, localMax.y, localMin.z},
        {localMin.x, localMin.y, localMax.z},
        {localMax.x, localMin.y, localMax.z},
        {localMin.x, localMax.y, localMax.z},
        {localMax.x, localMax.y, localMax.z}
    };

    glm::vec3 outMin(std::numeric_limits<float>::max());
    glm::vec3 outMax(std::numeric_limits<float>::lowest());

    for (const auto& corner : corners) {
        glm::vec3 world = glm::vec3(transform * glm::vec4(corner, 1.0f));
        outMin = glm::min(outMin, world);
        outMax = glm::max(outMax, world);
    }

    return {outMin, outMax};
}

bool loadObjBounds(const std::string& path, glm::vec3& outMin, glm::vec3& outMax, std::string& error) {
    cloth::io::ObjMesh mesh;
    if (!cloth::io::loadOBJ(path, mesh, error)) {
        return false;
    }
    if (mesh.positions.empty()) {
        error = "OBJ has no positions.";
        return false;
    }
    outMin = glm::vec3(std::numeric_limits<float>::max());
    outMax = glm::vec3(std::numeric_limits<float>::lowest());
    for (size_t i = 0; i + 2 < mesh.positions.size(); i += 3) {
        glm::vec3 p(mesh.positions[i], mesh.positions[i + 1], mesh.positions[i + 2]);
        outMin = glm::min(outMin, p);
        outMax = glm::max(outMax, p);
    }
    return true;
}

void applyTexture(Material* material, bool useTexture, const TextureHandle& texture) {
    if (!material) return;
    if (useTexture && texture) {
        material->setTexture("texture_diffuse", texture);
    } else {
        material->setTexture("texture_diffuse", TextureHandle{});
    }
}

enum SelectedObject {
    SelectedNone = 0,
    SelectedFloor = 1,
    SelectedSphere = 2,
    SelectedCube = 3,
    SelectedTriangle = 4,
    SelectedObj = 5
};

struct FlyCamera {
    glm::vec3 position{30.0f, 22.0f, 38.0f};
    float yaw = -135.0f;
    float pitch = -15.0f;
    float moveSpeed = 18.0f;
    float lookSensitivity = 0.12f;
    bool rotating = false;
    double lastX = 0.0;
    double lastY = 0.0;
};

glm::vec3 computeFront(const FlyCamera& cam) {
    float radYaw = glm::radians(cam.yaw);
    float radPitch = glm::radians(cam.pitch);
    glm::vec3 front;
    front.x = cosf(radYaw) * cosf(radPitch);
    front.y = sinf(radPitch);
    front.z = sinf(radYaw) * cosf(radPitch);
    return glm::normalize(front);
}

void syncCamera(Camera& camera, const FlyCamera& state, int fbWidth, int fbHeight) {
    glm::vec3 eye = state.position;
    glm::vec3 front = computeFront(state);
    glm::vec3 up(0.0f, 1.0f, 0.0f);
    camera.set(eye, eye + front, up);
    camera.setShape(55.0f, static_cast<float>(fbWidth) / static_cast<float>(fbHeight), 0.1f, 250.0f);
}

void handleCameraInput(GLFWwindow* window, FlyCamera& cam, float dt, bool uiWantsInput) {
    if (uiWantsInput) return;

    glm::vec3 front = computeFront(cam);
    glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

    float speed = cam.moveSpeed * dt;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) speed *= 1.7f;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) cam.position += front * speed;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) cam.position -= front * speed;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) cam.position -= right * speed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) cam.position += right * speed;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) cam.position += up * speed;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) cam.position -= up * speed;
}

void handleMouseLook(GLFWwindow* window, FlyCamera& cam, bool uiWantsInput) {
    if (uiWantsInput) {
        cam.rotating = false;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        return;
    }

    int leftHeld = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
    if (leftHeld == GLFW_PRESS) {
        if (!cam.rotating) {
            cam.rotating = true;
            glfwGetCursorPos(window, &cam.lastX, &cam.lastY);
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        } else {
            double x, y;
            glfwGetCursorPos(window, &x, &y);
            double dx = x - cam.lastX;
            double dy = y - cam.lastY;
            cam.lastX = x;
            cam.lastY = y;

            cam.yaw += static_cast<float>(dx) * cam.lookSensitivity;
            cam.pitch -= static_cast<float>(dy) * cam.lookSensitivity;
            cam.pitch = std::clamp(cam.pitch, -89.0f, 89.0f);
        }
    } else if (cam.rotating) {
        cam.rotating = false;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

void handleScrollZoom(GLFWwindow* window, double xoffset, double yoffset) {
    (void)xoffset;
    if (!window) return;
    auto* cam = static_cast<FlyCamera*>(glfwGetWindowUserPointer(window));
    if (!cam) return;
    if (yoffset == 0.0) return;
    glm::vec3 front = computeFront(*cam);
    cam->position += front * static_cast<float>(yoffset) * 2.0f;
}

struct UISettings {
    bool showFloor = true;
    bool showSphere = true;
    bool showCube = true;
    bool showTriangle = true;
    bool showObj = true;
    int selectedObject = SelectedNone;
    bool requestOpenObjectPopup = false;
    bool cubeWireframe = false;
    bool triangleWireframe = false;
    bool objWireframe = false;
    bool floorWireframe = false;
    
    int floorMaterialType = 0; // 0=Phong, 1=Silk, 2=PBR
    int sphereMaterialType = 1; // 0=Phong, 1=Silk, 2=PBR
    int cubeMaterialType = 0;
    int triangleMaterialType = 0;
    int objMaterialType = 0;
    float sphereIor = 1.52f;
    float cubeIor = 1.52f;
    float triangleIor = 1.52f;
    float objIor = 1.52f;
    
    glm::vec3 floorColor{0.35f, 0.35f, 0.38f};
    glm::vec3 sphereColor{0.8f, 0.45f, 0.45f};
    glm::vec3 cubeColor{0.3f, 0.7f, 1.0f};
    glm::vec3 triangleColor{1.0f, 0.6f, 0.2f};
    glm::vec3 objColor{0.8f, 0.8f, 0.8f};
    
    // Floor material properties
    float floorShininess = 32.0f;
    float floorMetallic = 0.1f;
    float floorRoughness = 0.3f;
    glm::vec3 floorSpecular{0.5f, 0.5f, 0.5f};

    // Sphere material properties
    float sphereShininess = 32.0f;
    float sphereMetallic = 0.1f;
    float sphereRoughness = 0.3f;
    glm::vec3 sphereSpecular{0.5f, 0.5f, 0.5f};
    
    // Cube material properties  
    float cubeShininess = 32.0f;
    float cubeMetallic = 0.1f;
    float cubeRoughness = 0.3f;
    glm::vec3 cubeSpecular{0.5f, 0.5f, 0.5f};

    float triangleShininess = 32.0f;
    float triangleMetallic = 0.1f;
    float triangleRoughness = 0.3f;
    glm::vec3 triangleSpecular{0.5f, 0.5f, 0.5f};

    float objShininess = 32.0f;
    float objMetallic = 0.1f;
    float objRoughness = 0.3f;
    glm::vec3 objSpecular{0.5f, 0.5f, 0.5f};
    
    // Lighting settings
    bool shadowEnabled = true;
    float shadowBias = 0.005f;
    float shadowSoftness = 2.0f;
    bool useCascadedShadows = true;
    bool debugCascades = false;
    int cascadeCount = 4;
    float cascadeSplitLambda = 0.6f;
    float cascadeMaxDistance = 120.0f;
    
    bool ssaoEnabled = true;
    float ssaoRadius = 5.0f;
    float ssaoIntensity = 1.5f;
    float ssaoBias = 0.025f;
    
    glm::vec3 lightPosition{35.0f, 60.0f, 35.0f};
    glm::vec3 lightAmbient{0.2f, 0.2f, 0.2f};
    glm::vec3 lightDiffuse{1.1f, 1.0f, 0.95f};
    glm::vec3 lightSpecular{1.0f, 1.0f, 1.0f};
    
    std::vector<sandbox::ExtraLight> extraLights;

    float envMapIntensity = 0.35f;
    bool iblEnabled = true;
    float iblIntensity = 1.0f;
    bool iblReady = false;

    char objPath[260] = "";
    float objScale = 1.0f;
    glm::vec3 objPosition{0.0f, 2.0f, 8.0f};
    std::string objStatus;
    bool requestObjLoad = false;

    char objTexturePath[260] = "C:\\Users\\detou\\Desktop\\Eye\\Textures";
    bool objUseTexture = true;
    std::string objTextureStatus;
    bool requestObjTextureLoad = false;

    char floorTexturePath[260] = "";
    bool floorUseTexture = false;
    std::string floorTextureStatus;
    bool requestFloorTextureLoad = false;

    char sphereTexturePath[260] = "";
    bool sphereUseTexture = false;
    std::string sphereTextureStatus;
    bool requestSphereTextureLoad = false;

    char cubeTexturePath[260] = "";
    bool cubeUseTexture = false;
    std::string cubeTextureStatus;
    bool requestCubeTextureLoad = false;

    char triangleTexturePath[260] = "";
    bool triangleUseTexture = false;
    std::string triangleTextureStatus;
    bool requestTriangleTextureLoad = false;
};

void renderUI(UISettings& settings, const UnifiedRenderer& renderer, float fps) {
    // Info HUD
    const float infoWidth = 260.0f;
    const float padding = 10.0f;
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - infoWidth - padding, padding), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(infoWidth, 180.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.75f);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;
    ImGui::Begin("InfoHUD", nullptr, flags);
    ImGui::Text("FPS: %.1f", fps);
    
    auto stats = renderer.getStats();
    ImGui::Separator();
    ImGui::Text("Render Stats");
    ImGui::Text("Draw calls: %d", stats.drawCalls);
    ImGui::Text("Instances: %d", stats.instances);
    ImGui::Text("Triangles: %d", stats.triangles);
    
    ImGui::Separator();
    ImGui::Text("Navigation");
    ImGui::Text("WASD/QE  - Move");
    ImGui::Text("LMB drag - Look");
    ImGui::Text("Mouse wheel - Zoom");
    ImGui::Text("Shift    - Fast move");
    ImGui::Text("Esc      - Quit");
    ImGui::End();

    // Settings Panel
    ImGui::Begin("Scene Settings");
    
    if (ImGui::CollapsingHeader("Visibility", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Show Floor", &settings.showFloor);
        ImGui::Checkbox("Show Sphere", &settings.showSphere);
        ImGui::Checkbox("Show Cube", &settings.showCube);
        ImGui::Checkbox("Show Triangle", &settings.showTriangle);
        ImGui::Checkbox("Show OBJ", &settings.showObj);
    }
    
    if (ImGui::CollapsingHeader("Visual Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Shadows");
        ImGui::Checkbox("Enable Shadows", &settings.shadowEnabled);
        if (settings.shadowEnabled) {
            ImGui::SliderFloat("Shadow Bias", &settings.shadowBias, 0.001f, 0.02f, "%.4f");
            ImGui::SliderFloat("Shadow Softness", &settings.shadowSoftness, 1.0f, 4.0f, "%.0f");
            ImGui::Separator();
            ImGui::Checkbox("Cascaded Shadows", &settings.useCascadedShadows);
            if (settings.useCascadedShadows) {
                ImGui::Checkbox("Debug Cascades", &settings.debugCascades);
                ImGui::SliderInt("Cascade Count", &settings.cascadeCount, 1, 4);
                ImGui::SliderFloat("Cascade Lambda", &settings.cascadeSplitLambda, 0.0f, 1.0f, "%.2f");
                ImGui::SliderFloat("Cascade Distance", &settings.cascadeMaxDistance, 20.0f, 250.0f, "%.1f");
            }
        }
        ImGui::Separator();
        ImGui::Text("Screen-Space AO");
        ImGui::Checkbox("Enable SSAO", &settings.ssaoEnabled);
        if (settings.ssaoEnabled) {
            ImGui::SliderFloat("SSAO Radius", &settings.ssaoRadius, 0.5f, 10.0f, "%.1f");
            ImGui::SliderFloat("SSAO Intensity", &settings.ssaoIntensity, 0.5f, 4.0f, "%.2f");
            ImGui::SliderFloat("SSAO Bias", &settings.ssaoBias, 0.001f, 0.1f, "%.3f");
        }
    }
    
    if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Main Light");
        ImGui::SliderFloat3("Position", &settings.lightPosition.x, -200.0f, 200.0f, "%.1f");
        ImGui::Separator();
        ImGui::Text("Light Colors");
        ImGui::ColorEdit3("Ambient", &settings.lightAmbient.x);
        ImGui::ColorEdit3("Diffuse", &settings.lightDiffuse.x);
        ImGui::ColorEdit3("Specular", &settings.lightSpecular.x);
        
        ImGui::Separator();
        ImGui::Text("Additional Lights");
        if (ImGui::Button("+ Add Light")) {
            sandbox::ExtraLight extra;
            extra.enabled = true;
            extra.position[0] = -30.0f + static_cast<float>(settings.extraLights.size()) * 20.0f;
            extra.position[1] = 60.0f;
            extra.position[2] = 30.0f;
            extra.diffuse[0] = 1.0f;
            extra.diffuse[1] = 1.0f;
            extra.diffuse[2] = 1.0f;
            extra.intensity = 1.0f;
            extra.castsShadow = false;
            settings.extraLights.push_back(extra);
        }
        
        int removeIndex = -1;
        for (size_t i = 0; i < settings.extraLights.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            auto& light = settings.extraLights[i];
            char header[64];
            snprintf(header, sizeof(header), "Light %d %s", static_cast<int>(i + 1),
                     light.enabled ? "" : "(Off)");
            if (ImGui::TreeNode(header)) {
                ImGui::Checkbox("Enabled", &light.enabled);
                ImGui::SliderFloat3("Position", light.position, -200.0f, 200.0f, "%.1f");
                ImGui::ColorEdit3("Color", light.diffuse);
                ImGui::SliderFloat("Intensity", &light.intensity, 0.0f, 3.0f, "%.2f");
                ImGui::Checkbox("Cast Shadow", &light.castsShadow);
                if (ImGui::Button("Remove")) {
                    removeIndex = static_cast<int>(i);
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        if (removeIndex >= 0) {
            settings.extraLights.erase(settings.extraLights.begin() + removeIndex);
        }
    }
    
    if (ImGui::CollapsingHeader("Materials", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Sphere Material");
        const char* matTypes[] = {"Phong", "Silk", "PBR", "Refraction"};
        ImGui::Combo("##SphMat", &settings.sphereMaterialType, matTypes, 4);
        ImGui::ColorEdit3("Sphere Color", &settings.sphereColor.x);
        
        // Show material-specific properties based on type
        if (settings.sphereMaterialType == 0) {  // Phong
            ImGui::SliderFloat("Shininess##Sph", &settings.sphereShininess, 1.0f, 256.0f, "%.0f");
            ImGui::ColorEdit3("Specular##Sph", &settings.sphereSpecular.x);
        } else if (settings.sphereMaterialType == 1) {  // Silk
            ImGui::SliderFloat("Shininess##Sph", &settings.sphereShininess, 8.0f, 128.0f, "%.0f");
            ImGui::ColorEdit3("Specular##Sph", &settings.sphereSpecular.x);
        } else if (settings.sphereMaterialType == 2) {  // PBR
            ImGui::SliderFloat("Metallic##Sph", &settings.sphereMetallic, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Roughness##Sph", &settings.sphereRoughness, 0.04f, 1.0f, "%.2f");
        } else if (settings.sphereMaterialType == 3) { // Refraction
            ImGui::SliderFloat("IOR##Sph", &settings.sphereIor, 1.0f, 2.5f, "%.2f");
        }
        
        ImGui::Separator();
        ImGui::Text("Cube Material");
        ImGui::Combo("##CubeMat", &settings.cubeMaterialType, matTypes, 4);
        ImGui::ColorEdit3("Cube Color", &settings.cubeColor.x);
        
        // Show material-specific properties based on type
        if (settings.cubeMaterialType == 0) {  // Phong
            ImGui::SliderFloat("Shininess##Cube", &settings.cubeShininess, 1.0f, 256.0f, "%.0f");
            ImGui::ColorEdit3("Specular##Cube", &settings.cubeSpecular.x);
        } else if (settings.cubeMaterialType == 1) {  // Silk
            ImGui::SliderFloat("Shininess##Cube", &settings.cubeShininess, 8.0f, 128.0f, "%.0f");
            ImGui::ColorEdit3("Specular##Cube", &settings.cubeSpecular.x);
        } else if (settings.cubeMaterialType == 2) {  // PBR
            ImGui::SliderFloat("Metallic##Cube", &settings.cubeMetallic, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Roughness##Cube", &settings.cubeRoughness, 0.04f, 1.0f, "%.2f");
        } else if (settings.cubeMaterialType == 3) { // Refraction
            ImGui::SliderFloat("IOR##Cube", &settings.cubeIor, 1.0f, 2.5f, "%.2f");
        }
        
        ImGui::Checkbox("Cube Wireframe", &settings.cubeWireframe);
        
        ImGui::Separator();
        ImGui::ColorEdit3("Triangle Color", &settings.triangleColor.x);
        ImGui::Checkbox("Triangle Wireframe", &settings.triangleWireframe);
    }

    if (ImGui::CollapsingHeader("OBJ Model", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* matTypes[] = {"Phong", "Silk", "PBR", "Refraction"};
        ImGui::InputText("OBJ Path", settings.objPath, sizeof(settings.objPath));
        if (ImGui::Button("Load OBJ")) {
            settings.requestObjLoad = true;
        }
        if (!settings.objStatus.empty()) {
            ImGui::Text("Status: %s", settings.objStatus.c_str());
        } else {
            ImGui::Text("Status: No OBJ loaded");
        }
        ImGui::SliderFloat("OBJ Scale", &settings.objScale, 0.01f, 20.0f, "%.2f");
        ImGui::SliderFloat3("OBJ Position", &settings.objPosition.x, -50.0f, 50.0f, "%.1f");
        ImGui::Separator();
        ImGui::Combo("OBJ Material", &settings.objMaterialType, matTypes, 4);
        ImGui::ColorEdit3("OBJ Color", &settings.objColor.x);
        if (settings.objMaterialType == 0) {
            ImGui::SliderFloat("Shininess##Obj", &settings.objShininess, 1.0f, 256.0f, "%.0f");
            ImGui::ColorEdit3("Specular##Obj", &settings.objSpecular.x);
        } else if (settings.objMaterialType == 1) {
            ImGui::SliderFloat("Shininess##Obj", &settings.objShininess, 8.0f, 128.0f, "%.0f");
            ImGui::ColorEdit3("Specular##Obj", &settings.objSpecular.x);
        } else if (settings.objMaterialType == 2) {
            ImGui::SliderFloat("Metallic##Obj", &settings.objMetallic, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Roughness##Obj", &settings.objRoughness, 0.04f, 1.0f, "%.2f");
        } else if (settings.objMaterialType == 3) {
            ImGui::SliderFloat("IOR##Obj", &settings.objIor, 1.0f, 2.5f, "%.2f");
        }
        ImGui::Checkbox("OBJ Wireframe", &settings.objWireframe);
        ImGui::Separator();
        ImGui::InputText("OBJ Texture", settings.objTexturePath, sizeof(settings.objTexturePath));
        if (ImGui::Button("Load OBJ Texture")) {
            settings.requestObjTextureLoad = true;
        }
        ImGui::Checkbox("Use OBJ Texture", &settings.objUseTexture);
        if (!settings.objTextureStatus.empty()) {
            ImGui::Text("Texture: %s", settings.objTextureStatus.c_str());
        }
    }

    if (ImGui::CollapsingHeader("Environment", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Env Intensity", &settings.envMapIntensity, 0.0f, 2.0f, "%.2f");
        if (!settings.iblReady) {
            ImGui::BeginDisabled();
        }
        ImGui::Checkbox("IBL Enabled", &settings.iblEnabled);
        ImGui::SliderFloat("IBL Intensity", &settings.iblIntensity, 0.0f, 2.0f, "%.2f");
        if (!settings.iblReady) {
            ImGui::EndDisabled();
        }
    }
    
    ImGui::End();

    if (settings.requestOpenObjectPopup) {
        ImGui::OpenPopup("Object Properties");
        settings.requestOpenObjectPopup = false;
    }

    if (ImGui::BeginPopup("Object Properties")) {
        const char* matTypes[] = {"Phong", "Silk", "PBR", "Refraction"};
        const char* floorMatTypes[] = {"Phong", "Silk", "PBR"};
        const char* objectName = "None";
        if (settings.selectedObject == SelectedFloor) objectName = "Floor";
        else if (settings.selectedObject == SelectedSphere) objectName = "Sphere";
        else if (settings.selectedObject == SelectedCube) objectName = "Cube";
        else if (settings.selectedObject == SelectedTriangle) objectName = "Triangle";
        else if (settings.selectedObject == SelectedObj) objectName = "OBJ";

        ImGui::Text("Selected: %s", objectName);
        ImGui::Separator();

        if (settings.selectedObject == SelectedFloor) {
            ImGui::Combo("Material", &settings.floorMaterialType, floorMatTypes, 3);
            ImGui::ColorEdit3("Color", &settings.floorColor.x);
            ImGui::SliderFloat("Shininess", &settings.floorShininess, 1.0f, 256.0f, "%.0f");
            ImGui::ColorEdit3("Specular", &settings.floorSpecular.x);
            ImGui::SliderFloat("Metallic", &settings.floorMetallic, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Roughness", &settings.floorRoughness, 0.04f, 1.0f, "%.2f");
            ImGui::Checkbox("Wireframe", &settings.floorWireframe);
            ImGui::Separator();
            ImGui::InputText("Texture", settings.floorTexturePath, sizeof(settings.floorTexturePath));
            if (ImGui::Button("Load Texture")) {
                settings.requestFloorTextureLoad = true;
            }
            ImGui::Checkbox("Use Texture", &settings.floorUseTexture);
            if (!settings.floorTextureStatus.empty()) {
                ImGui::Text("Texture: %s", settings.floorTextureStatus.c_str());
            }
        } else if (settings.selectedObject == SelectedSphere) {
            ImGui::Combo("Material", &settings.sphereMaterialType, matTypes, 4);
            ImGui::ColorEdit3("Color", &settings.sphereColor.x);
            if (settings.sphereMaterialType == 0 || settings.sphereMaterialType == 1) {
                ImGui::SliderFloat("Shininess", &settings.sphereShininess, 1.0f, 256.0f, "%.0f");
                ImGui::ColorEdit3("Specular", &settings.sphereSpecular.x);
            } else if (settings.sphereMaterialType == 2) {
                ImGui::SliderFloat("Metallic", &settings.sphereMetallic, 0.0f, 1.0f, "%.2f");
                ImGui::SliderFloat("Roughness", &settings.sphereRoughness, 0.04f, 1.0f, "%.2f");
            } else if (settings.sphereMaterialType == 3) {
                ImGui::SliderFloat("IOR", &settings.sphereIor, 1.0f, 2.5f, "%.2f");
            }
            bool allowTexture = settings.sphereMaterialType != 3;
            if (!allowTexture) ImGui::BeginDisabled();
            ImGui::Separator();
            ImGui::InputText("Texture", settings.sphereTexturePath, sizeof(settings.sphereTexturePath));
            if (ImGui::Button("Load Texture")) {
                settings.requestSphereTextureLoad = true;
            }
            ImGui::Checkbox("Use Texture", &settings.sphereUseTexture);
            if (!settings.sphereTextureStatus.empty()) {
                ImGui::Text("Texture: %s", settings.sphereTextureStatus.c_str());
            }
            if (!allowTexture) ImGui::EndDisabled();
        } else if (settings.selectedObject == SelectedCube) {
            ImGui::Combo("Material", &settings.cubeMaterialType, matTypes, 4);
            ImGui::ColorEdit3("Color", &settings.cubeColor.x);
            if (settings.cubeMaterialType == 0 || settings.cubeMaterialType == 1) {
                ImGui::SliderFloat("Shininess", &settings.cubeShininess, 1.0f, 256.0f, "%.0f");
                ImGui::ColorEdit3("Specular", &settings.cubeSpecular.x);
            } else if (settings.cubeMaterialType == 2) {
                ImGui::SliderFloat("Metallic", &settings.cubeMetallic, 0.0f, 1.0f, "%.2f");
                ImGui::SliderFloat("Roughness", &settings.cubeRoughness, 0.04f, 1.0f, "%.2f");
            } else if (settings.cubeMaterialType == 3) {
                ImGui::SliderFloat("IOR", &settings.cubeIor, 1.0f, 2.5f, "%.2f");
            }
            ImGui::Checkbox("Wireframe", &settings.cubeWireframe);
            bool allowTexture = settings.cubeMaterialType != 3;
            if (!allowTexture) ImGui::BeginDisabled();
            ImGui::Separator();
            ImGui::InputText("Texture", settings.cubeTexturePath, sizeof(settings.cubeTexturePath));
            if (ImGui::Button("Load Texture")) {
                settings.requestCubeTextureLoad = true;
            }
            ImGui::Checkbox("Use Texture", &settings.cubeUseTexture);
            if (!settings.cubeTextureStatus.empty()) {
                ImGui::Text("Texture: %s", settings.cubeTextureStatus.c_str());
            }
            if (!allowTexture) ImGui::EndDisabled();
        } else if (settings.selectedObject == SelectedTriangle) {
            ImGui::Combo("Material", &settings.triangleMaterialType, matTypes, 4);
            ImGui::ColorEdit3("Color", &settings.triangleColor.x);
            if (settings.triangleMaterialType == 0 || settings.triangleMaterialType == 1) {
                ImGui::SliderFloat("Shininess", &settings.triangleShininess, 1.0f, 256.0f, "%.0f");
                ImGui::ColorEdit3("Specular", &settings.triangleSpecular.x);
            } else if (settings.triangleMaterialType == 2) {
                ImGui::SliderFloat("Metallic", &settings.triangleMetallic, 0.0f, 1.0f, "%.2f");
                ImGui::SliderFloat("Roughness", &settings.triangleRoughness, 0.04f, 1.0f, "%.2f");
            } else if (settings.triangleMaterialType == 3) {
                ImGui::SliderFloat("IOR", &settings.triangleIor, 1.0f, 2.5f, "%.2f");
            }
            ImGui::Checkbox("Wireframe", &settings.triangleWireframe);
            bool allowTexture = settings.triangleMaterialType != 3;
            if (!allowTexture) ImGui::BeginDisabled();
            ImGui::Separator();
            ImGui::InputText("Texture", settings.triangleTexturePath, sizeof(settings.triangleTexturePath));
            if (ImGui::Button("Load Texture")) {
                settings.requestTriangleTextureLoad = true;
            }
            ImGui::Checkbox("Use Texture", &settings.triangleUseTexture);
            if (!settings.triangleTextureStatus.empty()) {
                ImGui::Text("Texture: %s", settings.triangleTextureStatus.c_str());
            }
            if (!allowTexture) ImGui::EndDisabled();
        } else if (settings.selectedObject == SelectedObj) {
            ImGui::Text("OBJ");
            ImGui::Combo("Material", &settings.objMaterialType, matTypes, 4);
            ImGui::ColorEdit3("Color", &settings.objColor.x);
            if (settings.objMaterialType == 0 || settings.objMaterialType == 1) {
                ImGui::SliderFloat("Shininess", &settings.objShininess, 1.0f, 256.0f, "%.0f");
                ImGui::ColorEdit3("Specular", &settings.objSpecular.x);
            } else if (settings.objMaterialType == 2) {
                ImGui::SliderFloat("Metallic", &settings.objMetallic, 0.0f, 1.0f, "%.2f");
                ImGui::SliderFloat("Roughness", &settings.objRoughness, 0.04f, 1.0f, "%.2f");
            } else if (settings.objMaterialType == 3) {
                ImGui::SliderFloat("IOR", &settings.objIor, 1.0f, 2.5f, "%.2f");
            }
            ImGui::Checkbox("Wireframe", &settings.objWireframe);
            bool allowTexture = settings.objMaterialType != 3;
            if (!allowTexture) ImGui::BeginDisabled();
            ImGui::Separator();
            ImGui::InputText("Texture", settings.objTexturePath, sizeof(settings.objTexturePath));
            if (ImGui::Button("Load Texture")) {
                settings.requestObjTextureLoad = true;
            }
            ImGui::Checkbox("Use Texture", &settings.objUseTexture);
            if (!settings.objTextureStatus.empty()) {
                ImGui::Text("Texture: %s", settings.objTextureStatus.c_str());
            }
            if (!allowTexture) ImGui::EndDisabled();
        }

        ImGui::EndPopup();
    }
}

// Helper to create material based on type
Material* createMaterialByType(int type, const glm::vec3& color, float ior) {
    switch (type) {
        case 0: return Material::createPhong(color);
        case 1: return Material::createSilk(color);
        case 2: return Material::createSilkPBR(color);
        case 3: {
            auto* mat = new Material("Refraction");
            mat->setRefractionIor(ior);
            return mat;
        }
        default: return Material::createPhong(color);
    }
}

} // namespace

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    const int initialWidth = 1280;
    const int initialHeight = 720;
    GLFWwindow* window = glfwCreateWindow(initialWidth, initialHeight, "SandboxGE Unified Demo", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGL(glfwGetProcAddress)) {
        std::cerr << "Failed to load OpenGL functions\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    std::cout << "OpenGL " << glGetString(GL_VERSION) << "\n";
    setWindowIcon(window);
    setTaskbarIcon(window);

    // ImGui setup
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 430");

    int fbWidth = 0, fbHeight = 0;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    fbWidth = std::max(1, fbWidth);
    fbHeight = std::max(1, fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    // Initialize shader system
    std::string shaderRoot = findShaderRoot();
    ShaderPath::setRoot(shaderRoot);
    std::cout << "Shader root: " << shaderRoot << "\n";

    // Pre-load shaders
    ShaderLib* shaderLib = ShaderLib::instance();
    (*shaderLib)["PhongUBO"];
    (*shaderLib)["SilkUBO"];
    (*shaderLib)["SilkPBR_UBO"];
    (*shaderLib)["Refraction"];
    std::cout << "Shaders loaded successfully\n";

    // Initialize camera
    Camera camera(
        glm::vec3(30.0f, 22.0f, 38.0f),
        glm::vec3(0.0f, 6.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        Camera::PERSPECTIVE
    );
    camera.setShape(55.0f, static_cast<float>(fbWidth) / static_cast<float>(fbHeight), 0.1f, 250.0f);
    FlyCamera camState;
    glfwSetWindowUserPointer(window, &camState);
    glfwSetScrollCallback(window, handleScrollZoom);

    // Initialize renderer
    UnifiedRenderer renderer;
    if (!renderer.initialize(fbWidth, fbHeight)) {
        std::cerr << "Failed to initialize UnifiedRenderer\n";
        return 1;
    }
    std::cout << "UnifiedRenderer initialized\n";

    // Create scene objects
    std::cout << "Creating floor...\n";
    FloorRenderable floor(80.0f, 80.0f, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.35f, 0.35f, 0.38f));
    std::cout << "Floor created\n";
    
    std::cout << "Creating sphere...\n";
    SphereRenderable sphere(4.0f, glm::vec3(10.0f, 4.0f, -4.0f), glm::vec3(0.8f, 0.45f, 0.45f));
    std::cout << "Sphere created\n";
    
    // Create cube
    std::cout << "Creating cube geometry...\n";
    auto cubeGeom = GeometryFactory::instance().createCube(1.0f);
    std::cout << "Creating cube material...\n";
    Material* cubeMaterial = nullptr;
    try {
        cubeMaterial = Material::createPhong(glm::vec3(0.3f, 0.7f, 1.0f));
        std::cout << "Cube material created\n";
    } catch (const std::exception& e) {
        std::cerr << "Exception creating cube material: " << e.what() << "\n";
        return 1;
    }
    std::cout << "Creating cube renderable...\n";
    glm::mat4 cubeTransform = glm::translate(glm::mat4(1.0f), glm::vec3(-10.0f, 3.0f, 0.0f));
    cubeTransform = glm::scale(cubeTransform, glm::vec3(6.0f, 4.0f, 6.0f));
    MeshRenderable cube(cubeGeom, cubeMaterial, cubeTransform);
    std::cout << "Cube created\n";
    
    // Create triangle
    std::cout << "Creating triangle geometry...\n";
    auto triangleGeom = GeometryFactory::instance().createTriangle(1.0f);
    std::cout << "Creating triangle material...\n";
    Material* triangleMaterial = Material::createPhong(glm::vec3(1.0f, 0.6f, 0.2f));
    std::cout << "Triangle material created\n";
    glm::mat4 triangleTransform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 6.0f, 0.0f));
    triangleTransform = glm::scale(triangleTransform, glm::vec3(6.0f));
    MeshRenderable triangle(triangleGeom, triangleMaterial, triangleTransform);
    std::cout << "Triangle created\n";
    std::cout << "All scene objects created successfully\n";

    UISettings uiSettings;
    std::unique_ptr<MeshRenderable> objMesh;
    GeometryHandle objGeometry;
    Material* objMaterial = nullptr;
    TextureHandle objTexture;
    TextureHandle floorTexture;
    TextureHandle sphereTexture;
    TextureHandle cubeTexture;
    TextureHandle triangleTexture;
    glm::vec3 objLocalMin(0.0f);
    glm::vec3 objLocalMax(0.0f);
    bool objBoundsValid = false;
    float lastTime = static_cast<float>(glfwGetTime());
    float orbitRadius = 12.0f;
    float orbitHeight = 6.0f;
    int lastRightButton = GLFW_RELEASE;

    // Render settings
    RenderSettings renderSettings;

    int envWidth = 0;
    int envHeight = 0;
    GLuint envMapTex = gfx::loadEnvironmentEXR(findEnvMapPath(), &envWidth, &envHeight);
    renderSettings.envMapTextureId = envMapTex;
    if (envMapTex == 0) {
        renderSettings.envMapIntensity = 0.0f;
    }

    gfx::IBLProcessor ibl;
    if (envMapTex != 0 && ibl.initialize()) {
        if (ibl.build(envMapTex)) {
            renderSettings.iblIrradianceMap = ibl.getIrradianceMap();
            renderSettings.iblPrefilterMap = ibl.getPrefilterMap();
            renderSettings.iblBrdfLut = ibl.getBrdfLut();
            uiSettings.iblReady = true;
        } else {
            renderSettings.iblEnabled = false;
            renderSettings.iblIntensity = 0.0f;
            uiSettings.iblReady = false;
        }
    } else {
        renderSettings.iblEnabled = false;
        renderSettings.iblIntensity = 0.0f;
        uiSettings.iblReady = false;
    }
    uiSettings.iblEnabled = renderSettings.iblEnabled;
    uiSettings.envMapIntensity = renderSettings.envMapIntensity;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        // Handle window resize
        int newWidth = 0, newHeight = 0;
        glfwGetFramebufferSize(window, &newWidth, &newHeight);
        newWidth = std::max(1, newWidth);
        newHeight = std::max(1, newHeight);
        if (newWidth != fbWidth || newHeight != fbHeight) {
            fbWidth = newWidth;
            fbHeight = newHeight;
            glViewport(0, 0, fbWidth, fbHeight);
            camera.setShape(55.0f, static_cast<float>(fbWidth) / static_cast<float>(fbHeight), 0.1f, 250.0f);
            renderer.resize(fbWidth, fbHeight);
        }

        float t = static_cast<float>(glfwGetTime());
        float dt = t - lastTime;
        lastTime = t;

        // ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiIO& io = ImGui::GetIO();
        bool uiWantsMouse = io.WantCaptureMouse;
        bool uiWantsKeyboard = io.WantCaptureKeyboard;
        bool popupOpen = ImGui::IsPopupOpen("Object Properties");

        int rightButton = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT);
        bool requestPick = (rightButton == GLFW_PRESS && lastRightButton == GLFW_RELEASE && !uiWantsMouse);
        lastRightButton = rightButton;

        // Camera controls
        handleMouseLook(window, camState, uiWantsMouse || requestPick || popupOpen);
        handleCameraInput(window, camState, dt, uiWantsMouse || uiWantsKeyboard);
        syncCamera(camera, camState, fbWidth, fbHeight);

        if (requestPick) {
            double mouseX = 0.0;
            double mouseY = 0.0;
            glfwGetCursorPos(window, &mouseX, &mouseY);
            Ray ray = buildPickRay(camera, mouseX, mouseY, fbWidth, fbHeight);
            int hit = SelectedNone;
            float bestT = std::numeric_limits<float>::max();

            if (uiSettings.showSphere) {
                float tHit = 0.0f;
                if (intersectRaySphere(ray, sphere.getPosition(), sphere.getRadius(), tHit) && tHit < bestT) {
                    bestT = tHit;
                    hit = SelectedSphere;
                }
            }

            const glm::vec3 unitMin(-0.5f);
            const glm::vec3 unitMax(0.5f);
            if (uiSettings.showFloor) {
                float tHit = 0.0f;
                AABB world = transformAABB(floor.getTransform(), unitMin, unitMax);
                if (intersectRayAABB(ray, world, tHit) && tHit < bestT) {
                    bestT = tHit;
                    hit = SelectedFloor;
                }
            }
            if (uiSettings.showCube) {
                float tHit = 0.0f;
                AABB world = transformAABB(cube.getTransform(), unitMin, unitMax);
                if (intersectRayAABB(ray, world, tHit) && tHit < bestT) {
                    bestT = tHit;
                    hit = SelectedCube;
                }
            }
            if (uiSettings.showTriangle) {
                float tHit = 0.0f;
                float triHalf = 0.5f;
                float triHeight = 0.866f;
                glm::vec3 triMin(-triHalf, 0.0f, -triHeight / 3.0f);
                glm::vec3 triMax(triHalf, triHeight, triHeight * 2.0f / 3.0f);
                AABB world = transformAABB(triangle.getTransform(), triMin, triMax);
                if (intersectRayAABB(ray, world, tHit) && tHit < bestT) {
                    bestT = tHit;
                    hit = SelectedTriangle;
                }
            }
            if (uiSettings.showObj && objMesh && objBoundsValid) {
                float tHit = 0.0f;
                AABB world = transformAABB(objMesh->getTransform(), objLocalMin, objLocalMax);
                if (intersectRayAABB(ray, world, tHit) && tHit < bestT) {
                    bestT = tHit;
                    hit = SelectedObj;
                }
            }

            if (hit != SelectedNone) {
                uiSettings.selectedObject = hit;
                uiSettings.requestOpenObjectPopup = true;
            }
        }

        // Update animated objects
        glm::mat4 cubeModel = glm::translate(glm::mat4(1.0f), glm::vec3(-10.0f, 3.0f, 0.0f));
        cubeModel = glm::rotate(cubeModel, 0.35f * t, glm::vec3(0.0f, 1.0f, 0.0f));
        cubeModel = glm::scale(cubeModel, glm::vec3(6.0f, 4.0f, 6.0f));
        cube.setTransform(cubeModel);

        glm::vec3 orbitPos = glm::vec3(std::cos(t) * orbitRadius, orbitHeight, std::sin(t) * orbitRadius);
        glm::mat4 triangleModel = glm::translate(glm::mat4(1.0f), orbitPos);
        triangleModel = glm::rotate(triangleModel, t * 2.0f, glm::vec3(0.0f, 1.0f, 0.0f));
        triangleModel = glm::scale(triangleModel, glm::vec3(6.0f));
        triangle.setTransform(triangleModel);

        // Update materials if changed
        static int lastFloorMat = -1;
        static int lastSphereMat = -1;
        static int lastCubeMat = -1;
        static int lastTriangleMat = -1;
        static int lastObjMat = -1;
        static bool lastFloorUseTexture = false;
        static bool lastSphereUseTexture = false;
        static bool lastCubeUseTexture = false;
        static bool lastTriangleUseTexture = false;
        static bool lastObjUseTexture = true;
        const int floorMatPreset = std::clamp(uiSettings.floorMaterialType, 0, 2);
        if (floorMatPreset != lastFloorMat) {
            FloorRenderable::MaterialPreset preset = FloorRenderable::MaterialPreset::Phong;
            if (floorMatPreset == 1) preset = FloorRenderable::MaterialPreset::Silk;
            if (floorMatPreset == 2) preset = FloorRenderable::MaterialPreset::SilkPBR;
            floor.setMaterialPreset(preset);
            applyTexture(floor.getMaterial(), uiSettings.floorUseTexture, floorTexture);
            lastFloorMat = floorMatPreset;
        }
        if (uiSettings.sphereMaterialType != lastSphereMat) {
            Material* newMat = createMaterialByType(uiSettings.sphereMaterialType, uiSettings.sphereColor, uiSettings.sphereIor);
            sphere.setMaterial(newMat);
            bool allowTexture = uiSettings.sphereMaterialType != 3;
            applyTexture(sphere.getMaterial(), allowTexture && uiSettings.sphereUseTexture, sphereTexture);
            lastSphereMat = uiSettings.sphereMaterialType;
        }
        if (uiSettings.cubeMaterialType != lastCubeMat) {
            delete cubeMaterial;
            cubeMaterial = createMaterialByType(uiSettings.cubeMaterialType, uiSettings.cubeColor, uiSettings.cubeIor);
            cube.setMaterial(cubeMaterial);
            bool allowTexture = uiSettings.cubeMaterialType != 3;
            applyTexture(cubeMaterial, allowTexture && uiSettings.cubeUseTexture, cubeTexture);
            lastCubeMat = uiSettings.cubeMaterialType;
        }
        if (uiSettings.triangleMaterialType != lastTriangleMat) {
            delete triangleMaterial;
            triangleMaterial = createMaterialByType(uiSettings.triangleMaterialType, uiSettings.triangleColor, uiSettings.triangleIor);
            triangle.setMaterial(triangleMaterial);
            bool allowTexture = uiSettings.triangleMaterialType != 3;
            applyTexture(triangleMaterial, allowTexture && uiSettings.triangleUseTexture, triangleTexture);
            lastTriangleMat = uiSettings.triangleMaterialType;
        }
        if (uiSettings.objMaterialType != lastObjMat && objMesh) {
            delete objMaterial;
            objMaterial = createMaterialByType(uiSettings.objMaterialType, uiSettings.objColor, uiSettings.objIor);
            objMesh->setMaterial(objMaterial);
            bool allowTexture = uiSettings.objMaterialType != 3;
            applyTexture(objMaterial, allowTexture && uiSettings.objUseTexture, objTexture);
            lastObjMat = uiSettings.objMaterialType;
        }

        // Update colors
        floor.setColor(uiSettings.floorColor);
        floor.setWireframe(uiSettings.floorWireframe);
        sphere.setColor(uiSettings.sphereColor);
        cubeMaterial->setDiffuse(uiSettings.cubeColor);
        triangleMaterial->setDiffuse(uiSettings.triangleColor);
        if (objMaterial) {
            objMaterial->setDiffuse(uiSettings.objColor);
        }
        
        // Apply material-specific properties from UI
        Material* sphereMat = sphere.getMaterial();
        if (sphereMat) {
            if (uiSettings.sphereMaterialType == 3) {
                sphereMat->setRefractionIor(uiSettings.sphereIor);
            } else {
                sphereMat->setShininess(uiSettings.sphereShininess);
                sphereMat->setSpecular(uiSettings.sphereSpecular);
                sphereMat->setMetallic(uiSettings.sphereMetallic);
                sphereMat->setRoughness(uiSettings.sphereRoughness);
            }
        }
        if (cubeMaterial) {
            if (uiSettings.cubeMaterialType == 3) {
                cubeMaterial->setRefractionIor(uiSettings.cubeIor);
            } else {
                cubeMaterial->setShininess(uiSettings.cubeShininess);
                cubeMaterial->setSpecular(uiSettings.cubeSpecular);
                cubeMaterial->setMetallic(uiSettings.cubeMetallic);
                cubeMaterial->setRoughness(uiSettings.cubeRoughness);
            }
        }
        if (triangleMaterial) {
            if (uiSettings.triangleMaterialType == 3) {
                triangleMaterial->setRefractionIor(uiSettings.triangleIor);
            } else {
                triangleMaterial->setShininess(uiSettings.triangleShininess);
                triangleMaterial->setSpecular(uiSettings.triangleSpecular);
                triangleMaterial->setMetallic(uiSettings.triangleMetallic);
                triangleMaterial->setRoughness(uiSettings.triangleRoughness);
            }
        }
        Material* floorMaterial = floor.getMaterial();
        if (floorMaterial) {
            floorMaterial->setShininess(uiSettings.floorShininess);
            floorMaterial->setSpecular(uiSettings.floorSpecular);
            floorMaterial->setMetallic(uiSettings.floorMetallic);
            floorMaterial->setRoughness(uiSettings.floorRoughness);
        }
        if (objMaterial) {
            if (uiSettings.objMaterialType == 3) {
                objMaterial->setRefractionIor(uiSettings.objIor);
            } else {
                objMaterial->setShininess(uiSettings.objShininess);
                objMaterial->setSpecular(uiSettings.objSpecular);
                objMaterial->setMetallic(uiSettings.objMetallic);
                objMaterial->setRoughness(uiSettings.objRoughness);
            }
        }

        if (uiSettings.floorUseTexture != lastFloorUseTexture) {
            applyTexture(floor.getMaterial(), uiSettings.floorUseTexture, floorTexture);
            lastFloorUseTexture = uiSettings.floorUseTexture;
        }
        if (uiSettings.sphereUseTexture != lastSphereUseTexture) {
            bool allowTexture = uiSettings.sphereMaterialType != 3;
            applyTexture(sphere.getMaterial(), allowTexture && uiSettings.sphereUseTexture, sphereTexture);
            lastSphereUseTexture = uiSettings.sphereUseTexture;
        }
        if (uiSettings.cubeUseTexture != lastCubeUseTexture) {
            bool allowTexture = uiSettings.cubeMaterialType != 3;
            applyTexture(cubeMaterial, allowTexture && uiSettings.cubeUseTexture, cubeTexture);
            lastCubeUseTexture = uiSettings.cubeUseTexture;
        }
        if (uiSettings.triangleUseTexture != lastTriangleUseTexture) {
            bool allowTexture = uiSettings.triangleMaterialType != 3;
            applyTexture(triangleMaterial, allowTexture && uiSettings.triangleUseTexture, triangleTexture);
            lastTriangleUseTexture = uiSettings.triangleUseTexture;
        }
        if (uiSettings.objUseTexture != lastObjUseTexture) {
            bool allowTexture = uiSettings.objMaterialType != 3;
            applyTexture(objMaterial, allowTexture && uiSettings.objUseTexture, objTexture);
            lastObjUseTexture = uiSettings.objUseTexture;
        }

        if (uiSettings.requestObjLoad) {
            uiSettings.requestObjLoad = false;
            std::string path = uiSettings.objPath;
            if (path.empty()) {
                uiSettings.objStatus = "Provide an OBJ path.";
            } else if (!std::filesystem::exists(path)) {
                uiSettings.objStatus = "OBJ not found.";
            } else {
                GeometryFactory::instance().releaseGeometry("obj_mesh");
                objGeometry = GeometryFactory::instance().createOBJ("obj_mesh", path, 1.0f);
                if (!objGeometry) {
                    uiSettings.objStatus = "Failed to load OBJ.";
                } else {
                    delete objMaterial;
                    objMaterial = createMaterialByType(uiSettings.objMaterialType, uiSettings.objColor, uiSettings.objIor);
                    applyTexture(objMaterial, uiSettings.objMaterialType != 3 && uiSettings.objUseTexture, objTexture);
                    glm::mat4 objTransform = glm::translate(glm::mat4(1.0f), uiSettings.objPosition);
                    objTransform = glm::scale(objTransform, glm::vec3(uiSettings.objScale));
                    objMesh = std::make_unique<MeshRenderable>(objGeometry, objMaterial, objTransform);
                    uiSettings.objStatus = "Loaded: " + std::filesystem::path(path).filename().string();
                    lastObjMat = uiSettings.objMaterialType;
                    std::string boundsError;
                    objBoundsValid = loadObjBounds(path, objLocalMin, objLocalMax, boundsError);
                    if (!objBoundsValid) {
                        uiSettings.objStatus += " (bounds failed)";
                    }
                }
            }
        }

        if (uiSettings.requestObjTextureLoad) {
            uiSettings.requestObjTextureLoad = false;
            std::string path = resolveTexturePath(uiSettings.objTexturePath);
            if (path.empty()) {
                uiSettings.objTextureStatus = "Texture path not found.";
            } else {
                std::string error;
                objTexture = loadTexture2D(path, "obj_diffuse", error);
                if (!objTexture) {
                    uiSettings.objTextureStatus = error;
                } else {
                    uiSettings.objTextureStatus = "Loaded: " + std::filesystem::path(path).filename().string();
                    applyTexture(objMaterial, uiSettings.objMaterialType != 3 && uiSettings.objUseTexture, objTexture);
                }
            }
        }

        if (uiSettings.requestFloorTextureLoad) {
            uiSettings.requestFloorTextureLoad = false;
            std::string path = resolveTexturePath(uiSettings.floorTexturePath);
            if (path.empty()) {
                uiSettings.floorTextureStatus = "Texture path not found.";
            } else {
                std::string error;
                floorTexture = loadTexture2D(path, "floor_diffuse", error);
                if (!floorTexture) {
                    uiSettings.floorTextureStatus = error;
                } else {
                    uiSettings.floorTextureStatus = "Loaded: " + std::filesystem::path(path).filename().string();
                    applyTexture(floor.getMaterial(), uiSettings.floorUseTexture, floorTexture);
                }
            }
        }
        if (uiSettings.requestSphereTextureLoad) {
            uiSettings.requestSphereTextureLoad = false;
            std::string path = resolveTexturePath(uiSettings.sphereTexturePath);
            if (path.empty()) {
                uiSettings.sphereTextureStatus = "Texture path not found.";
            } else {
                std::string error;
                sphereTexture = loadTexture2D(path, "sphere_diffuse", error);
                if (!sphereTexture) {
                    uiSettings.sphereTextureStatus = error;
                } else {
                    uiSettings.sphereTextureStatus = "Loaded: " + std::filesystem::path(path).filename().string();
                    applyTexture(sphere.getMaterial(), uiSettings.sphereMaterialType != 3 && uiSettings.sphereUseTexture, sphereTexture);
                }
            }
        }
        if (uiSettings.requestCubeTextureLoad) {
            uiSettings.requestCubeTextureLoad = false;
            std::string path = resolveTexturePath(uiSettings.cubeTexturePath);
            if (path.empty()) {
                uiSettings.cubeTextureStatus = "Texture path not found.";
            } else {
                std::string error;
                cubeTexture = loadTexture2D(path, "cube_diffuse", error);
                if (!cubeTexture) {
                    uiSettings.cubeTextureStatus = error;
                } else {
                    uiSettings.cubeTextureStatus = "Loaded: " + std::filesystem::path(path).filename().string();
                    applyTexture(cubeMaterial, uiSettings.cubeMaterialType != 3 && uiSettings.cubeUseTexture, cubeTexture);
                }
            }
        }
        if (uiSettings.requestTriangleTextureLoad) {
            uiSettings.requestTriangleTextureLoad = false;
            std::string path = resolveTexturePath(uiSettings.triangleTexturePath);
            if (path.empty()) {
                uiSettings.triangleTextureStatus = "Texture path not found.";
            } else {
                std::string error;
                triangleTexture = loadTexture2D(path, "triangle_diffuse", error);
                if (!triangleTexture) {
                    uiSettings.triangleTextureStatus = error;
                } else {
                    uiSettings.triangleTextureStatus = "Loaded: " + std::filesystem::path(path).filename().string();
                    applyTexture(triangleMaterial, uiSettings.triangleMaterialType != 3 && uiSettings.triangleUseTexture, triangleTexture);
                }
            }
        }

        if (objMesh) {
            glm::mat4 objTransform = glm::translate(glm::mat4(1.0f), uiSettings.objPosition);
            objTransform = glm::scale(objTransform, glm::vec3(uiSettings.objScale));
            objMesh->setTransform(objTransform);
        }

        // Update render settings from UI
        renderSettings.shadowEnabled = uiSettings.shadowEnabled;
        renderSettings.shadowBias = uiSettings.shadowBias;
        renderSettings.shadowSoftness = uiSettings.shadowSoftness;
        renderSettings.useCascadedShadows = uiSettings.useCascadedShadows;
        renderSettings.debugCascades = uiSettings.debugCascades;
        renderSettings.cascadeCount = uiSettings.cascadeCount;
        renderSettings.cascadeSplitLambda = uiSettings.cascadeSplitLambda;
        renderSettings.cascadeMaxDistance = uiSettings.cascadeMaxDistance;
        renderSettings.ssaoEnabled = uiSettings.ssaoEnabled;
        renderSettings.ssaoRadius = uiSettings.ssaoRadius;
        renderSettings.ssaoIntensity = uiSettings.ssaoIntensity;
        renderSettings.ssaoBias = uiSettings.ssaoBias;
        renderSettings.lightPosition[0] = uiSettings.lightPosition.x;
        renderSettings.lightPosition[1] = uiSettings.lightPosition.y;
        renderSettings.lightPosition[2] = uiSettings.lightPosition.z;
        renderSettings.lightAmbient[0] = uiSettings.lightAmbient.x;
        renderSettings.lightAmbient[1] = uiSettings.lightAmbient.y;
        renderSettings.lightAmbient[2] = uiSettings.lightAmbient.z;
        renderSettings.lightDiffuse[0] = uiSettings.lightDiffuse.x;
        renderSettings.lightDiffuse[1] = uiSettings.lightDiffuse.y;
        renderSettings.lightDiffuse[2] = uiSettings.lightDiffuse.z;
        renderSettings.lightSpecular[0] = uiSettings.lightSpecular.x;
        renderSettings.lightSpecular[1] = uiSettings.lightSpecular.y;
        renderSettings.lightSpecular[2] = uiSettings.lightSpecular.z;
        renderSettings.lights = uiSettings.extraLights;
        renderSettings.envMapIntensity = uiSettings.envMapIntensity;
        renderSettings.iblEnabled = uiSettings.iblEnabled;
        renderSettings.iblIntensity = uiSettings.iblIntensity;

        // Render frame
        renderer.beginFrame(&camera, t);
        
        if (uiSettings.showFloor) renderer.submit(&floor);
        if (uiSettings.showSphere) renderer.submit(&sphere);
        if (uiSettings.showCube) {
            if (uiSettings.cubeWireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            renderer.submit(&cube);
            if (uiSettings.cubeWireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
        if (uiSettings.showTriangle) {
            if (uiSettings.triangleWireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            renderer.submit(&triangle);
            if (uiSettings.triangleWireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
        if (uiSettings.showObj && objMesh) {
            if (uiSettings.objWireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            renderer.submit(objMesh.get());
            if (uiSettings.objWireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
        
        renderer.renderFrame(renderSettings);
        renderer.endFrame();

        // Render UI
        renderUI(uiSettings, renderer, io.Framerate);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    delete cubeMaterial;
    delete triangleMaterial;
    delete objMaterial;
    ibl.cleanup();
    if (envMapTex != 0) {
        glDeleteTextures(1, &envMapTex);
    }
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
