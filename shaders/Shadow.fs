#version 150

/// Outputs nothing - we only care about depth buffer

out vec4 fragColor;

void main()
{
    // Depth is written automatically to depth buffer
    // Output a simple color for debugging if needed
    fragColor = vec4(1.0);
}
