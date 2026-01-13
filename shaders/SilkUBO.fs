#version 460 core

// Inputs from vertex shader
in vec3 fragmentNormal;
in vec3 worldPos;

// Output
out vec4 fragColor;

// Material UBO (binding point 1) - matches C++ MaterialUBO structure
layout(std140, binding = 1) uniform MaterialBlock {
    vec4 ambient;      // 16 bytes
    vec4 diffuse;      // 16 bytes
    vec4 specular;     // 16 bytes
    float shininess;   // 16 bytes (aligned)
    float metallic;    // 16 bytes (aligned)
    float roughness;   // 16 bytes (aligned)
    int useTexture;    // 16 bytes (aligned)
} materialData;

// Lighting UBO (binding point 2)
layout(std140, binding = 2) uniform LightingBlock {
    vec4 lightPositions[4];   // xyz = position, w = intensity
    vec4 lightColors[4];      // rgb = color, a = range
    int lightCount;
    float ambientStrength;
    vec2 _padding;
};

// Shadow uniforms
uniform sampler2D shadowMap0;
uniform sampler2D shadowMap1;
uniform sampler2D shadowMap2;
uniform sampler2D shadowMap3;
uniform mat4 shadowMatrices[4];
uniform bool shadowsEnabled;

// View position for specular
uniform vec3 viewPos;

// Shadow sampling with PCF
float sampleShadow(sampler2D shadowMap, vec4 shadowCoord, float bias) {
    if (shadowCoord.w <= 0.0) return 1.0;
    
    // Perspective divide and transform to [0,1] range
    vec3 projCoords = shadowCoord.xyz / shadowCoord.w;
    projCoords = projCoords * 0.5 + 0.5;
    
    // Outside shadow map bounds
    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || 
        projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 1.0;
    }
    
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += (projCoords.z - bias) > pcfDepth ? 0.0 : 1.0;
        }
    }
    return shadow / 9.0;
}

// Sample shadow for a specific light index
float sampleShadowForLight(int lightIndex, vec3 pos) {
    vec4 shadowCoord;
    float shadowFactor = 1.0;
    
    if (lightIndex == 0) {
        shadowCoord = shadowMatrices[0] * vec4(pos, 1.0);
        shadowFactor = sampleShadow(shadowMap0, shadowCoord, 0.001);
    } else if (lightIndex == 1) {
        shadowCoord = shadowMatrices[1] * vec4(pos, 1.0);
        shadowFactor = sampleShadow(shadowMap1, shadowCoord, 0.001);
    } else if (lightIndex == 2) {
        shadowCoord = shadowMatrices[2] * vec4(pos, 1.0);
        shadowFactor = sampleShadow(shadowMap2, shadowCoord, 0.001);
    } else if (lightIndex == 3) {
        shadowCoord = shadowMatrices[3] * vec4(pos, 1.0);
        shadowFactor = sampleShadow(shadowMap3, shadowCoord, 0.001);
    }
    
    // Map shadow [0,1] to [0.3,1] for softer look
    return shadowFactor * 0.7 + 0.3;
}

// Simplified silk-like rendering with multi-light and per-light shadows
void main() {
    vec3 normal = normalize(fragmentNormal);
    vec3 viewDir = normalize(viewPos - worldPos);
    
    // Tangent-based anisotropy (for silk effect)
    vec3 tangent = normalize(cross(normal, vec3(0.0, 1.0, 0.0)));
    vec3 bitangent = cross(normal, tangent);
    
    // Ambient
    vec3 ambient = materialData.diffuse.rgb * ambientStrength;
    
    // Accumulate lighting from all lights with per-light shadows
    vec3 diffuseAccum = vec3(0.0);
    vec3 specularAccum = vec3(0.0);
    
    for (int i = 0; i < lightCount && i < 4; ++i) {
        vec3 lightPos = lightPositions[i].xyz;
        float intensity = lightPositions[i].w;
        vec3 lightColor = lightColors[i].rgb;
        
        vec3 lightDir = normalize(lightPos - worldPos);
        
        // Calculate shadow for this light
        float shadow = 1.0;
        if (shadowsEnabled) {
            shadow = sampleShadowForLight(i, worldPos);
        }
        
        // Diffuse
        float diff = max(dot(normal, lightDir), 0.0);
        diffuseAccum += lightColor * materialData.diffuse.rgb * diff * intensity * 0.7 * shadow;
        
        // Anisotropic specular
        vec3 halfVec = normalize(lightDir + viewDir);
        float dotTH = dot(tangent, halfVec);
        float dotBH = dot(bitangent, halfVec);
        float aniso = sqrt(max(0.0, 1.0 - dotTH * dotTH - dotBH * dotBH));
        float spec = pow(aniso, materialData.shininess);
        specularAccum += lightColor * materialData.specular.rgb * spec * intensity * 0.6 * shadow;
    }
    
    // Sheen effect (view-dependent rim lighting)
    float sheen = pow(1.0 - max(dot(normal, viewDir), 0.0), 3.0);
    vec3 sheenColor = materialData.specular.rgb * 0.3 * sheen;
    
    vec3 color = ambient + diffuseAccum + specularAccum + sheenColor;
    fragColor = vec4(color, 1.0);
}
