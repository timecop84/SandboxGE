#include "Light.h"
#include "Vector.h"
#include "Colour.h"
#include <ShaderLib.h>

Light::Light()
    : m_position(0.0f, 0.0f, 0.0f)
    , m_color(1.0f, 1.0f, 1.0f)
    , m_transform(1.0f)
    , m_constantAttenuation(1.0f)
    , m_linearAttenuation(0.0f)
    , m_quadraticAttenuation(0.0f)
    , m_enabled(true)
{
}

Light::Light(const glm::vec3& position, const glm::vec3& color)
    : m_position(position)
    , m_color(color)
    , m_transform(1.0f)
    , m_constantAttenuation(1.0f)
    , m_linearAttenuation(0.0f)
    , m_quadraticAttenuation(0.0f)
    , m_enabled(true)
{
}

Light::Light(const Vector& position, const Colour& diffuse, const Colour& specular, int lightType)
    : m_position(position.m_x, position.m_y, position.m_z)
    , m_color(diffuse.m_r, diffuse.m_g, diffuse.m_b)
    , m_transform(1.0f)
    , m_constantAttenuation(1.0f)
    , m_linearAttenuation(0.0f)
    , m_quadraticAttenuation(0.0f)
    , m_enabled(true)
{
}

void Light::setAttenuation(float constant, float linear, float quadratic)
{
    m_constantAttenuation = constant;
    m_linearAttenuation = linear;
    m_quadraticAttenuation = quadratic;
}

void Light::setTransform(const glm::mat4& transform) {
    m_transform = transform;
}

void Light::loadToShader(const std::string& uniformName) const {
    ShaderLib* shader = ShaderLib::instance();
    if (!shader) return;

    glm::vec4 position = m_transform * glm::vec4(m_position, 1.0f);
    glm::vec4 color4(m_color, 1.0f);
    glm::vec4 ambient = color4 * 0.25f;

    shader->setShaderParam(uniformName + ".position", position);
    shader->setShaderParam(uniformName + ".ambient", ambient);
    shader->setShaderParam(uniformName + ".diffuse", color4);
    shader->setShaderParam(uniformName + ".specular", color4);
    shader->setShaderParam(uniformName + ".direction", glm::vec3(0.0f));
    shader->setShaderParam(uniformName + ".constantAttenuation", m_constantAttenuation);
    shader->setShaderParam(uniformName + ".linearAttenuation", m_linearAttenuation);
    shader->setShaderParam(uniformName + ".quadraticAttenuation", m_quadraticAttenuation);
}
