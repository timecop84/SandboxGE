#include "Floor.h"
#include "Renderer.h"
#include <GeometryFactory.h>
#include <Material.h>
#include <Matrix.h>
#include <ShaderLib.h>
#include <glad/gl.h>
#include <glm/glm.hpp>

Floor::Floor() {
  m_position = Vector(20, 0, 20);
  m_width = 100;
  m_length = 100;
  m_floorWireframe = false;
  m_color = Colour(0.5f, 0.5f, 0.5f);
}

Floor::Floor(float width, float length, int /*pWidth*/, int /*pLength*/, Vector position) {
  m_width = width;
  m_length = length;
  m_position = position;
  m_color = Colour(0.5f, 0.5f, 0.5f);
}

void Floor::draw(const std::string &_shaderName, TransformStack &_transform, Camera *_cam) const {
  ShaderLib *shader = ShaderLib::instance();
  auto wrapper = (*shader)[_shaderName];
  if (!wrapper) return;
  wrapper->use();

  // Set floor material - brighter ambient for visibility
  wrapper->setUniform("material.ambient", glm::vec4(m_color.m_r * 0.6f, m_color.m_g * 0.6f, m_color.m_b * 0.6f, 1.0f));
  wrapper->setUniform("material.diffuse", glm::vec4(m_color.m_r, m_color.m_g, m_color.m_b, 1.0f));
  wrapper->setUniform("material.specular", glm::vec4(0.3f, 0.3f, 0.3f, 1.0f));
  wrapper->setUniform("material.shininess", 16.0f);
  
  // Disable AO for floor (it IS the ground)
  wrapper->setUniform("aoStrength", 0.0f);
  wrapper->setUniform("aoGroundColor", glm::vec3(1.0f, 1.0f, 1.0f));

  glPolygonMode(GL_FRONT_AND_BACK, m_floorWireframe ? GL_LINE : GL_FILL);

  static auto planeGeometry = FlockingGraphics::GeometryFactory::instance().createCube(1.0f);

  _transform.pushTransform();
  _transform.setPosition(m_position);
  _transform.setScale(m_width, 0.1f, m_length);
  Renderer::loadMatricesToShader(_shaderName, _transform, _cam);

  if (planeGeometry) planeGeometry->render();
  _transform.popTransform();
}

void Floor::setPosition(Vector position) { m_position = position; }
const Vector &Floor::getPosition() { return m_position; }
void Floor::setFloorWireframe(bool setEnable) { m_floorWireframe = setEnable; }
void Floor::setColor(Colour color) { m_color = color; }
