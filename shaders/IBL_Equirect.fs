#version 330 core

out vec4 FragColor;
in vec3 localPos;

uniform sampler2D equirectMap;

const float PI = 3.14159265359;

vec2 sampleSphericalMap(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= vec2(0.15915494, 0.318309886); // 1/(2*PI), 1/PI
    uv += 0.5;
    return uv;
}

void main() {
    vec3 n = normalize(localPos);
    vec2 uv = sampleSphericalMap(n);
    vec3 color = texture(equirectMap, uv).rgb;
    FragColor = vec4(color, 1.0);
}
