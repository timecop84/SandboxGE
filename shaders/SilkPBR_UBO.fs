#version 460 core

// Inputs from vertex shader
in vec3 fragmentNormal;
in vec3 worldPos;
in vec3 viewDir;

// Output
out vec4 fragColor;

// Material UBO (binding point 1)
layout(std140, binding = 1) uniform MaterialBlock {
    vec4 baseColor;        // RGB + metallic
    vec4 properties;       // roughness, ao, emission, useTexture
    mat4 textureTransform;
};

const float PI = 3.14159265359;

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

void main() {
    vec3 N = normalize(fragmentNormal);
    vec3 V = normalize(viewDir);
    
    // Light properties
    vec3 L = normalize(vec3(1.0, 1.0, 1.0));
    vec3 H = normalize(V + L);
    vec3 lightColor = vec3(1.0);
    
    // Material properties from UBO
    float metallic = baseColor.a;
    float roughness = properties.x;
    float ao = properties.y;
    
    // Base reflectivity (for dielectrics use 0.04, lerp with albedo for metals)
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, baseColor.rgb, metallic);
    
    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;
    
    // Energy conservation
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;
    
    // Diffuse
    float NdotL = max(dot(N, L), 0.0);
    vec3 diffuse = kD * baseColor.rgb / PI;
    
    // Combine
    vec3 Lo = (diffuse + specular) * lightColor * NdotL;
    
    // Ambient
    vec3 ambient = vec3(0.03) * baseColor.rgb * ao;
    
    vec3 color = ambient + Lo;
    
    // Tone mapping
    color = color / (color + vec3(1.0));
    
    // Gamma correction
    color = pow(color, vec3(1.0/2.2));
    
    fragColor = vec4(color, 1.0);
}
