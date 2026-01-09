#version 450 core

// Per-vertex attributes
layout(location = 0) in vec3 inVert;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;

// Per-instance attribute: particle position offset
layout(location = 3) in vec3 instancePos;

// Uniforms
uniform mat4 View;
uniform mat4 Projection;
uniform vec3 viewerPos;
uniform bool Normalize;

// Output to fragment shader
out vec3 fragmentNormal;
out vec3 lightDir;
out vec3 halfVector;
out vec3 eyeDirection;
out vec3 vPosition;
out vec2 fragUV;
out vec4 fragPosLightSpace;

struct Lights {
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
uniform Lights light;
uniform mat4 lightSpaceMatrix;

void main()
{
    // Build model matrix from instance position (just translation, no rotation/scale)
    mat4 M = mat4(1.0);
    M[3] = vec4(instancePos, 1.0);
    
    mat4 MV = View * M;
    mat4 MVP = Projection * MV;
    mat3 normalMatrix = mat3(transpose(inverse(MV)));
    
    // Transform normal
    fragmentNormal = normalMatrix * inNormal;
    if (Normalize) {
        fragmentNormal = normalize(fragmentNormal);
    }
    
    // Vertex position in clip space
    gl_Position = MVP * vec4(inVert, 1.0);
    
    // World position for lighting
    vec4 worldPosition = M * vec4(inVert, 1.0);
    eyeDirection = normalize(viewerPos - worldPosition.xyz);
    
    // Eye-space position
    vec4 eyeCoord = MV * vec4(inVert, 1.0);
    vPosition = eyeCoord.xyz / eyeCoord.w;
    
    // Light direction in eye space
    lightDir = normalize(light.position.xyz - vPosition);
    halfVector = normalize(lightDir + normalize(-vPosition));
    
    fragUV = inUV;
    fragPosLightSpace = lightSpaceMatrix * worldPosition;
}
