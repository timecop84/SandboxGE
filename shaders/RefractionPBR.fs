#version 460 core

in vec3 fragmentNormal;
in vec3 fragmentTangent;
in vec3 fragmentBitangent;
in vec3 worldPos;
in vec2 fragUV;
in vec3 eyeDirection;

out vec4 fragColour;

uniform sampler2D texture_diffuse;
uniform float ior; // Index of refraction
uniform float roughness;
uniform float metallic;
uniform vec3 F0; // Base reflectivity
uniform vec3 cameraPos;

// Fresnel-Schlick approximation
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    vec3 N = normalize(fragmentNormal);
    vec3 V = normalize(cameraPos - worldPos);
    float eta = 1.0 / ior;
    vec3 refractDir = refract(-V, N, eta);

    // Fresnel reflectance
    float cosTheta = clamp(dot(N, V), 0.0, 1.0);
    vec3 fresnel = fresnelSchlick(cosTheta, F0);

    // Sample refraction color
    vec2 refractUV = fragUV + refractDir.xy * 0.05; // tweak offset for effect
    vec4 refractColor = texture(texture_diffuse, refractUV);

    // Simple PBR: mix refraction and reflection
    vec3 baseColor = refractColor.rgb;
    vec3 reflected = fresnel * baseColor;
    vec3 transmitted = (1.0 - fresnel) * baseColor;
    vec3 finalColor = mix(transmitted, reflected, metallic);

    // Apply roughness as blur (simple approximation)
    finalColor = mix(finalColor, vec3(1.0), roughness * 0.15);

    fragColour = vec4(finalColor, refractColor.a);
}
