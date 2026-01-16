#version 330 core

out vec2 FragColor;
in vec2 TexCoord;

const float PI = 3.14159265359;

float geometrySchlickGGX(float NdotV, float roughness) {
    float a = roughness;
    float k = (a * a) / 2.0;
    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return num / denom;
}

float geometrySmith(float NdotV, float NdotL, float roughness) {
    float ggx1 = geometrySchlickGGX(NdotV, roughness);
    float ggx2 = geometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec2 integrateBRDF(float NdotV, float roughness) {
    vec3 V;
    V.x = sqrt(1.0 - NdotV * NdotV);
    V.y = 0.0;
    V.z = NdotV;

    float A = 0.0;
    float B = 0.0;

    vec3 N = vec3(0.0, 0.0, 1.0);

    const uint SAMPLE_COUNT = 1024u;
    for (uint i = 0u; i < SAMPLE_COUNT; ++i) {
        float Xi1 = float(i) / float(SAMPLE_COUNT);
        float Xi2 = fract(float(i) * 0.61803398875);
        float phi = 2.0 * PI * Xi1;
        float cosTheta = sqrt((1.0 - Xi2) / (1.0 + (roughness * roughness - 1.0) * Xi2));
        float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

        vec3 H = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(L.z, 0.0);
        float NdotH = max(H.z, 0.0);
        float VdotH = max(dot(V, H), 0.0);

        if (NdotL > 0.0) {
            float G = geometrySmith(NdotV, NdotL, roughness);
            float G_Vis = (G * VdotH) / max(NdotH * NdotV, 0.0001);
            float Fc = pow(1.0 - VdotH, 5.0);
            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }

    A /= float(SAMPLE_COUNT);
    B /= float(SAMPLE_COUNT);
    return vec2(A, B);
}

void main() {
    vec2 integrated = integrateBRDF(TexCoord.x, TexCoord.y);
    FragColor = integrated;
}
