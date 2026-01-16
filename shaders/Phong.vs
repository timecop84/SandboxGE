#version 460 core
uniform bool Normalize;
// the eye position of the camera
uniform vec3 viewerPos;
out vec3 fragmentNormal;
layout(location = 0) in vec3 inVert;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;
out vec3 worldPos;

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
// our material
uniform Materials material;
// array of lights
uniform Lights light;
// direction of the lights used for shading
out vec3 lightDir;
// out the blinn half vector
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
// calculate the fragments surface normal
fragmentNormal = (normalMatrix*inNormal);


if (Normalize == true)
{
 fragmentNormal = normalize(fragmentNormal);
}
// calculate the vertex position
gl_Position = MVP*vec4(inVert,1.0);

vec4 worldPosition = M * vec4(inVert, 1.0);
worldPos = worldPosition.xyz;
eyeDirection = normalize(viewerPos - worldPosition.xyz);
// Get vertex position in eye coordinates
// Transform the vertex to eye co-ordinates for frag shader
vec4 eyeCord=MV*vec4(inVert,1);

vPosition = eyeCord.xyz / eyeCord.w;;

float dist;

lightDir=vec3(light.position.xyz-eyeCord.xyz);
dist = length(lightDir);
lightDir/= dist;
halfVector = normalize(eyeDirection + lightDir);

fragUV = inUV;

// Calculate position in light space for shadow mapping
fragPosLightSpace = lightSpaceMatrix * worldPosition;
}
