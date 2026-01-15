#version 460 core

uniform bool Normalize;
uniform vec3 viewerPos;

in vec3 inVert;
in vec3 inNormal;
in vec2 inUV;

out vec3 fragmentNormal;
out vec3 fragmentTangent;
out vec3 fragmentBitangent;
out vec3 worldPos;
out vec3 vPosition;
out vec2 fragUV;
out vec3 eyeDirection;

uniform mat4 MV;
uniform mat4 MVP;
uniform mat3 normalMatrix;
uniform mat4 M;

void main() {
    // World position for lighting calculations
    vec4 worldPosition = M * vec4(inVert, 1.0);
    worldPos = worldPosition.xyz;
    
    // Transform normal to WORLD space for PBR (not view space)
    mat3 worldNormalMatrix = mat3(transpose(inverse(M)));
    fragmentNormal = worldNormalMatrix * inNormal;
    if (Normalize) {
        fragmentNormal = normalize(fragmentNormal);
    }
    
    // Generate tangent frame in world space for anisotropic shading
    vec3 worldNormal = normalize(fragmentNormal);
    vec3 up = abs(worldNormal.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    fragmentTangent = normalize(cross(up, worldNormal));
    fragmentBitangent = normalize(cross(worldNormal, fragmentTangent));
    
    // Eye direction in world space
    eyeDirection = normalize(viewerPos - worldPos);
    
    // Position in eye coordinates (for compatibility)
    vec4 eyeCord = MV * vec4(inVert, 1.0);
    vPosition = eyeCord.xyz / eyeCord.w;
    
    // Pass UV
    fragUV = inUV;
    
    // Final position
    gl_Position = MVP * vec4(inVert, 1.0);
}
