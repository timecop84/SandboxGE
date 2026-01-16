#include "rendering/UnifiedRenderer.h"
#include "rendering/MeshRenderable.h"
#include "renderables/FloorRenderable.h"
#include "renderables/SphereRenderable.h"
#include "materials/Material.h"
#include "core/RenderSettings.h"
#include "core/Camera.h"
#include "utils/ShaderPathResolver.h"
#include "utils/ShaderLib.h"
#include "utils/GeometryFactory.h"
#include "EnvMapLoader.h"
#include "IBL.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <vector>
#include <memory>
#include <cmath>

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

    int rightHeld = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT);
    if (rightHeld == GLFW_PRESS) {
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

struct UISettings {
    bool showFloor = true;
    bool showSphere = true;
    bool showCube = true;
    bool showTriangle = true;
    bool cubeWireframe = false;
    bool triangleWireframe = false;
    
    int sphereMaterialType = 1; // 0=Phong, 1=Silk, 2=PBR
    int cubeMaterialType = 0;
    float sphereIor = 1.52f;
    float cubeIor = 1.52f;
    
    glm::vec3 sphereColor{0.8f, 0.45f, 0.45f};
    glm::vec3 cubeColor{0.3f, 0.7f, 1.0f};
    glm::vec3 triangleColor{1.0f, 0.6f, 0.2f};
    
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
    ImGui::Text("RMB drag - Look");
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
    float lastTime = static_cast<float>(glfwGetTime());
    float orbitRadius = 12.0f;
    float orbitHeight = 6.0f;

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

        // Camera controls
        handleMouseLook(window, camState, uiWantsMouse);
        handleCameraInput(window, camState, dt, uiWantsMouse || uiWantsKeyboard);
        syncCamera(camera, camState, fbWidth, fbHeight);

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
        static int lastSphereMat = -1;
        static int lastCubeMat = -1;
        if (uiSettings.sphereMaterialType != lastSphereMat) {
            Material* newMat = createMaterialByType(uiSettings.sphereMaterialType, uiSettings.sphereColor, uiSettings.sphereIor);
            sphere.setMaterial(newMat);
            lastSphereMat = uiSettings.sphereMaterialType;
        }
        if (uiSettings.cubeMaterialType != lastCubeMat) {
            delete cubeMaterial;
            cubeMaterial = createMaterialByType(uiSettings.cubeMaterialType, uiSettings.cubeColor, uiSettings.cubeIor);
            cube.setMaterial(cubeMaterial);
            lastCubeMat = uiSettings.cubeMaterialType;
        }

        // Update colors
        sphere.setColor(uiSettings.sphereColor);
        cubeMaterial->setDiffuse(uiSettings.cubeColor);
        triangleMaterial->setDiffuse(uiSettings.triangleColor);
        
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
