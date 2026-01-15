#include "rendering/InstancedRenderable.h"
#include "rendering/UBOManager.h"
#include "rendering/UBOStructures.h"
#include <glad/gl.h>
#include <core/Camera.h>
#include <utils/GeometryFactory.h>
#include <iostream>

namespace sandbox {

InstancedRenderable::InstancedRenderable(GeometryHandle geometry, Material* material)
    : m_geometry(geometry), m_material(material), m_dummyTransform(1.0f) {}

InstancedRenderable::~InstancedRenderable() {}

void InstancedRenderable::addInstance(const glm::mat4& transform) {
    if (m_instances.size() >= InstanceLimits::MAX_INSTANCES_PER_BATCH) {
        std::cerr << "InstancedRenderable: Maximum instance count reached (" 
                  << InstanceLimits::MAX_INSTANCES_PER_BATCH << ")" << std::endl;
        return;
    }
    m_instances.push_back(transform);
}

void InstancedRenderable::clearInstances() {
    m_instances.clear();
}

const glm::mat4& InstancedRenderable::getTransform() const {
    // Return first instance transform, or identity if empty
    return m_instances.empty() ? m_dummyTransform : m_instances[0];
}

void InstancedRenderable::render(const RenderContext& context) {
    renderInternal(context, false);
}

void InstancedRenderable::renderShadow(const RenderContext& context) {
    renderInternal(context, true);
}

void InstancedRenderable::renderInternal(const RenderContext& context, bool shadowPass) {
    if (!m_geometry || !m_material || !context.camera || m_instances.empty()) {
        return;
    }
    
    // Bind material
    if (!shadowPass) {
        m_material->bind(context);
    }
    
    // Upload instance matrices to UBO
    uploadInstanceMatrices(context);
    
    // Build shared matrix UBO (view/projection only, model comes from instances)
    MatrixUBO matrixUBO;
    matrixUBO.model = glm::mat4(1.0f); // Identity, not used for instanced
    matrixUBO.view = context.camera->getViewMatrix();
    matrixUBO.projection = context.camera->getProjectionMatrix();
    matrixUBO.MVP = matrixUBO.projection * matrixUBO.view; // MVP without model
    matrixUBO.normalMatrix = glm::mat4(1.0f); // Not used for instanced
    
    UBOManager::instance()->updateUBO("Matrices", &matrixUBO, sizeof(MatrixUBO));
    
    // Draw instanced
    if (m_geometry) {
        // Get VAO from geometry
        glBindVertexArray(m_geometry->VAO);
        glDrawElementsInstanced(
            GL_TRIANGLES,
            static_cast<GLsizei>(m_geometry->indexCount),
            GL_UNSIGNED_INT,
            nullptr,
            static_cast<GLsizei>(m_instances.size())
        );
        glBindVertexArray(0);
    }
    
    // Unbind material
    if (!shadowPass) {
        m_material->unbind();
    }
}

void InstancedRenderable::uploadInstanceMatrices(const RenderContext& context) {
    (void)context;
    // Upload instance matrices to Instances UBO
    // Note: We upload up to MAX_INSTANCES_PER_BATCH matrices
    size_t count = std::min(m_instances.size(), static_cast<size_t>(InstanceLimits::MAX_INSTANCES_PER_BATCH));
    size_t dataSize = count * sizeof(glm::mat4);
    
    UBOManager::instance()->updateUBO("Instances", m_instances.data(), dataSize);
}

uint64_t InstancedRenderable::getSortKey(const RenderContext& context) const {
    if (!m_material || !context.camera || m_instances.empty()) {
        return 0;
    }
    
    // Use first instance position for depth sorting
    glm::vec3 pos = glm::vec3(m_instances[0][3]);
    glm::vec3 camPos = context.camera->getEye();
    float depth = glm::length(camPos - pos);
    uint16_t depthKey = static_cast<uint16_t>(glm::clamp(depth, 0.0f, 65535.0f));
    
    // Get shader hash (top 24 bits)
    ShaderHash shaderHash = static_cast<ShaderHash>(std::hash<std::string>{}(m_material->getShaderName()));
    uint64_t shaderPart = (static_cast<uint64_t>(shaderHash) & 0xFFFFFF) << 40;
    
    // Get material ID (next 24 bits)
    uint64_t materialPart = (static_cast<uint64_t>(m_material->getMaterialID()) & 0xFFFFFF) << 16;
    
    // Depth (bottom 16 bits)
    uint64_t depthPart = depthKey;
    
    return shaderPart | materialPart | depthPart;
}

} // namespace sandbox
