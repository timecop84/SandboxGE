#version 460 core

// Inputs from vertex shader
in vec3 fragmentNormal;
in vec3 worldPos;
in vec3 vPosition;
in vec2 TexCoords;

// Output
out vec4 fragColor;

// Material UBO (binding point 1) - matches C++ MaterialUBO structure
layout(std140, binding = 1) uniform MaterialBlock {
    vec4 ambient;      // 16 bytes
    vec4 diffuse;      // 16 bytes
    vec4 specular;     // 16 bytes
    vec4 shininess;    // 16 bytes (aligned)
    vec4 metallic;     // 16 bytes (aligned)
    vec4 roughness;    // 16 bytes (aligned)
    ivec4 useTexture;  // 16 bytes (aligned)
} materialData;

// Lighting UBO (binding point 2)
layout(std140, binding = 2) uniform LightingBlock {
    vec4 lightPositions[4];   // xyz = position, w = intensity
    vec4 lightColors[4];      // rgb = color, a = range
    ivec4 lightCount;
    vec4 ambientStrength;
};

// Shadow uniforms
uniform sampler2D shadowMap0;
uniform sampler2D shadowMap1;
uniform sampler2D shadowMap2;
uniform sampler2D shadowMap3;
uniform sampler2D texture_diffuse;
uniform mat4 shadowMatrices[4];
uniform bool shadowsEnabled;
uniform float shadowBias;
uniform float shadowSoftness;
uniform int useCascades;
uniform int cascadeCount;
uniform float cascadeSplits[4];
uniform int debugCascades;
uniform int lightCastsShadow[4];
uniform int lightShadowMapIndex[4];
uniform int lightCascadeStart[4];
uniform int lightCascadeCount[4];

// View position for specular
uniform vec3 viewPos;

// Improved shadow sampling with area light approximation
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
    
    // Penumbra size based on distance (area light approximation)
    float receiverDepth = projCoords.z;
    float penumbraSize = lightSize * (receiverDepth / (1.0 - receiverDepth + 0.001));
    penumbraSize *= max(1.0, shadowSoftness);
    penumbraSize = clamp(penumbraSize, 1.0, 4.0);
    
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

// Sample shadow for a specific light index with area light size
float sampleShadowForLight(int lightIndex, vec3 pos, float lightSize) {
    vec4 shadowCoord;
    float shadowFactor = 1.0;
    
    if (lightIndex == 0) {
        shadowCoord = shadowMatrices[0] * vec4(pos, 1.0);
        shadowFactor = sampleShadow(shadowMap0, shadowCoord, shadowBias, lightSize);
    } else if (lightIndex == 1) {
        shadowCoord = shadowMatrices[1] * vec4(pos, 1.0);
        shadowFactor = sampleShadow(shadowMap1, shadowCoord, shadowBias, lightSize);
    } else if (lightIndex == 2) {
        shadowCoord = shadowMatrices[2] * vec4(pos, 1.0);
        shadowFactor = sampleShadow(shadowMap2, shadowCoord, shadowBias, lightSize);
    } else if (lightIndex == 3) {
        shadowCoord = shadowMatrices[3] * vec4(pos, 1.0);
        shadowFactor = sampleShadow(shadowMap3, shadowCoord, shadowBias, lightSize);
    }
    
    // Softer shadow transition
    return shadowFactor * 0.8 + 0.2;
}

float sampleShadowByIndex(int index, vec3 pos, float lightSize) {
    if (index == 0) return sampleShadowForLight(0, pos, lightSize);
    if (index == 1) return sampleShadowForLight(1, pos, lightSize);
    if (index == 2) return sampleShadowForLight(2, pos, lightSize);
    if (index == 3) return sampleShadowForLight(3, pos, lightSize);
    return 1.0;
}

float sampleCascadeShadow(int startIndex, int count, vec3 pos, float lightSize) {
    float minShadow = 1.0;
    int cCount = clamp(count, 1, 4);
    for (int i = 0; i < cCount; ++i) {
        int mapIndex = startIndex + i;
        float shadow = sampleShadowByIndex(mapIndex, pos, lightSize);
        minShadow = min(minShadow, shadow);
    }
    return minShadow;
}

