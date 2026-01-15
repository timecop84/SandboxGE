#version 150

in vec3 inVert;
in vec2 inUV;

out vec2 TexCoords;

void main() {
    TexCoords = inUV;
    gl_Position = vec4(inVert.xy, 0.0, 1.0);
}
