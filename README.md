# SandboxGE

Standalone home for the OpenGL renderer that ships with the cloth sandbox. It bundles the renderer sources (`include/`, `src/`), shaders, and vendored `glad`/`glm` so it can be built and iterated on outside of the main app.

## Build

```
cmake -S . -B build
cmake --build build
```

- Target name: `SandboxGE`
- Depends on an existing GLFW target. If your build already defines `glfw`/`glfw3`/`glfw3::glfw`, it is picked up automatically; otherwise set `-DSANDBOX_GE_GLFW_TARGET=<target>` or make `glfw3` discoverable via `find_package`.
- OpenGL is linked via `opengl32` (Windows). Adjust if you target another platform.

## Cloth integration

The renderer still supports cloth rendering helpers. Point `SANDBOX_GE_CLOTH_INCLUDE_DIRS` at your ClothAPI headers (e.g. `${CMAKE_SOURCE_DIR}/include;${CMAKE_SOURCE_DIR}/engine;${CMAKE_SOURCE_DIR}/engine/core;${CMAKE_SOURCE_DIR}/modules/math/include`) before calling `add_subdirectory`.

## Assets

Copy `shaders/` next to your executable or configure your runtime to point `ShaderPathResolver` at this folder.
