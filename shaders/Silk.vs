#version 460 core

uniform bool Normalize;
uniform vec3 viewerPos;
out vec3 fragmentNormal;
out vec3 fragmentTangent;
out vec3 fragmentBitangent;
out vec3 worldPos;
layout(location = 0) in vec3 inVert;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;

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

out vec3 lightDir;
out vec3 halfVector;
out vec3 eyeDirection;
out vec3 vPosition;
out vec2 fragUV;
out vec4 fragPosLightSpace;

uniform mat4 MV;
uniform mat4 MVP;
uniform mat3 normalMatrix;
uniform mat4 M;
uniform mat4 lightSpaceMatrix;

void main()
{
    // Calculate the fragment's surface normal
    fragmentNormal = normalMatrix * inNormal;
    
    if (Normalize)
    {
        fragmentNormal = normalize(fragmentNormal);
    }
    
    // Generate tangent and bitangent for anisotropic shading
    // For cloth, we use screen-space derivatives approximation
    // In production, these would come from vertex attributes
    vec3 worldNormal = normalize((M * vec4(inNormal, 0.0)).xyz);
    
    // Create tangent frame - for cloth, tangent follows U direction (warp threads)
    vec3 up = abs(worldNormal.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    fragmentTangent = normalize(normalMatrix * normalize(cross(up, inNormal)));
    fragmentBitangent = normalize(cross(fragmentNormal, fragmentTangent));
    
    // Pass UV coordinates for weave pattern
    fragUV = inUV;
    
    // Calculate the vertex position
    gl_Position = MVP * vec4(inVert, 1.0);
    
    vec4 worldPosition = M * vec4(inVert, 1.0);
    worldPos = worldPosition.xyz;
    eyeDirection = normalize(viewerPos - worldPosition.xyz);
    
    // Get vertex position in eye coordinates
    vec4 eyeCord = MV * vec4(inVert, 1.0);
    vPosition = eyeCord.xyz / eyeCord.w;
    
    // Light direction
    lightDir = vec3(light.position.xyz - eyeCord.xyz);
    float dist = length(lightDir);
    lightDir /= dist;
    halfVector = normalize(eyeDirection + lightDir);
    
    // Calculate position in light space for shadow mapping
    fragPosLightSpace = lightSpaceMatrix * worldPosition;
}
