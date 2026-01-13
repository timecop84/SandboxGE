#version 150

/// @file Shadow.vs
/// @brief Vertex shader for shadow map depth pass

in vec3 inVert;

uniform mat4 lightSpaceMatrix;
uniform mat4 model;

void main()
{
    gl_Position = lightSpaceMatrix * model * vec4(inVert, 1.0);
}
