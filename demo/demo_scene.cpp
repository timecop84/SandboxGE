#include <GraphicsEngine.h>
#include <RenderSettings.h>
#include <ShaderPathResolver.h>
#include <SSAORenderer.h>
#include <ShadowRenderer.h>
#include <Floor.h>
#include <SphereObstacle.h>
#include <Camera.h>
#include <Colour.h>
#include <TransformStack.h>
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
#include <cstdio>
#include <vector>
#include <cmath>

namespace {

struct MeshBuffers {
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<uint32_t> indices;
    glm::vec3 color{0.8f};
};

MeshBuffers makeCubeBase() {
    MeshBuffers cube{};
    cube.positions = {
        // Front
        -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,
        // Back
         0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,
        // Left
        -0.5f, -0.5f, -0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f,
        // Right
         0.5f, -0.5f,  0.5f,  0.5f, -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,
        // Top
        -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f,
        // Bottom
        -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f};

    cube.normals = {
        // Front
         0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,
        // Back
         0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,
        // Left
        -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f,
        // Right
         1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,
        // Top
         0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,
        // Bottom
         0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f};

    cube.indices = {
         0,  1,  2,  0,  2,  3,   // Front
         4,  5,  6,  4,  6,  7,   // Back
         8,  9, 10,  8, 10, 11,   // Left
        12, 13, 14, 12, 14, 15,   // Right
        16, 17, 18, 16, 18, 19,   // Top
        20, 21, 22, 20, 22, 23};  // Bottom

    return cube;
}

MeshBuffers makeTriangleBase() {
    MeshBuffers tri{};
    constexpr float h = 0.8660254f; // sqrt(3) / 2
    tri.positions = {
        0.0f, 0.0f, 1.0f,
       -h,   0.0f, -0.5f,
        h,   0.0f, -0.5f};
    tri.normals = {
        0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f};
    tri.indices = {0, 1, 2};
    return tri;
}

void bakeMesh(const MeshBuffers& base, const glm::mat4& model, MeshBuffers& baked) {
    glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(model)));
    baked.positions.resize(base.positions.size());
    baked.normals.resize(base.normals.size());
    baked.indices = base.indices;
    baked.color = base.color;

    for (size_t i = 0; i < base.positions.size(); i += 3) {
        glm::vec4 pos(base.positions[i + 0], base.positions[i + 1], base.positions[i + 2], 1.0f);
        glm::vec3 normal(base.normals[i + 0], base.normals[i + 1], base.normals[i + 2]);

        glm::vec4 worldPos = model * pos;
        glm::vec3 worldNormal = glm::normalize(normalMat * normal);

        baked.positions[i + 0] = worldPos.x;
        baked.positions[i + 1] = worldPos.y;
        baked.positions[i + 2] = worldPos.z;
        baked.normals[i + 0] = worldNormal.x;
        baked.normals[i + 1] = worldNormal.y;
        baked.normals[i + 2] = worldNormal.z;
    }
}

gfx::MeshSource toMeshSource(const MeshBuffers& mesh) {
    gfx::MeshSource src{};
    src.positions = mesh.positions.data();
    src.normals = mesh.normals.data();
    src.indices = mesh.indices.data();
    src.vertexCount = static_cast<int>(mesh.positions.size() / 3);
    src.indexCount = static_cast<int>(mesh.indices.size());
    src.color = mesh.color;
    return src;
}

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

