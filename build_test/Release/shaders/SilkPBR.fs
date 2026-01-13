#version 460 core
// Inputs from vertex shader
in vec3 fragmentNormal;
in vec3 fragmentTangent;
in vec3 fragmentBitangent;
in vec3 worldPos;
in vec3 vPosition;
in vec2 fragUV;
in vec3 eyeDirection;

out vec4 fragColour;

// Multi-shadow support
const int MAX_SHADOW_LIGHTS = 4;
const int MAX_LIGHTS = 8;
uniform sampler2DShadow shadowMaps[MAX_SHADOW_LIGHTS];
uniform mat4 lightSpaceMatrices[MAX_SHADOW_LIGHTS];
uniform float lightIntensities[MAX_SHADOW_LIGHTS];
uniform int numShadowLights;
uniform float shadowMapSize;
uniform float shadowBias;
uniform float shadowSoftness;
uniform int shadowEnabled;

// Material
struct Material {
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
    float shininess;
};
uniform Material material;

// Main light
struct Light {
    vec4 position;
    vec3 direction;
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
    float constantAttenuation;
    float linearAttenuation;
    float quadraticAttenuation;
};
uniform Light light;

// Additional lights array
uniform int numLights;
uniform vec3 lightPositions[MAX_LIGHTS];
uniform vec3 lightColors[MAX_LIGHTS];
uniform float lightIntensitiesExtra[MAX_LIGHTS];

// PBR parameters
uniform float roughness;        // 0.0 = smooth, 1.0 = rough
uniform float metallic;         // 0.0 = dielectric, 1.0 = metal (typically 0 for fabric)
uniform float anisotropy;       // -1.0 to 1.0 anisotropic stretch
uniform float sheenIntensity;   // Sheen layer strength
uniform vec3 sheenColor;        // Sheen tint
uniform float subsurfaceAmount; // SSS strength
uniform vec3 subsurfaceColor;   // SSS color
uniform float weaveScale;       // Micro-detail weave pattern

// Checker pattern
uniform float checkerScale;
uniform vec3 checkerColor1;
uniform vec3 checkerColor2;

// AO
uniform float aoStrength;
uniform vec3 aoGroundColor;

uniform vec3 viewerPos;
uniform vec3 lightWorldPos;  // Light position in world space for PBR calculations

const float PI = 3.14159265359;

// ============================================================================
// PBR Helper Functions
// ============================================================================

// Fresnel-Schlick approximation
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Fresnel with roughness for ambient
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// GGX/Trowbridge-Reitz Normal Distribution Function
float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    
    return num / denom;
}

// Anisotropic GGX Distribution (for silk fiber direction)
float distributionGGXAnisotropic(vec3 N, vec3 H, vec3 T, vec3 B, float roughness, float aniso) {
    float at = max(roughness * (1.0 + aniso), 0.001);
    float ab = max(roughness * (1.0 - aniso), 0.001);
    
    float TdotH = dot(T, H);
    float BdotH = dot(B, H);
    float NdotH = max(dot(N, H), 0.0);
    
    float a2 = at * ab;
    vec3 v = vec3(ab * TdotH, at * BdotH, a2 * NdotH);
    float v2 = dot(v, v);
    float w2 = a2 / v2;
    
    return a2 * w2 * w2 / PI;
}

// Smith's Geometry Function (Schlick-GGX)
float geometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    
    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    
    return num / denom;
}

// Smith's method for combined geometry
float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = geometrySchlickGGX(NdotV, roughness);
    float ggx1 = geometrySchlickGGX(NdotL, roughness);
    
    return ggx1 * ggx2;
}

// Charlie Sheen Distribution (for fabric sheen layer)
float distributionCharlie(float NdotH, float roughness) {
    float invAlpha = 1.0 / roughness;
    float cos2h = NdotH * NdotH;
    float sin2h = max(1.0 - cos2h, 0.0078125);
    return (2.0 + invAlpha) * pow(sin2h, invAlpha * 0.5) / (2.0 * PI);
}

// Charlie Visibility term
float visibilityCharlie(float NdotV, float NdotL, float roughness) {
    float v = NdotV + sqrt(roughness + (1.0 - roughness) * NdotV * NdotV);
    float l = NdotL + sqrt(roughness + (1.0 - roughness) * NdotL * NdotL);
    return 1.0 / (v * l);
}

