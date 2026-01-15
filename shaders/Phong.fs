#version 460 core

in vec3 fragmentNormal;
out vec4 fragColour;
in vec2 fragUV;
in vec4 fragPosLightSpace;
in vec3 worldPos;

// Multi-shadow support
const int MAX_SHADOW_LIGHTS = 4;
uniform sampler2DShadow shadowMaps[MAX_SHADOW_LIGHTS];
uniform mat4 lightSpaceMatrices[MAX_SHADOW_LIGHTS];
uniform float lightIntensities[MAX_SHADOW_LIGHTS];
uniform int numShadowLights;

// Extra light array (no shadows)
const int MAX_LIGHTS = 8;
uniform int numLights;
uniform vec3 lightPositions[MAX_LIGHTS];
uniform vec3 lightColors[MAX_LIGHTS];
uniform float lightIntensitiesExtra[MAX_LIGHTS];

// Shadow parameters
uniform sampler2DShadow shadowMap;
uniform float shadowBias;
uniform float shadowSoftness;
uniform float shadowStrength;
uniform int shadowEnabled;
uniform float shadowMapSize;

// Checker pattern parameters
uniform float checkerScale;    // Checker pattern scale (0 = disabled)
uniform vec3 checkerColor1;    // Primary checker color
uniform vec3 checkerColor2;    // Secondary checker color

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
    vec2 texelSize = vec2(1.0 / shadowMapSize);
    float radius = max(1.0, shadowSoftness) * 1.5;
    // Fixed 5x5 PCF kernel
    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) {
            vec2 offset = vec2(x, y) * texelSize * radius;
            vec3 sampleCoord = vec3(projCoords.xy + offset, currentDepth);
            shadow += texture(shadowMap, sampleCoord);
        }
    }
    
    shadow /= 25.0;
    // Darken shadows by strength factor
    shadow = pow(clamp(shadow, 0.0, 1.0), shadowStrength);
    
    return shadow;
}

// Calculate shadow for a specific shadow map (multi-shadow)
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
    vec2 texelSize = vec2(1.0 / shadowMapSize);
    float radius = max(1.0, shadowSoftness) * 1.5;
    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) {
            vec2 offset = vec2(x, y) * texelSize * radius;
            vec3 sampleCoord = vec3(projCoords.xy + offset, currentDepth);
            shadow += texture(sMap, sampleCoord);
        }
    }
    shadow /= 25.0;
    shadow = pow(clamp(shadow, 0.0, 1.0), shadowStrength);
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
        
        // Weight shadow contribution by intensity
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

struct Materials
{
  vec4 ambient;
  vec4 diffuse;
  vec4 specular;
  float shininess;
};

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

// Ambient occlusion parameters
uniform float aoStrength;      // 0 = off, 0.5 = subtle, 1.0 = strong
uniform vec3 aoGroundColor;    // Color tint for ground occlusion

in vec3 lightDir;
// out the blinn half vector
in vec3 halfVector;
in vec3 eyeDirection;
in vec3 vPosition;


// Ambient occlusion approximation
float computeAO(vec3 N, vec3 V)
{
    // Hemisphere occlusion: darken surfaces facing down (ground bounce blocked)
    float hemisphereAO = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
    
    // Cavity/crease darkening: areas where normal faces away from view
    float cavityAO = clamp(dot(N, V) * 0.7 + 0.3, 0.0, 1.0);
    
    // Edge darkening for folds (inverted fresnel)
    float edgeAO = clamp(1.0 - pow(1.0 - abs(dot(N, V)), 2.0), 0.3, 1.0);
    
    return mix(1.0, hemisphereAO * cavityAO * edgeAO, aoStrength);
}

vec4 pointLight()
{
  vec3 N = normalize(fragmentNormal);
  vec3 halfV;
  float ndothv;
  float attenuation;
  vec3 E = normalize(eyeDirection);
  vec3 L = normalize(lightDir);
  float lambertTerm = dot(N,L);
  vec4 diffuse=vec4(0);
  vec4 ambient=vec4(0);
  vec4 specular=vec4(0);
  if (lambertTerm > 0.0)
  {
  float d;            // distance from surface to light position
  vec3 VP;            // direction from surface to light position

  // Compute vector from surface to light position
  VP = vec3 (light.position) - vPosition;

  // Compute distance between surface and light position
    d = length (VP);
    attenuation = 1.f / (light.constantAttenuation +
                       light.linearAttenuation * d +
                       light.quadraticAttenuation * d * d);

    diffuse+=material.diffuse*light.diffuse*lambertTerm*attenuation;
    ambient+=material.ambient*light.ambient*attenuation;
    halfV = normalize(halfVector);
    ndothv = max(dot(N, halfV), 0.0);
    specular+=material.specular*light.specular*pow(ndothv, material.shininess)* attenuation;
  }
return ambient + diffuse + specular;
}

vec3 extraLights(vec3 N, vec3 V)
{
    vec3 accum = vec3(0.0);
    int count = numLights;
    if (count > MAX_LIGHTS) count = MAX_LIGHTS;
    for (int i = 0; i < count; ++i) {
        vec3 L = lightPositions[i] - worldPos;
        float dist = length(L);
        if (dist > 0.0001) L /= dist;
        float diff = max(dot(N, L), 0.0);
        vec3 H = normalize(L + V);
        float spec = pow(max(dot(N, H), 0.0), material.shininess);
        float intensity = lightIntensitiesExtra[i];
        vec3 color = lightColors[i];
        accum += (material.diffuse.rgb * color * diff + material.specular.rgb * color * spec) * intensity;
    }
    return accum;
}

void main ()
{
    // Compute ambient occlusion
    vec3 N = normalize(fragmentNormal);
    vec3 V = normalize(eyeDirection);
    vec3 L = normalize(lightDir);
    float ao = computeAO(N, V);
    
    // Calculate shadow from all shadow-casting lights
    float shadow = calculateMultiShadow(N, L);
    
    // Apply checker pattern
    vec3 checkerMod = checkerPattern(fragUV, checkerScale, checkerColor1, checkerColor2);
    vec4 baseColor = pointLight();
    vec3 extra = extraLights(N, V);
    
    // Apply AO and shadow - affects ambient and diffuse, not specular
    vec3 aoTint = mix(aoGroundColor, vec3(1.0), ao);
    
    // Shadow affects diffuse and specular, not ambient
    vec3 ambient = material.ambient.rgb * light.ambient.rgb;
    vec3 litColor = baseColor.rgb - ambient; // Remove ambient to apply shadow only to lit parts
    vec3 finalColor = (ambient + litColor * shadow + extra) * checkerMod * aoTint;
    
    // Gamma correction
    finalColor = pow(clamp(finalColor, 0.0, 10.0), vec3(1.0 / 2.2));
    
    fragColour = vec4(finalColor, baseColor.a);
}

