#version 460 core

// Inputs from vertex shader
in vec3 fragmentNormal;
in vec3 worldPos;

// Output
out vec4 fragColor;

// Material UBO (binding point 1)
layout(std140, binding = 1) uniform MaterialBlock {
    vec4 baseColor;        // RGB + metallic
    vec4 properties;       // roughness, ao, emission, useTexture
    mat4 textureTransform;
};

void main() {
    // Normalize the normal
    vec3 normal = normalize(fragmentNormal);
    
    // Simple lighting calculation
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    float diff = max(dot(normal, lightDir), 0.0);
    
    // Ambient + diffuse
    vec3 ambient = baseColor.rgb * 0.2;
    vec3 diffuse = baseColor.rgb * diff * 0.8;
    
    fragColor = vec4(ambient + diffuse, 1.0);
}
