#include "renderables/FloorRenderable.h"
#include "rendering/UBOManager.h"
#include "rendering/UBOStructures.h"
#include <core/Camera.h>
#include <utils/GeometryFactory.h>
#include <rendering/ShadowRenderer.h>
#include <glm/gtc/matrix_transform.hpp>

namespace gfx {

FloorRenderable::FloorRenderable(float width, float length, const glm::vec3& position, const glm::vec3& color)
    : m_width(width), m_length(length), m_position(position), m_color(color), 
      m_wireframe(false), m_material(nullptr) {
    createGeometry();
    
        rebuildMaterial();
    
    updateTransform();
}

FloorRenderable::~FloorRenderable() {
    delete m_material;
}

void FloorRenderable::createGeometry() {
    // Use a scaled cube as the floor plane
    m_geometry = FlockingGraphics::GeometryFactory::instance().createCube(1.0f);
}

void FloorRenderable::updateTransform() {
    // Position and scale the floor
    m_transform = glm::translate(glm::mat4(1.0f), m_position);
    m_transform = glm::scale(m_transform, glm::vec3(m_width, 0.1f, m_length));
}

void FloorRenderable::setPosition(const glm::vec3& position) {
    m_position = position;
    updateTransform();
}

void FloorRenderable::setColor(const glm::vec3& color) {
    m_color = color;
    if (m_material) {
        m_material->setDiffuse(color);
    }
}

void FloorRenderable::setWireframe(bool wireframe) {
    m_wireframe = wireframe;
}

void FloorRenderable::setMaterialPreset(MaterialPreset preset) {
    if (m_materialPreset == preset) return;
    m_materialPreset = preset;
    rebuildMaterial();
}

void FloorRenderable::rebuildMaterial() {
    if (m_material) {
        delete m_material;
        m_material = nullptr;
    }

    switch (m_materialPreset) {
    case MaterialPreset::Silk:
        m_material = Material::createSilk(m_color);
        break;
    case MaterialPreset::SilkPBR:
        m_material = Material::createSilkPBR(m_color);
        break;
    case MaterialPreset::Phong:
    default:
        m_material = Material::createPhong(m_color);
        break;
    }
}

void FloorRenderable::render(const RenderContext& context) {
    if (!m_geometry || !m_material || !context.camera) {
        return;
    }
    
    // Bind material
    m_material->bind(context);
    
    // Build matrix UBO
    MatrixUBO matrixUBO;
    matrixUBO.model = m_transform;
    matrixUBO.view = context.camera->getViewMatrix();
    matrixUBO.projection = context.camera->getProjectionMatrix();
    matrixUBO.MVP = matrixUBO.projection * matrixUBO.view * matrixUBO.model;
    
    // Normal matrix
    glm::mat3 normalMat3 = glm::transpose(glm::inverse(glm::mat3(matrixUBO.model)));
    matrixUBO.normalMatrix = glm::mat4(normalMat3);
    
    // Upload to UBO
    UBOManager::instance()->updateUBO("Matrices", &matrixUBO, sizeof(MatrixUBO));
    
    // Set wireframe if needed
    if (m_wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    
    // Render geometry
    m_geometry->render();
    
    // Reset wireframe
    if (m_wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    
    // Unbind material
    m_material->unbind();
}

void FloorRenderable::renderShadow(const RenderContext& context) {
    if (!m_geometry) {
        return;
    }
    
    // For shadow pass, just set model matrix and render
    // The shadow shader is already active with light matrices
    Shadow::setModelMatrix(m_transform);
    
    // Render geometry
    m_geometry->render();
}

uint64_t FloorRenderable::getSortKey(const RenderContext& context) const {
    if (!m_material || !context.camera) {
        return 0;
    }
    
    // Calculate depth
    glm::vec3 pos = glm::vec3(m_transform[3]);
    glm::vec3 camPos = context.camera->getEye();
    float depth = glm::length(camPos - pos);
    uint16_t depthKey = static_cast<uint16_t>(glm::clamp(depth, 0.0f, 65535.0f));
    
    // Get shader hash
    ShaderHash shaderHash = std::hash<std::string>{}(m_material->getShaderName());
    uint64_t shaderPart = (static_cast<uint64_t>(shaderHash) & 0xFFFFFF) << 40;
    
    // Get material ID
    uint64_t materialPart = (static_cast<uint64_t>(m_material->getMaterialID()) & 0xFFFFFF) << 16;
    
    // Depth
    uint64_t depthPart = depthKey;
    
    return shaderPart | materialPart | depthPart;
}

} // namespace gfx
