#version 460 core

// Inputs from vertex shader
in vec3 fragmentNormal;
in vec3 worldPos;
in vec3 viewDir;

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

// View position for correct view direction
uniform vec3 viewPos;

const float PI = 3.14159265359;

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
    
    // Penumbra size (area light approximation)
    float receiverDepth = projCoords.z;
    float penumbraSize = lightSize * (receiverDepth / (1.0 - receiverDepth + 0.001));
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

// GGX/Trowbridge-Reitz normal distribution
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    
    return a2 / max(denom, 0.0001);
}

// Schlick-GGX geometry function
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    
    return NdotV / (NdotV * (1.0 - k) + k);
}

// Smith's method
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    
    return ggx1 * ggx2;
}

// Fresnel-Schlick approximation
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Sample shadow for a specific light index with area light size
float sampleShadowForLight(int lightIndex, vec3 pos, float lightSize) {
    vec4 shadowCoord;
    float shadowFactor = 1.0;
    
    if (lightIndex == 0) {
        shadowCoord = shadowMatrices[0] * vec4(pos, 1.0);
        shadowFactor = sampleShadow(shadowMap0, shadowCoord, 0.001, lightSize);
    } else if (lightIndex == 1) {
        shadowCoord = shadowMatrices[1] * vec4(pos, 1.0);
        shadowFactor = sampleShadow(shadowMap1, shadowCoord, 0.001, lightSize);
    } else if (lightIndex == 2) {
        shadowCoord = shadowMatrices[2] * vec4(pos, 1.0);
        shadowFactor = sampleShadow(shadowMap2, shadowCoord, 0.001, lightSize);
    } else if (lightIndex == 3) {
        shadowCoord = shadowMatrices[3] * vec4(pos, 1.0);
        shadowFactor = sampleShadow(shadowMap3, shadowCoord, 0.001, lightSize);
    }
    
    // Softer shadow transition
    return shadowFactor * 0.8 + 0.2;
}

void main() {
    vec3 N = normalize(fragmentNormal);
    vec3 V = normalize(viewPos - worldPos);
    
    // Material properties from UBO
    vec3 albedo = materialData.diffuse.rgb;
    float metal = materialData.metallic;
    float rough = max(materialData.roughness, 0.04);
    
    // Base reflectivity
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metal);
    
    // Ambient with color bleeding for non-metals
    vec3 colorBleeding = albedo * (1.0 - metal) * ambientStrength * 0.3;
    vec3 ambient = (albedo + colorBleeding) * ambientStrength;
    
    // Accumulate lighting from all lights with per-light shadows
    vec3 Lo = vec3(0.0);
    
    for (int i = 0; i < lightCount && i < 4; ++i) {
        vec3 lightPos = lightPositions[i].xyz;
        float lightIntensity = lightPositions[i].w;
        vec3 lightColor = lightColors[i].rgb;
        
        vec3 L = normalize(lightPos - worldPos);
        vec3 H = normalize(V + L);
        
        // Distance attenuation (balanced for visual quality)
        float distance = length(lightPos - worldPos);
        float attenuation = lightIntensity / (1.0 + 0.007 * distance + 0.0002 * distance * distance);
        
        // Calculate shadow with area light approximation
        float lightSize = 0.5 + lightIntensity * 0.5;
        float shadow = 1.0;
        if (shadowsEnabled) {
            float shadowFactor = sampleShadowForLight(i, worldPos, lightSize);
            shadow = shadowFactor;
            
            // Ambient bounce: Even in shadow, light scatters (fake GI)
            ambient += albedo * lightColor * attenuation * (1.0 - shadowFactor) * 0.1;
        }
        
        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, rough);
        float G = GeometrySmith(N, V, L, rough);
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        
        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specularBRDF = numerator / denominator;
        
        // Energy conservation
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metal;
        
        // Diffuse
        float NdotL = max(dot(N, L), 0.0);
        vec3 diffuseBRDF = kD * albedo / PI;
        
        // Combine with physical attenuation and per-light shadow
        Lo += (diffuseBRDF + specularBRDF) * lightColor * NdotL * attenuation * shadow;
    }
    
    vec3 color = ambient + Lo;
    
    // Tone mapping (Reinhard)
    color = color / (color + vec3(1.0));
    
    // Gamma correction
    color = pow(color, vec3(1.0/2.2));
    
    fragColor = vec4(color, 1.0);
}
