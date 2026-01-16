#version 460 core

// Input attributes (match GeometryFactory layout)
layout(location = 0) in vec3 inVert;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;

// Output to fragment shader
out vec3 fragmentNormal;
out vec3 worldPos;
out vec3 vPosition;
out vec2 TexCoords;

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
    vec4 viewPosition = view * worldPosition;
    vPosition = viewPosition.xyz / viewPosition.w;
    gl_Position = projection * viewPosition;
    
    // Transform normal
    fragmentNormal = mat3(normalMatrix) * inNormal;
    TexCoords = inTexCoord;
}
