#pragma once

// SandboxGE - OpenGL Graphics Engine Public API
// Single header to include all public interfaces

// Core Types
#include "core/Types.h"
#include "core/RenderSettings.h"
#include "core/Camera.h"
#include "core/Light.h"
#include "core/Colour.h"

// Rendering System
#include "rendering/IRenderable.h"
#include "rendering/UnifiedRenderer.h"
#include "rendering/RenderQueue.h"
#include "rendering/ShadowRenderer.h"
#include "rendering/SSAORenderer.h"

// Renderables (new API)
#include "renderables/FloorRenderable.h"
#include "renderables/SphereRenderable.h"
#include "rendering/MeshRenderable.h"
#include "rendering/InstancedRenderable.h"

// Materials
#include "materials/Material.h"

// Utilities
#include "utils/ShaderLib.h"
#include "utils/TransformStack.h"
#include "utils/GeometryFactory.h"
#include "utils/LightingHelper.h"
#include "utils/ShaderPathResolver.h"

// Resource Management
#include "core/ResourceManager.h"

// To use SandboxGE:
// 1. Include this single header: #include <SandboxGE.h>
// 2. Create renderables (e.g. sandbox::FloorRenderable)
// 3. Submit them to a RenderQueue
// 4. Render using sandbox::UnifiedRenderer
