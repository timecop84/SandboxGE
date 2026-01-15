#version 150

in vec2 TexCoords;
out vec4 fragColor;

uniform sampler2D sceneTexture;
uniform sampler2D ssaoTexture;
uniform float ssaoStrength;

void main() {
    vec3 sceneColor = texture(sceneTexture, TexCoords).rgb;
    float ao = texture(ssaoTexture, TexCoords).r;
    
    // Apply AO - affects overall lighting
    ao = mix(1.0, ao, ssaoStrength);
    
    fragColor = vec4(sceneColor * ao, 1.0);
}
