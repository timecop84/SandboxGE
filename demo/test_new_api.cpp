/**
 * @file test_new_api.cpp
 * @brief Minimal test of the new UnifiedRenderer API
 */

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include "rendering/UnifiedRenderer.h"
#include "materials/Material.h"
#include "rendering/MeshRenderable.h"
#include "core/ResourceManager.h"
#include <Camera.h>
#include <GeometryFactory.h>
#include <iostream>

using namespace gfx;

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
    
    // Create a material
    auto* material = Material::createPhong(
        glm::vec3(1.0f, 0.2f, 0.2f)  // red diffuse color
    );

    // Create renderable
    MeshRenderable cube(cubeGeom, material, glm::mat4(1.0f));

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

        // Rotate cube
        glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), time, glm::vec3(0, 1, 0));
        cube.setTransform(rotation);

        // Render frame
        renderer.beginFrame(&camera, time);
        renderer.submit(&cube);
        renderer.renderFrame(settings);
        renderer.endFrame();

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
