#version 460 core

// Inputs from vertex shader
in vec3 fragmentNormal;
in vec3 worldPos;

// Output
out vec4 fragColor;

// Material UBO (binding point 1) - Phong materials use ambient/diffuse/specular
layout(std140, binding = 1) uniform MaterialBlock {
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
    float shininess;
    float metallic;
    float roughness;
    int useTexture;
} materialData;

// Lighting UBO (binding point 2)
layout(std140, binding = 2) uniform LightingBlock {
    vec4 lightPositions[4];
    vec4 lightColors[4];
    int lightCount;
    float ambientStrength;
    vec2 _padding;
};

// Shadow maps
uniform sampler2D shadowMap0;
uniform sampler2D shadowMap1;
uniform sampler2D shadowMap2;
uniform sampler2D shadowMap3;
uniform mat4 shadowMatrices[4];
uniform bool shadowsEnabled;

// View position for specular
uniform vec3 viewPos;

// Improved shadow sampling with area light approximation (PCSS-like)
float sampleShadow(sampler2D shadowMap, vec4 shadowCoord, float bias, float lightSize) {
    if (shadowCoord.w <= 0.0) return 1.0;
    
    // Perspective divide and transform to [0,1] range
    vec3 projCoords = shadowCoord.xyz / shadowCoord.w;
    projCoords = projCoords * 0.5 + 0.5;
    
    // Outside shadow map bounds
    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || 
        projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 1.0;
    }
    
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    
    // Estimate penumbra size based on distance (fake area light)
    float receiverDepth = projCoords.z;
    float penumbraSize = lightSize * (receiverDepth / (1.0 - receiverDepth + 0.001));
    penumbraSize = clamp(penumbraSize, 1.0, 4.0);
    
    // Larger PCF kernel for softer shadows
    float shadow = 0.0;
    int samples = 0;
    int radius = int(penumbraSize);
    for (int x = -radius; x <= radius; ++x) {
        for (int y = -radius; y <= radius; ++y) {
            vec2 offset = vec2(x, y) * texelSize;
            float pcfDepth = texture(shadowMap, projCoords.xy + offset).r;
            shadow += (projCoords.z - bias) > pcfDepth ? 0.0 : 1.0;
            samples++;
        }
    }
    return shadow / float(samples);
}

// Phong lighting with physically-based improvements
void main() {
    vec3 normal = normalize(fragmentNormal);
    vec3 viewDir = normalize(viewPos - worldPos);
    
    // Fake ambient bounce: Use material color to simulate light bouncing off nearby surfaces
    // More saturated materials contribute more color bleeding
    vec3 colorBleeding = materialData.diffuse.rgb * ambientStrength * 0.3;
    
    // Ambient component with color bleeding
    vec3 ambient = (materialData.ambient.rgb + colorBleeding) * ambientStrength;
    
    // Accumulate lighting from all lights
    vec3 diffuse = vec3(0.0);
    vec3 specular = vec3(0.0);
    
    for (int i = 0; i < lightCount; ++i) {
        vec3 lightPos = lightPositions[i].xyz;
        vec3 lightColor = lightColors[i].rgb;
        float lightIntensity = lightPositions[i].w;
        
        // Distance attenuation (balanced for visual quality)
        float distance = length(lightPos - worldPos);
        float attenuation = lightIntensity / (1.0 + 0.007 * distance + 0.0002 * distance * distance);
        
        // Calculate shadow for this light (up to 4 lights supported)
        // Use area light approximation with size based on light intensity
        float lightSize = 0.5 + lightIntensity * 0.5; // Larger lights = softer shadows
        float shadow = 1.0;
        if (shadowsEnabled && i < 4) {
            vec4 shadowCoord;
            if (i == 0) shadowCoord = shadowMatrices[0] * vec4(worldPos, 1.0);
            else if (i == 1) shadowCoord = shadowMatrices[1] * vec4(worldPos, 1.0);
            else if (i == 2) shadowCoord = shadowMatrices[2] * vec4(worldPos, 1.0);
            else shadowCoord = shadowMatrices[3] * vec4(worldPos, 1.0);
            
            float shadowFactor;
            if (i == 0) shadowFactor = sampleShadow(shadowMap0, shadowCoord, 0.001, lightSize);
            else if (i == 1) shadowFactor = sampleShadow(shadowMap1, shadowCoord, 0.001, lightSize);
            else if (i == 2) shadowFactor = sampleShadow(shadowMap2, shadowCoord, 0.001, lightSize);
            else shadowFactor = sampleShadow(shadowMap3, shadowCoord, 0.001, lightSize);
            
            // Softer shadow transition
            shadow = shadowFactor * 0.8 + 0.2;
            
            // Ambient bounce: Light scatters even in shadow (fake GI)
            ambient += materialData.diffuse.rgb * lightColor * attenuation * (1.0 - shadowFactor) * 0.1;
        }
        
        vec3 lightDir = normalize(lightPos - worldPos);
        
        // Diffuse component with attenuation
        float diff = max(dot(normal, lightDir), 0.0);
        diffuse += lightColor * materialData.diffuse.rgb * diff * attenuation * shadow;
        
        // Specular component (Blinn-Phong) with physical attenuation
        vec3 halfVec = normalize(lightDir + viewDir);
        float spec = pow(max(dot(normal, halfVec), 0.0), materialData.shininess);
        specular += lightColor * materialData.specular.rgb * spec * attenuation * shadow;
    }
    
    // Combine components
    vec3 color = ambient + diffuse + specular;
    fragColor = vec4(color, 1.0);
}
