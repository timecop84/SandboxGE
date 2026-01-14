cmake_minimum_required(VERSION 3.24)

project(SandboxGE LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Auto-build GLFW if available in sibling cloth_solver project
set(_GLFW_SOURCE_DIR "")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../cloth_solver/external/glfw/CMakeLists.txt")
    set(_GLFW_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../cloth_solver/external/glfw")
endif()
# Only add GLFW subdirectory if the target doesn't already exist
if(_GLFW_SOURCE_DIR AND NOT TARGET glfw)
    # Build GLFW from source
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory("${_GLFW_SOURCE_DIR}" glfw)
endif()

if(NOT DEFINED SANDBOX_GE_GLFW_TARGET)
    set(SANDBOX_GE_GLFW_TARGET "" CACHE STRING "Existing GLFW target to link against (leave empty to auto-detect).")
endif()
if(NOT DEFINED SANDBOX_GE_GLFW_LIB)
    set(SANDBOX_GE_GLFW_LIB "" CACHE FILEPATH "Optional fallback: path to glfw3 library if no target is available.")
endif()
if(NOT DEFINED SANDBOX_GE_GLFW_INCLUDE_DIR)
    set(SANDBOX_GE_GLFW_INCLUDE_DIR "" CACHE PATH "Optional fallback: include directory for GLFW headers when using SANDBOX_GE_GLFW_LIB.")
endif()
if(NOT DEFINED SANDBOX_GE_EXTRA_INCLUDE_DIRS)
    set(SANDBOX_GE_EXTRA_INCLUDE_DIRS "" CACHE STRING "Optional include paths for external math/helpers (e.g., Vector/Matrix).")
endif()
set(SANDBOX_GE_GLAD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/glad)

set(SANDBOX_GE_SOURCES
    # Core infrastructure
    src/core/ResourceManager.cpp
    
    # Rendering system
    src/rendering/UBOManager.cpp
    src/rendering/MeshRenderable.cpp
    src/rendering/InstancedRenderable.cpp
    src/rendering/RenderQueue.cpp
    src/rendering/InstanceBatcher.cpp
    src/rendering/UnifiedRenderer.cpp
    src/Renderer.cpp
    src/SSAORenderer.cpp
    src/ShadowRenderer.cpp
    
    # Materials
    src/materials/Material.cpp
    src/materials/ShaderHotReloader.cpp
    src/ShaderLib_GLAD.cpp
    
    # Renderables (new IRenderable implementations)
    src/renderables/FloorRenderable.cpp
    src/renderables/SphereRenderable.cpp
    
    # Core components
    src/Camera.cpp
    src/Light.cpp
    src/Colour.cpp
    src/TransformStack.cpp
    
    # Utilities
    src/ShaderPathResolver.cpp
    src/LightingHelper.cpp
    src/GeometryFactory.cpp
    
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
    # First check if we just built GLFW above
    if(TARGET glfw)
        set(_SANDBOX_GE_GLFW_TARGET glfw)
    elseif(TARGET glfw3)
        set(_SANDBOX_GE_GLFW_TARGET glfw3)
    endif()
    # Fall back to imported target checks
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
endif()

if(NOT _SANDBOX_GE_GLFW_TARGET AND SANDBOX_GE_GLFW_LIB)
    if(NOT EXISTS "${SANDBOX_GE_GLFW_LIB}")
        message(FATAL_ERROR "SANDBOX_GE_GLFW_LIB is set but does not exist: ${SANDBOX_GE_GLFW_LIB}")
    endif()
    add_library(SandboxGE_GLFW_FALLBACK INTERFACE)
    target_link_libraries(SandboxGE_GLFW_FALLBACK INTERFACE ${SANDBOX_GE_GLFW_LIB})
    if(SANDBOX_GE_GLFW_INCLUDE_DIR)
        target_include_directories(SandboxGE_GLFW_FALLBACK INTERFACE ${SANDBOX_GE_GLFW_INCLUDE_DIR})
    endif()
    set(_SANDBOX_GE_GLFW_TARGET SandboxGE_GLFW_FALLBACK)
endif()

if(NOT _SANDBOX_GE_GLFW_TARGET)
    message(FATAL_ERROR "GLFW target not found. Set SANDBOX_GE_GLFW_TARGET to an existing target, "
                        "or provide SANDBOX_GE_GLFW_LIB (+ SANDBOX_GE_GLFW_INCLUDE_DIR) to link a prebuilt glfw3.")
endif()

if(WIN32)
    target_link_libraries(SandboxGE PUBLIC ${_SANDBOX_GE_GLFW_TARGET} opengl32)
else()
    target_link_libraries(SandboxGE PUBLIC ${_SANDBOX_GE_GLFW_TARGET} GL)
endif()

# ===== Test New API =====
add_executable(TestNewAPI demo/test_new_api.cpp)
target_include_directories(TestNewAPI PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glad/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glm
)
target_link_libraries(TestNewAPI PRIVATE SandboxGE)
target_compile_definitions(TestNewAPI PRIVATE _USE_MATH_DEFINES)

add_custom_command(TARGET TestNewAPI POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    $<TARGET_FILE_DIR:TestNewAPI>/shaders
)

option(SANDBOX_GE_BUILD_DEMO "Build SandboxGE demo scene" OFF)
if(SANDBOX_GE_BUILD_DEMO)
    if(NOT SANDBOX_GE_EXTRA_INCLUDE_DIRS)
        message(FATAL_ERROR "Demo requires SANDBOX_GE_EXTRA_INCLUDE_DIRS (for Vector/Matrix types).")
    endif()

    if(NOT DEFINED SANDBOX_GE_IMGUI_DIR)
        set(SANDBOX_GE_IMGUI_DIR "" CACHE PATH "Path to Dear ImGui root (folder containing imgui.h).")
    endif()

    set(_SANDBOX_GE_IMGUI_DIR ${SANDBOX_GE_IMGUI_DIR})
    if(NOT _SANDBOX_GE_IMGUI_DIR)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui.h")
            set(_SANDBOX_GE_IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui")
        elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../cloth_solver/external/imgui/imgui.h")
            set(_SANDBOX_GE_IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../cloth_solver/external/imgui")
        endif()
    endif()
    if(NOT _SANDBOX_GE_IMGUI_DIR OR NOT EXISTS "${_SANDBOX_GE_IMGUI_DIR}/imgui.h")
        message(FATAL_ERROR "Demo requires Dear ImGui. Set SANDBOX_GE_IMGUI_DIR to a folder containing imgui.h (e.g. ../cloth_solver/external/imgui).")
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

    add_custom_command(TARGET SandboxGE_Demo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:SandboxGE_Demo>/shaders
    )
    
    # Unified Renderer Demo (new architecture)
    add_executable(UnifiedDemo demo/demo_unified.cpp ${_SANDBOX_GE_IMGUI_SOURCES})
    target_include_directories(UnifiedDemo PRIVATE ${_SANDBOX_GE_IMGUI_DIR} ${_SANDBOX_GE_IMGUI_DIR}/backends)
    target_link_libraries(UnifiedDemo PRIVATE SandboxGE)
    target_compile_definitions(UnifiedDemo PRIVATE _USE_MATH_DEFINES)
    
    if(NOT WIN32)
        target_compile_definitions(UnifiedDemo PRIVATE IMGUI_IMPL_OPENGL_ES3)
    endif()
    
    add_custom_command(TARGET UnifiedDemo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:UnifiedDemo>/shaders
    )
endif()