// Subsurface scattering approximation
vec3 subsurfaceScattering(vec3 N, vec3 L, vec3 V, vec3 albedo, vec3 sssColor, float amount) {
    // Wrap lighting
    float wrap = 0.5;
    float NdotL = dot(N, L);
    float wrapDiffuse = max(0.0, (NdotL + wrap) / (1.0 + wrap));
    
    // Back-lighting (light passing through thin fabric)
    float backLight = max(0.0, dot(V, -L)) * 0.5;
    
    // View-dependent translucency
    float VdotN = 1.0 - max(0.0, dot(V, N));
    float translucency = pow(VdotN, 2.0) * backLight;
    
    vec3 sss = sssColor * (wrapDiffuse * 0.3 + translucency) * amount;
    return mix(albedo * wrapDiffuse, albedo + sss, amount * 0.5);
}

// Weave pattern micro-detail
float weavePattern(vec2 uv, float scale) {
    if (scale <= 0.0) return 1.0;
    vec2 weavePt = uv * scale;
    float warp = sin(weavePt.x * 6.28318) * 0.5 + 0.5;
    float weft = sin(weavePt.y * 6.28318) * 0.5 + 0.5;
    float pattern = mix(warp, weft, step(0.5, fract(weavePt.x + weavePt.y)));
    return mix(0.9, 1.0, pattern);
}

// Checker pattern
vec3 checkerPattern(vec2 uv, float scale, vec3 color1, vec3 color2) {
    if (scale <= 0.0) return vec3(1.0);
    vec2 checker = floor(uv * scale);
    float pattern = mod(checker.x + checker.y, 2.0);
    return mix(color1, color2, pattern);
}

// Ambient occlusion
float computeAO(vec3 N, vec3 V) {
    float hemisphereAO = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
    float cavityAO = clamp(dot(N, V) * 0.7 + 0.3, 0.0, 1.0);
    float edgeAO = clamp(1.0 - pow(1.0 - abs(dot(N, V)), 2.0), 0.3, 1.0);
    return mix(1.0, hemisphereAO * cavityAO * edgeAO, aoStrength);
}

// ============================================================================
// Shadow Functions
// ============================================================================

float calculateShadowForMap(sampler2DShadow sMap, vec4 lsPos, vec3 normal, vec3 lightDir) {
    vec3 projCoords = lsPos.xyz / lsPos.w;
    projCoords = projCoords * 0.5 + 0.5;
    
    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0)
        return 1.0;
    
    float bias = max(shadowBias * (1.0 - dot(normal, lightDir)), shadowBias * 0.1);
    float currentDepth = projCoords.z - bias;
    
    float shadow = 0.0;
    float mapSize = shadowMapSize > 0.0 ? shadowMapSize : 4096.0;
    vec2 texelSize = vec2(1.0 / mapSize);
    
    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) {
            vec3 sampleCoord = vec3(projCoords.xy + vec2(x, y) * texelSize * shadowSoftness, currentDepth);
            shadow += texture(sMap, sampleCoord);
        }
    }
    return shadow / 25.0;
}

float calculateMultiShadow(vec3 normal, vec3 lightDir) {
    if (shadowEnabled == 0) return 1.0;
    if (numShadowLights <= 0) return 1.0;
    
    float totalShadowContrib = 0.0;
    float totalIntensity = 0.0;
    vec4 wPos = vec4(worldPos, 1.0);
    
    for (int i = 0; i < numShadowLights && i < MAX_SHADOW_LIGHTS; ++i) {
        vec4 lsPos = lightSpaceMatrices[i] * wPos;
        float rawShadow = calculateShadowForMap(shadowMaps[i], lsPos, normal, lightDir);
        float intensity = lightIntensities[i];
        
        float shadowContrib = (1.0 - rawShadow) * intensity;
        totalShadowContrib += shadowContrib;
        totalIntensity += intensity;
    }
    
    if (totalIntensity > 0.0) {
        float avgShadowContrib = totalShadowContrib / totalIntensity;
        return 1.0 - clamp(avgShadowContrib, 0.0, 1.0);
    }
    
    return 1.0;
}

// ============================================================================
// Main PBR Calculation
// ============================================================================

