#include "rendering/MeshRenderable.h"
#include "rendering/UBOManager.h"
#include "rendering/UBOStructures.h"
#include <Camera.h>
#include <GeometryFactory.h>
#include <ShadowRenderer.h>
#include <glm/gtc/matrix_transform.hpp>

namespace gfx {

MeshRenderable::MeshRenderable(GeometryHandle geometry, Material* material, const glm::mat4& transform)
    : m_geometry(geometry), m_material(material), m_transform(transform) {}

MeshRenderable::~MeshRenderable() {}

void MeshRenderable::render(const RenderContext& context) {
    renderInternal(context, false);
}

void MeshRenderable::renderShadow(const RenderContext& context) {
    if (!m_geometry) {
        return;
    }
    
    // For shadow pass, just set model matrix and render
    // The shadow shader is already active with light matrices
    Shadow::setModelMatrix(m_transform);
    
    // Render geometry
    m_geometry->render();
}

void MeshRenderable::renderInternal(const RenderContext& context, bool shadowPass) {
    if (!m_geometry || !m_material || !context.camera) {
        return;
    }
    
    // Bind material (or shadow material for shadow pass)
    if (!shadowPass) {
        m_material->bind(context);
    }
    
    // Build matrix UBO
    MatrixUBO matrixUBO;
    matrixUBO.model = m_transform;
    matrixUBO.view = context.camera->getViewMatrix();
    matrixUBO.projection = context.camera->getProjectionMatrix();
    matrixUBO.MVP = matrixUBO.projection * matrixUBO.view * matrixUBO.model;
    
    // Normal matrix (inverse transpose of model)
    glm::mat3 normalMat3 = glm::transpose(glm::inverse(glm::mat3(matrixUBO.model)));
    matrixUBO.normalMatrix = glm::mat4(normalMat3); // Store as mat4 for std140 alignment
    
    // Upload to UBO
    UBOManager::instance()->updateUBO("Matrices", &matrixUBO, sizeof(MatrixUBO));
    
    // Render geometry
    if (m_geometry) {
        m_geometry->render();
    }
    
    // Unbind material
    if (!shadowPass) {
        m_material->unbind();
    }
}

uint64_t MeshRenderable::getSortKey(const RenderContext& context) const {
    if (!m_material || !context.camera) {
        return 0;
    }
    
    // Calculate depth from camera
    glm::vec3 pos = glm::vec3(m_transform[3]);
    glm::vec3 camPos = context.camera->getEye();
    float depth = glm::length(camPos - pos);
    uint16_t depthKey = static_cast<uint16_t>(glm::clamp(depth, 0.0f, 65535.0f));
    
    // Get shader hash (top 24 bits)
    ShaderHash shaderHash = std::hash<std::string>{}(m_material->getShaderName());
    uint64_t shaderPart = (static_cast<uint64_t>(shaderHash) & 0xFFFFFF) << 40;
    
    // Get material ID (next 24 bits)
    uint64_t materialPart = (static_cast<uint64_t>(m_material->getMaterialID()) & 0xFFFFFF) << 16;
    
    // Depth (bottom 16 bits)
    uint64_t depthPart = depthKey;
    
    return shaderPart | materialPart | depthPart;
}

} // namespace gfx
