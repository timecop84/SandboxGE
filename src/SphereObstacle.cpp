#include "SphereObstacle.h"
#include "Renderer.h"
#include <GeometryFactory.h>
#include <Matrix.h>
#include <ShaderLib.h>
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <cmath>

// Simple 3D noise using sin combinations (similar to cloth turbulence)
float SphereObstacle::noiseFunction(float x, float y, float z) const {
    float t = m_deformationTime * m_deformationSpeed;
    float value = 0.0f;
    float amplitude = 1.0f;
    float frequency = m_deformationFrequency;
    float maxValue = 0.0f;
    
    for (int i = 0; i < m_deformationOctaves; ++i) {
        // Pseudo-random noise using sin/cos combinations
        float n1 = sin(x * frequency + t * 0.7f) * cos(y * frequency * 1.1f + t * 0.3f);
        float n2 = sin(y * frequency * 0.9f + t * 0.5f) * cos(z * frequency + t * 0.8f);
        float n3 = sin(z * frequency * 1.2f + t * 0.6f) * cos(x * frequency * 0.8f + t * 0.4f);
        float n4 = sin((x + y) * frequency * 0.7f + t) * cos((y + z) * frequency * 0.6f);
        
        value += (n1 + n2 + n3 + n4) * 0.25f * amplitude;
        maxValue += amplitude;
        
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }
    
    return value / maxValue; // Normalize to roughly -1 to 1
}

void SphereObstacle::generateDeformedSphere() {
    const int stacks = m_sphereSegments;
    const int slices = m_sphereSegments * 2;
    const float PI = 3.14159265359f;
    
    m_deformedVertices.clear();
    m_deformedNormals.clear();
    m_indices.clear();
    
    // Generate vertices
    for (int i = 0; i <= stacks; ++i) {
        float phi = PI * float(i) / float(stacks);
        float sinPhi = sin(phi);
        float cosPhi = cos(phi);
        
        for (int j = 0; j <= slices; ++j) {
            float theta = 2.0f * PI * float(j) / float(slices);
            float sinTheta = sin(theta);
            float cosTheta = cos(theta);
            
            // Base sphere position (unit sphere)
            float x = cosTheta * sinPhi;
            float y = cosPhi;
            float z = sinTheta * sinPhi;
            
            // Apply deformation
            float deform = 1.0f;
            if (m_deformationEnabled) {
                float noise = noiseFunction(x * 3.0f, y * 3.0f, z * 3.0f);
                deform = 1.0f + noise * m_deformationStrength;
            }
            
            // Deformed position
            m_deformedVertices.push_back(x * deform);
            m_deformedVertices.push_back(y * deform);
            m_deformedVertices.push_back(z * deform);
            
            // Normal (approximate - for proper normals we'd need to compute gradient)
            // For now, use radial direction which works well for small deformations
            m_deformedNormals.push_back(x);
            m_deformedNormals.push_back(y);
            m_deformedNormals.push_back(z);
        }
    }
    
    // Generate indices
    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < slices; ++j) {
            int first = i * (slices + 1) + j;
            int second = first + slices + 1;
            
            m_indices.push_back(first);
            m_indices.push_back(second);
            m_indices.push_back(first + 1);
            
            m_indices.push_back(second);
            m_indices.push_back(second + 1);
            m_indices.push_back(first + 1);
        }
    }
}

float SphereObstacle::getDeformedRadius(const Vector& worldPos) const {
    if (!m_deformationEnabled) {
        return m_obstRadius;
    }
    
    // Get direction from sphere center to point
    Vector dir = worldPos - m_obstPosition;
    float dist = sqrt(dir.m_x * dir.m_x + dir.m_y * dir.m_y + dir.m_z * dir.m_z);
    if (dist < 0.001f) return m_obstRadius;
    
    // Normalize direction (this is the unit sphere position)
    float x = dir.m_x / dist;
    float y = dir.m_y / dist;
    float z = dir.m_z / dist;
    
    // Sample noise at this direction
    float noise = noiseFunction(x * 3.0f, y * 3.0f, z * 3.0f);
    float deform = 1.0f + noise * m_deformationStrength;
    
    return m_obstRadius * deform;
}

void SphereObstacle::updateDeformation(float deltaTime) {
    if (m_deformationEnabled) {
        m_deformationTime += deltaTime;
        generateDeformedSphere();
        
        // Update GPU buffers
        if (m_bufferInitialized) {
            glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0, m_deformedVertices.size() * sizeof(float), m_deformedVertices.data());
            
            glBindBuffer(GL_ARRAY_BUFFER, m_nbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0, m_deformedNormals.size() * sizeof(float), m_deformedNormals.data());
            
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
    }
}