vec3 calculatePBR(vec3 N, vec3 V, vec3 L, vec3 H, vec3 T, vec3 B, 
                  vec3 albedo, vec3 radiance, float shadow) {
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.001);
    float NdotH = max(dot(N, H), 0.0);
    float HdotV = max(dot(H, V), 0.0);
    
    // Base reflectivity (F0) - for fabric, typically low (~0.04)
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);
    
    // Cook-Torrance BRDF
    float effectiveRoughness = max(roughness, 0.04);
    
    // Use anisotropic distribution for silk
    float D = distributionGGXAnisotropic(N, H, T, B, effectiveRoughness, anisotropy);
    float G = geometrySmith(N, V, L, effectiveRoughness);
    vec3 F = fresnelSchlick(HdotV, F0);
    
    vec3 numerator = D * G * F;
    float denominator = 4.0 * NdotV * NdotL + 0.0001;
    vec3 specular = numerator / denominator;
    
    // Energy conservation
    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
    
    // Diffuse with SSS
    vec3 effectiveSSS = length(subsurfaceColor) > 0.01 ? subsurfaceColor : albedo * vec3(1.2, 0.9, 0.85);
    vec3 diffuse = subsurfaceScattering(N, L, V, albedo, effectiveSSS, subsurfaceAmount);
    
    // Sheen layer (Charlie model for fabric)
    float sheenRoughness = max(0.3, 1.0 - sheenIntensity * 0.5);
    float Ds = distributionCharlie(NdotH, sheenRoughness);
    float Vs = visibilityCharlie(NdotV, NdotL, sheenRoughness);
    vec3 effectiveSheenColor = length(sheenColor) > 0.01 ? sheenColor : vec3(1.0);
    vec3 sheen = effectiveSheenColor * sheenIntensity * Ds * Vs * NdotL;
    
    // Combine
    vec3 Lo = (kD * diffuse / PI + specular + sheen) * radiance * NdotL * shadow;
    
    return Lo;
}

void main() {
    // Build TBN frame
    vec3 N = normalize(fragmentNormal);
    vec3 T = normalize(fragmentTangent);
    vec3 B = normalize(fragmentBitangent);
    vec3 V = normalize(viewerPos - worldPos);
    
    // Apply weave micro-detail to normal
    float weaveVar = weavePattern(fragUV, weaveScale);
    
    // Get base albedo with checker pattern
    vec3 checkerMod = checkerPattern(fragUV, checkerScale, checkerColor1, checkerColor2);
    vec3 albedo = material.diffuse.rgb * checkerMod * weaveVar;
    
    // Calculate shadow
    vec3 mainLightPos = lightWorldPos;  // Use world-space light position
    vec3 L0 = normalize(mainLightPos - worldPos);
    float shadow = calculateMultiShadow(N, L0);
    
    // Ambient occlusion
    float ao = computeAO(N, V);
    vec3 aoTint = mix(aoGroundColor, vec3(1.0), ao);
    
    // Main light contribution
    vec3 L = L0;
    vec3 H = normalize(V + L);
    float distance = length(mainLightPos - worldPos);
    float attenuation = 1.0 / (light.constantAttenuation + 
                               light.linearAttenuation * distance + 
                               light.quadraticAttenuation * distance * distance);
    vec3 radiance = light.diffuse.rgb * attenuation;
    
    vec3 Lo = calculatePBR(N, V, L, H, T, B, albedo, radiance, shadow);
    
    // Additional lights
    for (int i = 0; i < numLights && i < MAX_LIGHTS; ++i) {
        vec3 lPos = lightPositions[i];
        vec3 lColor = lightColors[i];
        float lIntensity = lightIntensitiesExtra[i];
        
        vec3 Li = normalize(lPos - worldPos);
        vec3 Hi = normalize(V + Li);
        float dist = length(lPos - worldPos);
        float atten = lIntensity / (1.0 + 0.01 * dist + 0.001 * dist * dist);
        vec3 rad = lColor * atten;
        
        Lo += calculatePBR(N, V, Li, Hi, T, B, albedo, rad, shadow);
    }
    
    // Ambient
    vec3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), vec3(0.04), roughness);
    vec3 ambient = light.ambient.rgb * albedo * (1.0 - F * 0.5);
    
    // Final color
    vec3 color = ambient + Lo;
    color *= aoTint;
    
    // Tone mapping (Reinhard)
    color = color / (color + vec3(1.0));
    
    // Gamma correction
    color = pow(color, vec3(1.0 / 2.2));
    
    fragColour = vec4(color, material.diffuse.a);
}