void renderLightingUI(gfx::RenderSettings& settings) {
    if (ImGui::CollapsingHeader("Visual Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Sphere Visibility", &settings.sphereVisibility);
        ImGui::Checkbox("Floor Visibility", &settings.floorVisibility);
        ImGui::Checkbox("Custom Mesh Visibility", &settings.customMeshVisibility);
        if (settings.customMeshVisibility) {
            ImGui::Checkbox("Custom Mesh Wireframe", &settings.customMeshWireframe);
        }
        ImGui::Separator();
        ImGui::Text("Shadows");
        ImGui::Checkbox("Enable Shadows", &settings.shadowEnabled);
        if (settings.shadowEnabled) {
            ImGui::SliderFloat("Shadow Bias", &settings.shadowBias, 0.001f, 0.02f, "%.4f");
            ImGui::SliderFloat("Shadow Softness", &settings.shadowSoftness, 1.0f, 4.0f, "%.0f");
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
        ImGui::SliderFloat3("Light XYZ", settings.lightPosition, -200.0f, 200.0f, "%.1f");
        ImGui::Separator();
        ImGui::Text("Light Colors");
        ImGui::ColorEdit3("Ambient", settings.lightAmbient);
        ImGui::ColorEdit3("Diffuse", settings.lightDiffuse);
        ImGui::ColorEdit3("Specular", settings.lightSpecular);
        ImGui::Separator();
        ImGui::Text("Additional Lights");
        if (ImGui::Button("+ Add Light")) {
            gfx::ExtraLight extra;
            extra.enabled = true;
            extra.position[0] = -30.0f + static_cast<float>(settings.lights.size()) * 20.0f;
            extra.position[1] = 60.0f;
            extra.position[2] = 30.0f;
            extra.diffuse[0] = 1.0f;
            extra.diffuse[1] = 1.0f;
            extra.diffuse[2] = 1.0f;
            extra.intensity = 1.0f;
            settings.lights.push_back(extra);
        }
        int removeIndex = -1;
        for (size_t i = 0; i < settings.lights.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            auto& light = settings.lights[i];
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
            settings.lights.erase(settings.lights.begin() + removeIndex);
        }
    }
}

void renderHUD(float fps) {
    const float infoWidth = 260.0f;
    const float padding = 10.0f;
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - infoWidth - padding, padding), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(infoWidth, 120.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.75f);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;
    ImGui::Begin("InfoHUD", nullptr, flags);
    ImGui::Text("FPS: %.1f", fps);
    ImGui::Separator();
    ImGui::Text("Navigation");
    ImGui::Text("WASD/QE  - Move");
    ImGui::Text("RMB drag - Look");
    ImGui::Text("Shift    - Fast move");
    ImGui::Text("Esc      - Quit");
    ImGui::End();
}

} // namespace

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    const int initialWidth = 1280;
    const int initialHeight = 720;
    GLFWwindow* window = glfwCreateWindow(initialWidth, initialHeight, "SandboxGE Demo Scene", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGL(glfwGetProcAddress)) {
        std::cerr << "Failed to load OpenGL functions via GLAD\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    // ImGui setup
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");

    int fbWidth = 0;
    int fbHeight = 0;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    fbWidth = std::max(1, fbWidth);
    fbHeight = std::max(1, fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    gfx::Engine engine;
    engine.initialize(fbWidth, fbHeight);
    ShaderPath::setRoot(findShaderRoot());

    Floor floor(80.0f, 80.0f, 1, 1, Vector(0.0f, 0.0f, 0.0f));
    floor.setColor(Colour(0.35f, 0.35f, 0.38f));

    SphereObstacle sphere;
    sphere.m_obstRadius = 4.0f;
    sphere.setPosition(Vector(10.0f, sphere.m_obstRadius, -4.0f));
    sphere.m_colour.set(0.8f, 0.45f, 0.45f, 1.0f);

    Camera camera(glm::vec3(30.0f, 22.0f, 38.0f),
                  glm::vec3(0.0f, 6.0f, 0.0f),
                  glm::vec3(0.0f, 1.0f, 0.0f),
                  Camera::PERSPECTIVE);
    camera.setShape(55.0f, static_cast<float>(fbWidth) / static_cast<float>(fbHeight), 0.1f, 250.0f);
    FlyCamera camState;
    camState.position = glm::vec3(30.0f, 22.0f, 38.0f);

    TransformStack transformStack;
    Renderer::ClothRenderData clothData{};
    std::vector<glm::vec3> primaryColors;

    gfx::RenderSettings settings;
    settings.useSilkShader = false;
    settings.clothVisibility = false;
    settings.pointVisibility = false;
    settings.customMeshVisibility = true;
    settings.lightPosition[0] = 35.0f;
    settings.lightPosition[1] = 60.0f;
    settings.lightPosition[2] = 35.0f;
    settings.lightDiffuse[0] = 1.1f;
    settings.lightDiffuse[1] = 1.0f;
    settings.lightDiffuse[2] = 0.95f;

    MeshBuffers cubeBase = makeCubeBase();
    cubeBase.color = glm::vec3(0.3f, 0.7f, 1.0f);
    MeshBuffers triBase = makeTriangleBase();
    triBase.color = glm::vec3(1.0f, 0.6f, 0.2f);

    MeshBuffers cubeMesh;
    MeshBuffers triMesh;

    float orbitRadius = 12.0f;
    float orbitHeight = 6.0f;
    float lastTime = static_cast<float>(glfwGetTime());

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        int newWidth = 0;
        int newHeight = 0;
        glfwGetFramebufferSize(window, &newWidth, &newHeight);
        newWidth = std::max(1, newWidth);
        newHeight = std::max(1, newHeight);
        if (newWidth != fbWidth || newHeight != fbHeight) {
            fbWidth = newWidth;
            fbHeight = newHeight;
            glViewport(0, 0, fbWidth, fbHeight);
            camera.setShape(55.0f, static_cast<float>(fbWidth) / static_cast<float>(fbHeight), 0.1f, 250.0f);
            engine.resize(fbWidth, fbHeight);
        }

        float t = static_cast<float>(glfwGetTime());
        float dt = t - lastTime;
        lastTime = t;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiIO& io = ImGui::GetIO();
        bool uiWantsMouse = io.WantCaptureMouse;
        bool uiWantsKeyboard = io.WantCaptureKeyboard;

        handleMouseLook(window, camState, uiWantsMouse);
        handleCameraInput(window, camState, dt, uiWantsMouse || uiWantsKeyboard);
        syncCamera(camera, camState, fbWidth, fbHeight);

        glm::mat4 cubeModel = glm::translate(glm::mat4(1.0f), glm::vec3(-10.0f, 3.0f, 0.0f));
        cubeModel = glm::rotate(cubeModel, 0.35f * t, glm::vec3(0.0f, 1.0f, 0.0f));
        cubeModel = glm::scale(cubeModel, glm::vec3(6.0f, 4.0f, 6.0f));
        bakeMesh(cubeBase, cubeModel, cubeMesh);

        glm::vec3 orbitPos = glm::vec3(std::cos(t) * orbitRadius, orbitHeight, std::sin(t) * orbitRadius);
        glm::mat4 triModel = glm::translate(glm::mat4(1.0f), orbitPos);
        triModel = glm::rotate(triModel, t * 2.0f, glm::vec3(0.0f, 1.0f, 0.0f));
        triModel = glm::scale(triModel, glm::vec3(4.0f));
        bakeMesh(triBase, triModel, triMesh);

        std::vector<gfx::MeshSource> meshes;
        meshes.reserve(2);
        meshes.push_back(toMeshSource(cubeMesh));
        meshes.push_back(toMeshSource(triMesh));
        engine.syncMeshes(meshes);

        // Keep renderer toggles in sync
        Shadow::setEnabled(settings.shadowEnabled);
        Shadow::setBias(settings.shadowBias);
        Shadow::setSoftness(settings.shadowSoftness);
        SSAO::setEnabled(settings.ssaoEnabled);
        if (settings.ssaoEnabled) {
            SSAO::setRadius(settings.ssaoRadius);
            SSAO::setIntensity(settings.ssaoIntensity);
            SSAO::setBias(settings.ssaoBias);
        }

        engine.renderScene(&camera, &floor, &sphere, clothData, primaryColors, settings, transformStack);

        ImGui::Begin("Rendering");
        renderLightingUI(settings);
        ImGui::End();
        renderHUD(io.Framerate);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    engine.cleanup(clothData);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
