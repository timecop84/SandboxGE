cmake_minimum_required(VERSION 3.24)

project(SandboxGE LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED SANDBOX_GE_GLFW_TARGET)
    set(SANDBOX_GE_GLFW_TARGET "" CACHE STRING "Existing GLFW target to link against (leave empty to auto-detect).")
endif()
if(NOT DEFINED SANDBOX_GE_EXTRA_INCLUDE_DIRS)
    set(SANDBOX_GE_EXTRA_INCLUDE_DIRS "" CACHE STRING "Optional include paths for external math/helpers (e.g., Vector/Matrix).")
endif()
set(SANDBOX_GE_GLAD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/glad)

set(SANDBOX_GE_SOURCES
    src/Renderer.cpp
    src/Floor.cpp
    src/SphereObstacle.cpp
    src/GraphicsEngine.cpp
    src/SSAORenderer.cpp
    src/ShadowRenderer.cpp
    src/ShaderPathResolver.cpp
    src/LightingHelper.cpp
    src/Camera.cpp
    src/Light.cpp
    src/Material.cpp
    src/Colour.cpp
    src/ShaderLib_GLAD.cpp
    src/GeometryFactory.cpp
    src/TransformStack.cpp
    ${SANDBOX_GE_GLAD_DIR}/src/gl.c
)

add_library(SandboxGE STATIC ${SANDBOX_GE_SOURCES})

target_include_directories(SandboxGE
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${SANDBOX_GE_GLAD_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/external/glm
)
if(SANDBOX_GE_EXTRA_INCLUDE_DIRS)
    target_include_directories(SandboxGE PUBLIC ${SANDBOX_GE_EXTRA_INCLUDE_DIRS})
endif()

set(_SANDBOX_GE_GLFW_TARGET ${SANDBOX_GE_GLFW_TARGET})
if(NOT _SANDBOX_GE_GLFW_TARGET)
    if(TARGET glfw)
        set(_SANDBOX_GE_GLFW_TARGET glfw)
    elseif(TARGET glfw3)
        set(_SANDBOX_GE_GLFW_TARGET glfw3)
    elseif(TARGET glfw3::glfw)
        set(_SANDBOX_GE_GLFW_TARGET glfw3::glfw)
    else()
        find_package(glfw3 QUIET CONFIG)
        if(TARGET glfw)
            set(_SANDBOX_GE_GLFW_TARGET glfw)
        elseif(TARGET glfw3::glfw)
            set(_SANDBOX_GE_GLFW_TARGET glfw3::glfw)
        endif()
    endif()
endif()

if(NOT _SANDBOX_GE_GLFW_TARGET)
    message(FATAL_ERROR "GLFW target not found. Set SANDBOX_GE_GLFW_TARGET to an existing target or make glfw3 available.")
endif()

target_link_libraries(SandboxGE PUBLIC ${_SANDBOX_GE_GLFW_TARGET} opengl32)
