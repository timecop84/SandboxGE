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

// Simplified silk-like rendering with anisotropic highlights
void main() {
    vec3 normal = normalize(fragmentNormal);
    
    // Light direction
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    
    // View direction (camera at origin assumption for now)
    vec3 viewDir = normalize(-worldPos);
    
    // Tangent-based anisotropy (simplified)
    vec3 tangent = normalize(cross(normal, vec3(0.0, 1.0, 0.0)));
    vec3 bitangent = cross(normal, tangent);
    
    // Anisotropic specular
    vec3 halfVec = normalize(lightDir + viewDir);
    float dotTH = dot(tangent, halfVec);
    float dotBH = dot(bitangent, halfVec);
    float aniso = sqrt(max(0.0, 1.0 - dotTH * dotTH - dotBH * dotBH));
    
    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = baseColor.rgb * diff * 0.7;
    
    // Specular with anisotropic highlight
    float spec = pow(aniso, 32.0);
    vec3 specular = vec3(0.6) * spec;
    
    // Sheen effect
    float sheen = pow(1.0 - max(dot(normal, viewDir), 0.0), 3.0);
    vec3 sheenColor = vec3(0.3) * sheen;
    
    // Ambient
    vec3 ambient = baseColor.rgb * 0.2;
    
    fragColor = vec4(ambient + diffuse + specular + sheenColor, 1.0);
}
