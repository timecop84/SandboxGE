
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include "rendering/UnifiedRenderer.h"
#include "materials/Material.h"
#include "rendering/MeshRenderable.h"
#include "renderables/FloorRenderable.h"
#include "renderables/SphereRenderable.h"
#include "core/ResourceManager.h"
#include <core/Camera.h>
#include <utils/GeometryFactory.h>
#include <utils/ShaderLib.h>
#include <utils/ShaderPathResolver.h>
#include <iostream>
#include <filesystem>

using namespace sandbox;

// Find shader directory
std::string findShaderRoot() {
    std::filesystem::path exe = std::filesystem::current_path();
    
    // Check relative to exe
    if (std::filesystem::exists(exe / "shaders" / "Phong.vs")) {
        return (exe / "shaders").string();
    }
    
    // Check parent directories
    auto parent = exe.parent_path();
    for (int i = 0; i < 3; ++i) {
        if (std::filesystem::exists(parent / "shaders" / "Phong.vs")) {
            return (parent / "shaders").string();
        }
        parent = parent.parent_path();
    }
    
    return "shaders";
}

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Test New API", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    // Initialize GLAD
    if (!gladLoadGL(glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return -1;
    }

    std::cout << "OpenGL " << glGetString(GL_VERSION) << "\n";
    
    // Initialize shader path
    std::string shaderRoot = findShaderRoot();
    ShaderPath::setRoot(shaderRoot);
    std::cout << "Shader root: " << shaderRoot << "\n";
    
    // Pre-load Phong shader (will trigger auto-creation)
    ShaderLib* shaderLib = ShaderLib::instance();
    auto* phongShader = (*shaderLib)["Phong"];
    if (!phongShader) {
        std::cerr << "Failed to create Phong shader\n";
        return -1;
    }
    std::cout << "Phong shader loaded successfully\n";
    
    // Load PhongUBO shader
    auto* phongUBOShader = (*shaderLib)["PhongUBO"];
    if (!phongUBOShader) {
        std::cerr << "Failed to create PhongUBO shader\n";
        return -1;
    }
    std::cout << "PhongUBO shader loaded successfully\n";
    
    // Load SilkUBO shader
    auto* silkUBOShader = (*shaderLib)["SilkUBO"];
    if (!silkUBOShader) {
        std::cerr << "Failed to create SilkUBO shader\n";
        return -1;
    }
    std::cout << "SilkUBO shader loaded successfully\n";
    
    // Load SilkPBR_UBO shader
    auto* silkPBR_UBOShader = (*shaderLib)["SilkPBR_UBO"];
    if (!silkPBR_UBOShader) {
        std::cerr << "Failed to create SilkPBR_UBO shader\n";
        return -1;
    }
    std::cout << "SilkPBR_UBO shader loaded successfully\n";

    // Create camera
    Camera camera(
        glm::vec3(0, 5, 10),    // eye position
        glm::vec3(0, 0, 0),      // look at center
        glm::vec3(0, 1, 0),      // up vector
        Camera::ProjectionType::PERSPECTIVE
    );
    camera.setPerspective(45.0f, 800.0f/600.0f, 0.1f, 100.0f);

    // Initialize renderer
    UnifiedRenderer renderer;
    if (!renderer.initialize(800, 600)) {
        std::cerr << "Failed to initialize renderer\n";
        return -1;
    }

    // Create a cube geometry
    auto cubeGeom = FlockingGraphics::GeometryFactory::instance().createCube(2.0f);
    
    // Create materials
    auto* cubeMaterial = Material::createPhong(glm::vec3(1.0f, 0.2f, 0.2f));  // red Phong
    auto* silkMaterial = Material::createSilk(glm::vec3(0.2f, 0.7f, 1.0f));   // blue Silk
    auto* pbrMaterial = Material::createSilkPBR(glm::vec3(0.8f, 0.45f, 0.45f)); // pink PBR
    
    // Create renderables
    MeshRenderable cube(cubeGeom, cubeMaterial, glm::mat4(1.0f));
    
    // Create floor
    FloorRenderable floor(40.0f, 40.0f, glm::vec3(0.0f, -3.0f, 0.0f), glm::vec3(0.35f, 0.35f, 0.38f));
    
    // Create spheres with different materials
    SphereRenderable sphere1(1.5f, glm::vec3(-4.0f, 0.0f, 0.0f), glm::vec3(0.2f, 0.7f, 1.0f));
    sphere1.setMaterial(silkMaterial);  // Use Silk shader
    
    SphereRenderable sphere2(1.0f, glm::vec3(4.0f, 0.0f, -2.0f), glm::vec3(0.8f, 0.45f, 0.45f));
    sphere2.setMaterial(pbrMaterial);  // Use PBR shader

    // Render settings
    RenderSettings settings;
    settings.useSilkShader = false;  // Use basic Phong
    settings.clothVisibility = true;

    float time = 0.0f;

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        time += 0.016f; // ~60fps

        // Clear
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        
        // Check OpenGL errors before render
        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            std::cerr << "OpenGL error before render: " << err << "\n";
        }

        // Rotate cube
        glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), time, glm::vec3(0, 1, 0));
        cube.setTransform(rotation);

        // Render frame
        renderer.beginFrame(&camera, time);
        renderer.submit(&floor);
        renderer.submit(&sphere1);
        renderer.submit(&sphere2);
        renderer.submit(&cube);
        renderer.renderFrame(settings);
        renderer.endFrame();
        
        // Check OpenGL errors after render
        err = glGetError();
        if (err != GL_NO_ERROR) {
            std::cerr << "OpenGL error after render: " << err << "\n";
        }

        // Stats
        auto stats = renderer.getStats();
        if ((int)time % 60 == 0) {
            std::cout << "Draw calls: " << stats.drawCalls 
                      << " | Instances: " << stats.instances << "\n";
        }

        glfwSwapBuffers(window);
        glfwPollEvents();

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            break;
        }
    }

    renderer.cleanup();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
