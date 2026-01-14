#include "renderables/SphereRenderable.h"
#include "rendering/UBOManager.h"
#include "rendering/UBOStructures.h"
#include <Camera.h>
#include <GeometryFactory.h>
#include <ShadowRenderer.h>
#include <glm/gtc/matrix_transform.hpp>

namespace gfx {

SphereRenderable::SphereRenderable(float radius, const glm::vec3& position, const glm::vec3& color)
    : m_radius(radius), m_position(position), m_color(color), 
      m_wireframe(false), m_material(nullptr) {
    createGeometry();
    
    // Create material
    m_material = Material::createPhong(m_color);
    
    updateTransform();
}

SphereRenderable::~SphereRenderable() {
    delete m_material;
}

void SphereRenderable::createGeometry() {
    // Create sphere with decent tessellation
    m_geometry = FlockingGraphics::GeometryFactory::instance().createSphere(1.0f, 32);
}

void SphereRenderable::updateTransform() {
    // Position and scale the sphere
    m_transform = glm::translate(glm::mat4(1.0f), m_position);
    m_transform = glm::scale(m_transform, glm::vec3(m_radius));
}

void SphereRenderable::setPosition(const glm::vec3& position) {
    m_position = position;
    updateTransform();
}

void SphereRenderable::setRadius(float radius) {
    m_radius = radius;
    updateTransform();
}

void SphereRenderable::setColor(const glm::vec3& color) {
    m_color = color;
    if (m_material) {
        m_material->setDiffuse(color);
    }
}

void SphereRenderable::setWireframe(bool wireframe) {
    m_wireframe = wireframe;
}

void SphereRenderable::setMaterial(Material* material) {
    m_material = material;
    if (m_material) {
        m_material->setDiffuse(m_color);
    }
}

void SphereRenderable::render(const RenderContext& context) {
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

void SphereRenderable::renderShadow(const RenderContext& context) {
    if (!m_geometry) {
        return;
    }
    
    // For shadow pass, just set model matrix and render
    // The shadow shader is already active with light matrices
    Shadow::setModelMatrix(m_transform);
    
    // Render geometry
    m_geometry->render();
}

uint64_t SphereRenderable::getSortKey(const RenderContext& context) const {
    if (!m_material || !context.camera) {
        return 0;
    }
    
    // Calculate depth
    glm::vec3 camPos = context.camera->getEye();
    float depth = glm::length(camPos - m_position);
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
