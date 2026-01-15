#version 460 core

// Input attributes
layout(location = 0) in vec3 inVert;
layout(location = 2) in vec3 inNormal;

// Output to fragment shader
out vec3 fragmentNormal;
out vec3 worldPos;
out vec3 viewDir;

// Matrix UBO (binding point 0)
layout(std140, binding = 0) uniform MatrixBlock {
    mat4 model;
    mat4 view;
    mat4 projection;
    mat4 MVP;
    mat4 normalMatrix;
};

void main() {
    // Transform position
    vec4 worldPosition = model * vec4(inVert, 1.0);
    worldPos = worldPosition.xyz;
    gl_Position = projection * view * worldPosition;
    
    // Transform normal
    fragmentNormal = mat3(normalMatrix) * inNormal;
    
    // View direction in world space
    vec4 viewPos = inverse(view) * vec4(0.0, 0.0, 0.0, 1.0);
    viewDir = normalize(viewPos.xyz - worldPos);
}