// Silk shader with physically-based improvements
void main() {
    vec3 normal = normalize(fragmentNormal);
    vec3 viewDir = normalize(viewPos - worldPos);

    int lightCountN = lightCount.x;
    float ambientStrengthN = ambientStrength.x;
    
    // Tangent-based anisotropy (for silk effect)
    vec3 tangent = normalize(cross(normal, vec3(0.0, 1.0, 0.0)));
    vec3 bitangent = cross(normal, tangent);
    
    vec3 albedo = materialData.diffuse.rgb;
    if (materialData.useTexture.x != 0) {
        albedo *= texture(texture_diffuse, TexCoords).rgb;
    }

    // Ambient with color bleeding (silk tends to have rich colors)
    vec3 colorBleeding = albedo * ambientStrengthN * 0.4;
    vec3 ambient = (albedo + colorBleeding) * ambientStrengthN;
    
    // Accumulate lighting from all lights with per-light shadows
    vec3 diffuseAccum = vec3(0.0);
    vec3 specularAccum = vec3(0.0);

    if (debugCascades != 0 && useCascades != 0) {
        float viewDepth = -vPosition.z;
        int count = cascadeCount > 0 ? cascadeCount : 1;
        if (count > 4) count = 4;
        int cascadeIndex = count - 1;
        for (int i = 0; i < count; ++i) {
            if (viewDepth <= cascadeSplits[i]) {
                cascadeIndex = i;
                break;
            }
        }
        vec3 debugColor = vec3(1.0, 0.0, 0.0);
        if (cascadeIndex == 1) debugColor = vec3(0.0, 1.0, 0.0);
        else if (cascadeIndex == 2) debugColor = vec3(0.0, 0.2, 1.0);
        else if (cascadeIndex == 3) debugColor = vec3(1.0, 1.0, 0.0);
        fragColor = vec4(debugColor, 1.0);
        return;
    }
    
    for (int i = 0; i < lightCountN && i < 4; ++i) {
        vec3 lightPos = lightPositions[i].xyz;
        float lightIntensity = lightPositions[i].w;
        vec3 lightColor = lightColors[i].rgb;
        
        // Distance attenuation (balanced for visual quality)
        float distance = length(lightPos - worldPos);
        float attenuation = lightIntensity / (1.0 + 0.007 * distance + 0.0002 * distance * distance);
        
        vec3 lightDir = normalize(lightPos - worldPos);
        
        // Calculate shadow with area light size
        float lightSize = 0.5 + lightIntensity * 0.5;
        float shadow = 1.0;
        if (shadowsEnabled && lightCastsShadow[i] != 0) {
            float shadowFactor = 1.0;
            if (useCascades != 0 && lightCascadeCount[i] > 1) {
                shadowFactor = sampleCascadeShadow(lightCascadeStart[i], lightCascadeCount[i], worldPos, lightSize);
            } else {
                int mapIndex = lightShadowMapIndex[i];
                shadowFactor = sampleShadowByIndex(mapIndex, worldPos, lightSize);
            }
            shadow = shadowFactor;
            
            // Ambient bounce from shadowed areas (fake GI)
            ambient += albedo * lightColor * attenuation * (1.0 - shadowFactor) * 0.15;
        }
        
        // Diffuse with physical attenuation
        float diff = max(dot(normal, lightDir), 0.0);
        diffuseAccum += lightColor * albedo * diff * attenuation * 0.7 * shadow;
        
        // Anisotropic specular (silk highlight)
        vec3 halfVec = normalize(lightDir + viewDir);
        float dotTH = dot(tangent, halfVec);
        float dotBH = dot(bitangent, halfVec);
        float aniso = sqrt(max(0.0, 1.0 - dotTH * dotTH - dotBH * dotBH));
        float spec = pow(aniso, materialData.shininess.x);
        specularAccum += lightColor * materialData.specular.rgb * spec * attenuation * 0.6 * shadow;
    }
    
    // Sheen effect (view-dependent rim lighting typical of silk)
    float sheen = pow(1.0 - max(dot(normal, viewDir), 0.0), 3.0);
    vec3 sheenColor = materialData.specular.rgb * 0.3 * sheen;
    
    vec3 color = ambient + diffuseAccum + specularAccum + sheenColor;
    fragColor = vec4(color, 1.0);
}
