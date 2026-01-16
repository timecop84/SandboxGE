#version 330 core

// Refraction shader for SandboxGE

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

out vec4 FragColor;

uniform sampler2D texture_diffuse;
uniform float ior; // Index of refraction
uniform vec3 cameraPos;
uniform vec3 lightWorldPos;
uniform vec3 lightColor;
uniform vec2 screenSize;
uniform sampler2D envMap;
uniform float envMapIntensity;
uniform mat4 viewMatrix;

const float PI = 3.14159265359;

vec2 equirectUV(vec3 dir) {
    dir = normalize(dir);
    float u = atan(dir.z, dir.x) / (2.0 * PI) + 0.5;
    float v = acos(clamp(dir.y, -1.0, 1.0)) / PI;
    return vec2(u, v);
}

void main()
{
    vec3 I = normalize(FragPos - cameraPos);
    vec3 N = normalize(Normal);
    vec3 V = normalize(cameraPos - FragPos);
    vec3 L = normalize(lightWorldPos - FragPos);
    float eta = 1.0 / ior;
    vec3 viewPos = vec3(viewMatrix * vec4(FragPos, 1.0));
    vec3 Iview = normalize(viewPos);
    vec3 Nview = normalize(mat3(viewMatrix) * N);
    vec3 refractDir = refract(Iview, Nview, eta);
    vec2 screenUV = gl_FragCoord.xy / screenSize;
    float viewDot = clamp(dot(N, -I), 0.0, 1.0);
    float strength = (0.03 + (1.0 - viewDot) * 0.28) * max(ior - 1.0, 0.2);
    vec2 offsetDir = refractDir.xy / max(abs(refractDir.z), 0.2);
    vec2 refractUV = screenUV + offsetDir * strength;
    refractUV = clamp(refractUV, vec2(0.001), vec2(0.999));
    vec4 refractColor = texture(texture_diffuse, refractUV);

    vec3 envColor = vec3(0.0);
    if (envMapIntensity > 0.001) {
        vec3 R = reflect(-V, N);
        vec2 envUV = equirectUV(R);
        envColor = texture(envMap, envUV).rgb;
    }

    float NdotL = max(dot(N, L), 0.0);
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), 64.0);
    float lightTerm = clamp(0.15 + NdotL + spec * 1.5, 0.0, 2.0);
    vec3 litReflection = envColor * envMapIntensity * lightColor * lightTerm;
    float fresnel = pow(1.0 - viewDot, 5.0);
    vec3 color = mix(refractColor.rgb, litReflection, fresnel);
    FragColor = vec4(color, 1.0);
}
