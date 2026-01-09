#version 460 core
/// @brief Silk/fabric fragment shader with anisotropic specular, SSS, and multi-shadow support

/// @brief the vertex normal
in vec3 fragmentNormal;
/// @brief tangent for anisotropic lighting
in vec3 fragmentTangent;
/// @brief bitangent for anisotropic lighting
in vec3 fragmentBitangent;
/// @brief our output fragment colour
out vec4 fragColour;

in vec3 lightDir;
in vec3 halfVector;
in vec3 eyeDirection;
in vec3 vPosition;
in vec2 fragUV;
in vec4 fragPosLightSpace;
in vec3 worldPos;

// Multi-shadow support
const int MAX_SHADOW_LIGHTS = 4;
uniform sampler2DShadow shadowMaps[MAX_SHADOW_LIGHTS];
uniform mat4 lightSpaceMatrices[MAX_SHADOW_LIGHTS];
uniform float lightIntensities[MAX_SHADOW_LIGHTS];
uniform int numShadowLights;
uniform float shadowMapSize;

// Shadow map (legacy)
uniform sampler2DShadow shadowMap;
uniform float shadowBias;
uniform float shadowSoftness;
uniform int shadowEnabled;

/// @brief material structure
struct Materials
{
  vec4 ambient;
  vec4 diffuse;
  vec4 specular;
  float shininess;
};

/// @brief light structure
struct Lights
{
    vec4 position;
    vec3 direction;
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
    float spotCosCutoff;
    float spotCosInnerCutoff;
    float spotExponent;
    float constantAttenuation;
    float linearAttenuation;
    float quadraticAttenuation;
};

uniform Materials material;
uniform Lights light;

// Silk-specific parameters
uniform float anisotropyU;     // Anisotropy along warp (U) direction
uniform float anisotropyV;     // Anisotropy along weft (V) direction
uniform float sheenIntensity;  // Silk sheen/rim light intensity
uniform float subsurfaceAmount;// SSS approximation amount
uniform vec3 subsurfaceColor;  // SSS color tint
uniform float weaveScale;      // Weave pattern scale
uniform float time;            // For subtle animation

// Checker pattern parameters
uniform float checkerScale;    // Checker pattern scale (0 = disabled)
uniform vec3 checkerColor1;    // Primary checker color
uniform vec3 checkerColor2;    // Secondary checker color

// Ambient occlusion parameters
uniform float aoStrength;      // 0 = off, 0.5 = subtle, 1.0 = strong
uniform vec3 aoGroundColor;    // Color tint for ground occlusion

// PCF shadow calculation
float calculateShadow(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir)
{
    if (shadowEnabled == 0) return 1.0;
    
    // Perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    
    // Transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    
    // Check if outside shadow map
    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0)
        return 1.0;
    
    // Slope-scaled bias
    float bias = max(shadowBias * (1.0 - dot(normal, lightDir)), shadowBias * 0.1);
    float currentDepth = projCoords.z - bias;
    
    // PCF filtering
    float shadow = 0.0;
    vec2 texelSize = vec2(1.0 / 2048.0); // Shadow map size
    int samples = int(shadowSoftness);
    samples = clamp(samples, 1, 4);
    
    for (int x = -samples; x <= samples; ++x) {
        for (int y = -samples; y <= samples; ++y) {
            vec3 sampleCoord = vec3(projCoords.xy + vec2(x, y) * texelSize, currentDepth);
            shadow += texture(shadowMap, sampleCoord);
        }
    }
    
    int totalSamples = (2 * samples + 1) * (2 * samples + 1);
    shadow /= float(totalSamples);
    
    return shadow;
}

// Calculate shadow for a specific shadow map
float calculateShadowForMap(sampler2DShadow sMap, vec4 lsPos, vec3 normal, vec3 lightDir)
{
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
            vec3 sampleCoord = vec3(projCoords.xy + vec2(x, y) * texelSize, currentDepth);
            shadow += texture(sMap, sampleCoord);
        }
    }
    shadow /= 25.0;
    return shadow;
}