SphereObstacle::SphereObstacle() {
  m_obstPosition = Vector(25, 15, 25);
  m_obstRadius = 7.0;
  m_colour.set(0.5f, 1.0f, 0.6f, 1.0f);
  m_sphereWireframe = false;
  
  // Deformation defaults
  m_deformationEnabled = false;
  m_deformationStrength = 0.15f;
  m_deformationFrequency = 2.0f;
  m_deformationSpeed = 1.5f;
  m_deformationTime = 0.0f;
  m_deformationOctaves = 3;
  m_sphereSegments = 40;
  m_bufferInitialized = false;
  m_vao = m_vbo = m_nbo = m_ebo = 0;
  
  // Generate initial sphere
  generateDeformedSphere();
}

void SphereObstacle::draw(const std::string &_shaderName,
                          TransformStack &_transform, Camera *_cam) const {
  ShaderLib *shader = ShaderLib::instance();
  auto wrapper = (*shader)[_shaderName];
  if (!wrapper) return;
  wrapper->use();

  // Set sphere material - brighter ambient for visibility
  wrapper->setUniform("material.ambient", glm::vec4(m_colour.m_r * 0.5f, m_colour.m_g * 0.5f, m_colour.m_b * 0.5f, 1.0f));
  wrapper->setUniform("material.diffuse", glm::vec4(m_colour.m_r, m_colour.m_g, m_colour.m_b, 1.0f));
  wrapper->setUniform("material.specular", glm::vec4(0.6f, 0.6f, 0.6f, 1.0f));
  wrapper->setUniform("material.shininess", 64.0f);
  
  // Subtle AO for sphere - just hemisphere darkening
  wrapper->setUniform("aoStrength", 0.3f);
  wrapper->setUniform("aoGroundColor", glm::vec3(0.5f, 0.45f, 0.4f));

  glPolygonMode(GL_FRONT_AND_BACK, m_sphereWireframe ? GL_LINE : GL_FILL);

  _transform.pushTransform();
  _transform.setPosition(m_obstPosition);
  _transform.setScale(m_obstRadius, m_obstRadius, m_obstRadius);
  Renderer::loadMatricesToShader(_shaderName, _transform, _cam);

  if (m_deformationEnabled && !m_deformedVertices.empty()) {
    // Use custom deformed sphere
    SphereObstacle* nonConstThis = const_cast<SphereObstacle*>(this);
    
    if (!m_bufferInitialized) {
      glGenVertexArrays(1, &nonConstThis->m_vao);
      glGenBuffers(1, &nonConstThis->m_vbo);
      glGenBuffers(1, &nonConstThis->m_nbo);
      glGenBuffers(1, &nonConstThis->m_ebo);
      nonConstThis->m_bufferInitialized = true;
      
      glBindVertexArray(m_vao);
      
      // Vertex buffer
      glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
      glBufferData(GL_ARRAY_BUFFER, m_deformedVertices.size() * sizeof(float), m_deformedVertices.data(), GL_DYNAMIC_DRAW);
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
      glEnableVertexAttribArray(0);
      
      // Normal buffer
      glBindBuffer(GL_ARRAY_BUFFER, m_nbo);
      glBufferData(GL_ARRAY_BUFFER, m_deformedNormals.size() * sizeof(float), m_deformedNormals.data(), GL_DYNAMIC_DRAW);
      glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
      glEnableVertexAttribArray(2);
      
      // Index buffer
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(unsigned int), m_indices.data(), GL_STATIC_DRAW);
      
      glBindVertexArray(0);
    }
    
    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_indices.size()), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
  } else {
    // Use standard sphere geometry
    static auto sphereGeometry =
        FlockingGraphics::GeometryFactory::instance().createSphere(1.0f, 40);
    if (sphereGeometry)
      sphereGeometry->render();
  }
  
  _transform.popTransform();
}

void SphereObstacle::renderGeometryOnly() const {
  // Render sphere geometry without any shader setup (for shadow pass)
  if (m_deformationEnabled && !m_deformedVertices.empty() && m_bufferInitialized) {
    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_indices.size()), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
  } else {
    static auto sphereGeometry =
        FlockingGraphics::GeometryFactory::instance().createSphere(1.0f, 40);
    if (sphereGeometry)
      sphereGeometry->render();
  }
}

Vector SphereObstacle::getPosition() { return m_obstPosition; }

float SphereObstacle::getRadius() { return m_obstRadius; }

void SphereObstacle::setObstacleWireframe(bool setEnable) {
  m_sphereWireframe = setEnable;
}

SphereObstacle::~SphereObstacle() {
  if (m_bufferInitialized) {
    glDeleteVertexArrays(1, &m_vao);
    glDeleteBuffers(1, &m_vbo);
    glDeleteBuffers(1, &m_nbo);
    glDeleteBuffers(1, &m_ebo);
  }
}
