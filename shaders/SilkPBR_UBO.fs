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

void main() {
    vec3 N = normalize(fragmentNormal);
    vec3 V = normalize(viewPos - worldPos);
    
    // Material properties from UBO - using materialData instance
    vec3 albedo = materialData.diffuse.rgb;
    float metal = materialData.metallic;
    float rough = max(materialData.roughness, 0.04);  // Clamp to avoid division issues
    
    // Base reflectivity (for dielectrics use 0.04, lerp with albedo for metals)
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metal);
    
    // Accumulate lighting from all lights with per-light shadows
    vec3 Lo = vec3(0.0);
    
    for (int i = 0; i < lightCount && i < 4; ++i) {
        vec3 lightPos = lightPositions[i].xyz;
        float intensity = lightPositions[i].w;
        vec3 lightColor = lightColors[i].rgb;
        
        vec3 L = normalize(lightPos - worldPos);
        vec3 H = normalize(V + L);
        
        // Calculate shadow for this light
        float shadow = 1.0;
        if (shadowsEnabled) {
            shadow = sampleShadowForLight(i, worldPos);
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
        
        // Combine for this light with per-light shadow
        Lo += (diffuseBRDF + specularBRDF) * lightColor * NdotL * intensity * shadow;
    }
    
    // Ambient (affected by AO if we had it, use 1.0 for now)
    vec3 ambient = ambientStrength * albedo;
    
    vec3 color = ambient + Lo;
    
    // Tone mapping (Reinhard)
    color = color / (color + vec3(1.0));
    
    // Gamma correction
    color = pow(color, vec3(1.0/2.2));
    
    fragColor = vec4(color, 1.0);
}
