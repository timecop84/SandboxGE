#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 2) in vec3 aNormal;
layout(location = 1) in vec2 aTexCoords;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;

layout(std140, binding = 0) uniform MatrixBlock {
    mat4 model;
    mat4 view;
    mat4 projection;
    mat4 MVP;
    mat4 normalMatrix;
};

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(normalMatrix) * aNormal;
    TexCoords = aTexCoords;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
