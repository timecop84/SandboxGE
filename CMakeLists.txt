cmake_minimum_required(VERSION 3.24)

project(SandboxGE LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build bundled GLFW only if the target doesn't already exist (e.g. parent project added it first)
if(NOT TARGET glfw)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/glfw" glfw)
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

set(_SANDBOX_GE_GLFW_TARGET "")
if(TARGET glfw)
    set(_SANDBOX_GE_GLFW_TARGET glfw)
elseif(TARGET glfw3)
    set(_SANDBOX_GE_GLFW_TARGET glfw3)
elseif(TARGET glfw3::glfw)
    set(_SANDBOX_GE_GLFW_TARGET glfw3::glfw)
endif()

if(NOT _SANDBOX_GE_GLFW_TARGET)
    message(FATAL_ERROR "GLFW target not found. Ensure the 'external/glfw' submodule is initialized: "
                        "git submodule update --init --recursive")
endif()

if(WIN32)
    target_link_libraries(SandboxGE PUBLIC ${_SANDBOX_GE_GLFW_TARGET} opengl32)
else()
    target_link_libraries(SandboxGE PUBLIC ${_SANDBOX_GE_GLFW_TARGET} GL)
endif()

option(SANDBOX_GE_BUILD_DEMO "Build SandboxGE demo scene" OFF)
if(SANDBOX_GE_BUILD_DEMO)
    if(NOT DEFINED SANDBOX_GE_IMGUI_DIR)
        set(SANDBOX_GE_IMGUI_DIR "" CACHE PATH "Path to Dear ImGui root (folder containing imgui.h).")
    endif()

    set(_SANDBOX_GE_IMGUI_DIR ${SANDBOX_GE_IMGUI_DIR})
    if(NOT _SANDBOX_GE_IMGUI_DIR)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui.h")
            set(_SANDBOX_GE_IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui")
        endif()
    endif()
    if(NOT _SANDBOX_GE_IMGUI_DIR OR NOT EXISTS "${_SANDBOX_GE_IMGUI_DIR}/imgui.h")
        message(FATAL_ERROR "Demo requires Dear ImGui. Add it as a submodule at external/imgui, "
                            "or set SANDBOX_GE_IMGUI_DIR to a folder containing imgui.h.")
    endif()

    set(_SANDBOX_GE_IMGUI_SOURCES
        ${_SANDBOX_GE_IMGUI_DIR}/imgui.cpp
        ${_SANDBOX_GE_IMGUI_DIR}/imgui_draw.cpp
        ${_SANDBOX_GE_IMGUI_DIR}/imgui_tables.cpp
        ${_SANDBOX_GE_IMGUI_DIR}/imgui_widgets.cpp
        ${_SANDBOX_GE_IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        ${_SANDBOX_GE_IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    )

    add_executable(SandboxGE_Demo demo/demo_scene.cpp ${_SANDBOX_GE_IMGUI_SOURCES})
    target_include_directories(SandboxGE_Demo PRIVATE ${_SANDBOX_GE_IMGUI_DIR} ${_SANDBOX_GE_IMGUI_DIR}/backends)
    target_link_libraries(SandboxGE_Demo PRIVATE SandboxGE)
    target_compile_definitions(SandboxGE_Demo PRIVATE _USE_MATH_DEFINES)
    
    # Force ImGui to use GLSL 4.20 for WSL compatibility
    if(NOT WIN32)
        target_compile_definitions(SandboxGE_Demo PRIVATE IMGUI_IMPL_OPENGL_ES3)
    endif()

    add_custom_command(TARGET SandboxGE_Demo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:SandboxGE_Demo>/shaders
    )
endif()
