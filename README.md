# SandboxGE API

SandboxGE is a small renderer library (currently OpenGL backend) used by the cloth sandbox.

Consumer-facing usage is via:

```cpp
#include <SandboxGE.h>
```

All engine-facing types live in the `sandbox::` namespace (legacy helpers like `ShaderPath` are still global).

## Table of Contents

- [Quick Start](#quick-start)
- [Core Concepts](#core-concepts)
- [Backend-Agnostic Layer](#backend-agnostic-layer)
- [API Reference](#api-reference)
  - [Renderer](#renderer)
  - [Renderables](#renderables)
  - [Materials](#materials)
  - [Render Settings](#render-settings)
  - [Resource Manager](#resource-manager)
  - [Utilities](#utilities)
- [Shaders](#shaders)
- [Build](#build)
- [Demo Scene](#demo-scene)

---

## Quick Start

Minimal “submit a floor + sphere” example:

```cpp
#include <SandboxGE.h>

int main() {
    // Your app must create a GL context (GLFW/SDL/etc) before calling initialize.

    sandbox::UnifiedRenderer renderer;
    if (!renderer.initialize(1280, 720)) {
        return 1;
    }

    sandbox::RenderSettings settings;
    settings.shadowEnabled = true;
    settings.ssaoEnabled = true;

    // Camera is a shared core type.
    sandbox::Camera camera;
    camera.setShape(55.0f, 1280.0f / 720.0f, 0.1f, 250.0f);

    // Optional: set shader root if your exe isn't next to the shaders folder.
    ShaderPath::setRoot("E:/dev/SandboxGE/shaders");

    sandbox::FloorRenderable floor(
        100.0f, 100.0f,
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.7f, 0.7f, 0.7f));

    sandbox::SphereRenderable sphere(
        2.0f,
        glm::vec3(0.0f, 2.0f, 0.0f),
        glm::vec3(0.6f, 0.9f, 0.7f));

    // Per-frame:
    renderer.beginFrame(&camera, /*timeSeconds=*/0.0f);
    renderer.submit(&floor);
    renderer.submit(&sphere);
    renderer.renderFrame(settings);
    renderer.endFrame();

    renderer.cleanup();
    return 0;
}
```

---

## Backend-Agnostic Layer

SandboxGE is moving toward a backend-agnostic API surface. Public headers now expose `sandbox::rhi` enums
instead of raw OpenGL types for buffers and textures. The current implementation still uses OpenGL behind
the scenes in `.cpp` files.

Current RHI-facing types:

- `sandbox::rhi::BufferUsage` (`Static`, `Dynamic`, `Stream`)
- `sandbox::rhi::TextureFormat` (`R8`, `R16F`, `R32F`, `RG16F`, `RGB16F`, `RGBA16F`, `RGBA8`, `Depth24`, `Depth32F`)
- `sandbox::rhi::TextureTarget` (`Texture2D`)
- `sandbox::rhi::SamplerDesc` (filter/wrap/compare state)

New device scaffolding lives in `rhi/Device.h` and `rhi/DeviceGL.h`. The OpenGL implementation is created via:

```cpp
auto device = sandbox::rhi::createOpenGLDevice();
```

Render passes, shaders, and post effects can be wired to this `Device` interface to decouple from OpenGL
and swap backend implementations (OpenGL/Vulkan/Metal/etc).

---

## Core Concepts

### Render Loop

SandboxGE’s render flow is:

1. `UnifiedRenderer::initialize(width, height)`
2. Per frame:
    - `beginFrame(camera, time)` (camera is `sandbox::Camera*`)
    - `submit(renderable)` (0..N times)
    - `renderFrame(settings)`
    - `endFrame()`
3. `cleanup()` when shutting down

### Renderables (Concept)

All drawables implement `sandbox::IRenderable`:

- `render(const RenderContext&)`
- `renderShadow(const RenderContext&)`
- `getSortKey(const RenderContext&)`
- `getMaterial()` and `getTransform()`

Renderables are submitted as raw pointers (`IRenderable*`). Ownership stays with the app.

### RenderContext

`sandbox::RenderContext` is passed to renderables during execution. It includes:

- `sandbox::Camera* camera`
- viewport (`viewportWidth`, `viewportHeight`)
- current pass (`Pass::{SHADOW,SCENE,SSAO,COMPOSITE}`)
- shadow matrices (`shadowMatrices[4]`)
- cascade/shadow routing (`useCascades`, `cascadeSplits`, `lightShadowMapIndex`, `lightCascadeStart`, `lightCascadeCount`)
- environment inputs (`envMapTextureId`, `iblIrradianceMap`, `iblPrefilterMap`, `iblBrdfLut`)
- refraction inputs (`refractionSourceTexture`, `mainLightPosition`, `mainLightColor`)

---

## API Reference

### Renderer

#### `sandbox::UnifiedRenderer`

Primary entry point.

| Method | Notes |
| ------ | ----- |
| `bool initialize(int width, int height)` | Creates internal GPU resources and post-processing passes |
| `void resize(int width, int height)` | Resize viewport and post-process buffers |
| `void setShaderRoot(const std::string& rootDir)` | Override shader search root |
| `void beginFrame(Camera* camera, float time)` | Starts collecting renderables |
| `void submit(IRenderable* renderable, bool castsShadows = true)` | Adds to internal queue |
| `void renderFrame(const RenderSettings& settings)` | Executes shadow + scene + SSAO + composite |
| `void endFrame()` | Ends the frame; queue is cleared |
| `void cleanup()` | Releases GPU resources |
| `const Stats& getStats() const` | Draw/triangle/instance counters |

### Renderables

Built-in renderables included by `SandboxGE.h`:

- `sandbox::FloorRenderable`
  - `setPosition`, `setColor`, `setWireframe`, `setMaterialPreset`
- `sandbox::SphereRenderable`
  - `setPosition`, `setRadius`, `setColor`, `setWireframe`, `setMaterialPreset`, `setMaterial`
- `sandbox::MeshRenderable`
  - Wraps a `GeometryHandle` + `Material*` + transform
- `sandbox::InstancedRenderable`
  - Repeated geometry via instancing (`addInstance`, `clearInstances`)

All of these are `IRenderable` and can be mixed in the same frame.

### Materials

#### `sandbox::Material`

A material wraps a shader program selection plus a `MaterialUBO` payload.

Common factories:

- `Material::createPhong(...)`
- `Material::createSilk(...)`
- `Material::createSilkPBR(...)`
- `Material::createShadow()`

Other helpers:

- `setRefractionIor(...)` for the `Refraction` shader

RAII-friendly overloads:

- `Material::createPhongUnique(...)`
- `Material::createSilkUnique(...)`
- `Material::createSilkPBRUnique(...)`
- `Material::createShadowUnique()`

Materials are typically owned by the app (or a higher-level system) and referenced by renderables.

### Render Settings

#### `sandbox::RenderSettings`

Per-frame toggles and parameters used by the renderer:

- Visibility toggles (`clothVisibility`, `floorVisibility`, `sphereVisibility`, `customMeshVisibility`, …)
- Post effects (`shadowEnabled`, `ssaoEnabled`, …)
- Cascaded shadows (`useCascadedShadows`, `cascadeCount`, `cascadeSplitLambda`, `cascadeMaxDistance`, `debugCascades`)
- Primary light (`lightPosition`, `lightDiffuse`, …)
- Additional lights (`std::vector<ExtraLight> lights`)
- Material-related knobs for Silk/SilkPBR and checker overlay
- Environment/IBL (`envMapTextureId`, `envMapIntensity`, `iblEnabled`, `iblIrradianceMap`, `iblPrefilterMap`, `iblBrdfLut`)

### Resource Manager

#### `sandbox::ResourceManager`

Handle-based registry for GPU resources.

| Method | Notes |
| ------ | ----- |
| `GeometryHandle getGeometry(const std::string& name)` | Delegates to `GeometryFactory` |
| `void setDevice(std::unique_ptr<rhi::Device> device)` | Override the active rendering backend |
| `BufferHandle createBuffer(name, size, rhi::BufferUsage usage)` | Creates a named buffer |
| `TextureHandle createTexture(name, w, h, rhi::TextureFormat format, rhi::TextureTarget target = Texture2D)` | Creates a named texture |
| `void clearAll()` | Releases all registered resources |

`rhi::BufferUsage` includes `Static`, `Dynamic`, `Stream`. `rhi::TextureFormat` includes `R8`, `R16F`, `R32F`, `RG16F`, `RGB16F`, `RGBA16F`, `RGBA8`, `Depth24`, `Depth32F`.

Minimal usage:

```cpp
auto* rm = sandbox::ResourceManager::instance();
auto vbo = rm->createBuffer("mesh.vbo", 1024 * 1024, sandbox::rhi::BufferUsage::Dynamic);
auto tex = rm->createTexture("gbuffer.albedo", 1024, 1024, sandbox::rhi::TextureFormat::RGBA8);
```

Migration note:

If you are swapping in a non-OpenGL backend, call `ResourceManager::setDevice(...)` before `UnifiedRenderer::initialize(...)` so render passes, SSAO, and shadow maps allocate textures through your backend device.

### Utilities

Included utility headers:

- `utils/ShaderLib.h` (sandbox::ShaderLib)
- `utils/TransformStack.h`
- `utils/GeometryFactory.h`
- `utils/LightingHelper.h` (sandbox::Lighting)
- `utils/ShaderPathResolver.h` (global `ShaderPath` helper)

---

## Shaders

SandboxGE expects a `shaders/` folder to be discoverable at runtime.

Ways to make that work:

- Run the executable from a directory where `./shaders` exists
- Copy `SandboxGE/shaders/` next to the executable
- Call `UnifiedRenderer::setShaderRoot(...)` to point at the shader directory

---

## Build

```bash
cmake -S . -B build -DSANDBOX_GE_CXX_STANDARD=20
cmake --build build
```

- Target name: `SandboxGE`
- C++ standard defaults to 20; set `-DSANDBOX_GE_CXX_STANDARD=23` to enable C++23.
- Depends on an existing GLFW target. If your build already defines `glfw`/`glfw3`/`glfw3::glfw`, it is picked up automatically; otherwise set `-DSANDBOX_GE_GLFW_TARGET=<target>` or make `glfw3` discoverable via `find_package`.
- If you include the `external/MathLib` submodule, the include path is auto-detected; otherwise point `SANDBOX_GE_EXTRA_INCLUDE_DIRS` at your math headers when calling `add_subdirectory`.
- OpenGL is linked via `opengl32` (Windows). Adjust if you target another platform.

## Demo Scene

Enable the built-in demo to get a quick test scene (floor + cube + sphere + an orbiting triangle) rendered with SandboxGE:

```bash
cmake -S . -B build -DSANDBOX_GE_BUILD_DEMO=ON
cmake --build build --target UnifiedDemo
```

Runtime notes:

- Make the `shaders/` folder available next to the executable or run the demo from the repo root; it also checks the parent directory for `shaders/`.
- The demo uses Phong shading, SSAO, shadows, and a simple camera pointed at the origin so it is ready for renderer experiments.