// Calculate combined shadow from all shadow maps with intensity weighting
float calculateMultiShadow(vec3 normal, vec3 lightDir)
{
    if (shadowEnabled == 0) return 1.0;
    if (numShadowLights <= 0) return calculateShadow(fragPosLightSpace, normal, lightDir);
    
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

// Procedural checker pattern
vec3 checkerPattern(vec2 uv, float scale, vec3 color1, vec3 color2)
{
    if (scale <= 0.0) return vec3(1.0); // Return white multiplier if disabled
    vec2 checker = floor(uv * scale);
    float pattern = mod(checker.x + checker.y, 2.0);
    return mix(color1, color2, pattern);
}

// Kajiya-Kay anisotropic specular for hair/fabric
// T = tangent direction, H = half vector
float kajiyaKaySpecular(vec3 T, vec3 H, float exponent)
{
    float TdotH = dot(T, H);
    float sinTH = sqrt(max(0.0, 1.0 - TdotH * TdotH));
    return pow(sinTH, exponent);
}

// Subsurface scattering approximation (wrap diffuse + back-lighting)
vec3 subsurfaceScattering(vec3 N, vec3 L, vec3 V, vec3 albedo, vec3 sssColor, float sssAmount)
{
    // Wrap lighting for softer diffuse
    float wrap = 0.5;
    float NdotL = dot(N, L);
    float wrapDiffuse = max(0.0, (NdotL + wrap) / (1.0 + wrap));
    
    // Back-lighting simulation (light passing through thin fabric)
    float backLight = max(0.0, dot(V, -L)) * 0.5;
    
    // View-dependent translucency
    float VdotN = 1.0 - max(0.0, dot(V, N));
    float translucency = pow(VdotN, 2.0) * backLight;
    
    vec3 sss = sssColor * (wrapDiffuse * 0.3 + translucency) * sssAmount;
    return mix(albedo * wrapDiffuse, albedo + sss, sssAmount * 0.5);
}

// Simple weave pattern for micro-detail
float weavePattern(vec2 uv, float scale)
{
    vec2 weavePt = uv * scale;
    float warp = sin(weavePt.x * 6.28318) * 0.5 + 0.5;
    float weft = sin(weavePt.y * 6.28318) * 0.5 + 0.5;
    // Alternating weave pattern
    float pattern = mix(warp, weft, step(0.5, fract(weavePt.x + weavePt.y)));
    return mix(0.85, 1.0, pattern); // Subtle variation
}

// Fresnel-Schlick approximation for rim/sheen
float fresnelSchlick(float cosTheta, float F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// Ambient occlusion approximation
float computeAO(vec3 N, vec3 V)
{
    // Hemisphere occlusion: darken surfaces facing down
    float hemisphereAO = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
    
    // Cavity/crease darkening
    float cavityAO = clamp(dot(N, V) * 0.7 + 0.3, 0.0, 1.0);
    
    // Edge darkening for folds
    float edgeAO = clamp(1.0 - pow(1.0 - abs(dot(N, V)), 2.0), 0.3, 1.0);
    
    return mix(1.0, hemisphereAO * cavityAO * edgeAO, aoStrength);
}

vec4 silkLighting()
{
    vec3 N = normalize(fragmentNormal);
    vec3 T = normalize(fragmentTangent);
    vec3 B = normalize(fragmentBitangent);
    vec3 V = normalize(eyeDirection);
    vec3 L = normalize(lightDir);
    vec3 H = normalize(halfVector);
    
    // Basic lambertian for base
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.001);
    
    // Distance attenuation
    vec3 VP = vec3(light.position.xyz) - vPosition;
    float d = length(VP);
    float attenuation = 1.0 / (light.constantAttenuation +
                              light.linearAttenuation * d +
                              light.quadraticAttenuation * d * d);
    
    // === CHECKER PATTERN ===
    vec3 checkerMod = checkerPattern(fragUV, checkerScale, checkerColor1, checkerColor2);
    
    // === DIFFUSE with SSS ===
    vec3 albedo = material.diffuse.rgb * checkerMod;  // Apply checker pattern
    vec3 effectiveSSS = subsurfaceColor;
    if (length(subsurfaceColor) < 0.01) {
        // Default SSS color derived from diffuse
        effectiveSSS = albedo * vec3(1.2, 0.9, 0.85);
    }
    vec3 diffuse = subsurfaceScattering(N, L, V, albedo, effectiveSSS, subsurfaceAmount);
    
    // === ANISOTROPIC SPECULAR (Kajiya-Kay dual highlight) ===
    // Primary specular along warp (U) direction
    float specU = kajiyaKaySpecular(T, H, material.shininess * anisotropyU);
    // Secondary specular along weft (V) direction  
    float specV = kajiyaKaySpecular(B, H, material.shininess * anisotropyV);
    
    // Blend the two highlights for characteristic silk cross-sheen
    float anisoSpec = mix(specU, specV, 0.4) * 0.7 + max(specU, specV) * 0.3;
    vec3 specular = material.specular.rgb * light.specular.rgb * anisoSpec;
    
    // === SHEEN / RIM LIGHT ===
    // Silk has a distinctive rim glow due to fiber scattering
    float fresnel = fresnelSchlick(NdotV, 0.04);
    float rimLight = pow(1.0 - NdotV, 3.0);
    vec3 sheen = material.specular.rgb * sheenIntensity * (fresnel * 0.5 + rimLight * 0.5);
    
    // Add slight color shift to sheen (silk iridescence approximation)
    float hueShift = fragUV.x * 0.1 + fragUV.y * 0.1;
    sheen *= vec3(1.0 + sin(hueShift * 6.28) * 0.1, 
                  1.0 + sin(hueShift * 6.28 + 2.09) * 0.1,
                  1.0 + sin(hueShift * 6.28 + 4.19) * 0.1);
    
    // === WEAVE MICRO-DETAIL ===
    float weaveVar = weavePattern(fragUV, weaveScale);
    diffuse *= weaveVar;
    
    // === AMBIENT ===
    vec3 ambient = material.ambient.rgb * light.ambient.rgb;
    
    // Combine all lighting components
    vec3 finalColor = ambient + 
                      (diffuse * light.diffuse.rgb + specular + sheen) * attenuation * NdotL +
                      sheen * 0.3; // Add some sheen even in shadow for silk glow
    
    return vec4(finalColor, material.diffuse.a);
}

void main()
{
    vec4 lit = silkLighting();
    
    // Apply ambient occlusion
    vec3 N = normalize(fragmentNormal);
    vec3 V = normalize(eyeDirection);
    vec3 L = normalize(lightDir);
    float ao = computeAO(N, V);
    vec3 aoTint = mix(aoGroundColor, vec3(1.0), ao);
    
    // Calculate shadow from all shadow-casting lights
    float shadow = calculateMultiShadow(N, L);
    
    // Apply shadow - darken lit parts, keep some ambient
    vec3 ambient = material.ambient.rgb * light.ambient.rgb * 0.3;
    vec3 finalColor = ambient + (lit.rgb - ambient) * shadow;
    finalColor *= aoTint;
    
    fragColour = vec4(finalColor, lit.a);
}
