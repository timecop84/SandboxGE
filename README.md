# SandboxGE

Standalone home for the OpenGL renderer that ships with the cloth sandbox. It bundles the renderer sources (`include/`, `src/`), shaders, and vendored `glad`/`glm` so it can be built and iterated on outside of the main app.

## Build

```
cmake -S . -B build
cmake --build build
```

- Target name: `SandboxGE`
- Depends on an existing GLFW target. If your build already defines `glfw`/`glfw3`/`glfw3::glfw`, it is picked up automatically; otherwise set `-DSANDBOX_GE_GLFW_TARGET=<target>` or make `glfw3` discoverable via `find_package`.
- If your project provides math helpers like `Vector`/`Matrix`, point `SANDBOX_GE_EXTRA_INCLUDE_DIRS` at those headers when calling `add_subdirectory`.
- OpenGL is linked via `opengl32` (Windows). Adjust if you target another platform.

## Demo scene

Enable the built-in demo to get a quick test scene (floor + cube + sphere + an orbiting triangle) rendered with SandboxGE:

```bash
cmake -S . -B build -DSANDBOX_GE_BUILD_DEMO=ON -DSANDBOX_GE_EXTRA_INCLUDE_DIRS="E:/dev/cloth_solver/modules/math/include"
cmake --build build --target SandboxGE_Demo
```

Runtime notes:
- Make the `shaders/` folder available next to the executable or run the demo from the repo root; it also checks the parent directory for `shaders/`.
- The demo uses Phong shading, SSAO, shadows, and a simple camera pointed at the origin so it is ready for renderer experiments.

## Usage notes

SandboxGE is now cloth-agnostic: it only knows about generic meshes (positions/normals/uvs/indices + per-mesh colors). Game/simulation layers are responsible for converting their data (e.g., cloth particle meshes, collider meshes) into `MeshSource` buffers before calling `syncPrimaryMeshes`/`syncMeshes`.

## Assets

Copy `shaders/` next to your executable or configure your runtime to point `ShaderPathResolver` at this folder.
