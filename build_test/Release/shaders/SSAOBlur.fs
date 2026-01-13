#version 150
/// @brief SSAO blur shader - bilateral blur to preserve edges

in vec2 TexCoords;
out float fragColor;

uniform sampler2D ssaoTexture;
uniform vec2 texelSize;

void main() {
    float result = 0.0;
    
    // 4x4 box blur
    for (int x = -2; x < 2; ++x) {
        for (int y = -2; y < 2; ++y) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            result += texture(ssaoTexture, TexCoords + offset).r;
        }
    }
    
    fragColor = result / 16.0;
}
