#version 150
/// @brief Screen-Space Ambient Occlusion fragment shader

in vec2 TexCoords;
out float fragColor;

uniform sampler2D depthTexture;
uniform sampler2D noiseTexture;

uniform vec3 samples[64];  // Sample kernel
uniform mat4 projection;
uniform mat4 invProjection;
uniform vec2 screenSize;
uniform float radius;      // Sample radius in world units
uniform float bias;        // Depth bias to prevent self-occlusion
uniform float intensity;   // AO intensity multiplier

const int kernelSize = 48;
const vec2 noiseScale = vec2(4.0, 4.0);  // Noise texture is 4x4

// Reconstruct view-space position from depth
vec3 viewPosFromDepth(vec2 uv, float depth) {
    // Convert to NDC
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    // Unproject
    vec4 viewPos = invProjection * clipPos;
    return viewPos.xyz / viewPos.w;
}

// Linearize depth for better sampling
float linearizeDepth(float depth, float near, float far) {
    float z = depth * 2.0 - 1.0;
    return (2.0 * near * far) / (far + near - z * (far - near));
}

void main() {
    vec2 uv = TexCoords;
    
    // Get depth at current pixel
    float depth = texture(depthTexture, uv).r;
    
    // Skip background (far plane)
    if (depth >= 0.9999) {
        fragColor = 1.0;
        return;
    }
    
    // Reconstruct view-space position
    vec3 fragPos = viewPosFromDepth(uv, depth);
    
    // Compute normal from depth derivatives (screen-space normal)
    vec2 texelSize = 1.0 / screenSize;
    float depthL = texture(depthTexture, uv - vec2(texelSize.x, 0.0)).r;
    float depthR = texture(depthTexture, uv + vec2(texelSize.x, 0.0)).r;
    float depthU = texture(depthTexture, uv - vec2(0.0, texelSize.y)).r;
    float depthD = texture(depthTexture, uv + vec2(0.0, texelSize.y)).r;
    
    vec3 posL = viewPosFromDepth(uv - vec2(texelSize.x, 0.0), depthL);
    vec3 posR = viewPosFromDepth(uv + vec2(texelSize.x, 0.0), depthR);
    vec3 posU = viewPosFromDepth(uv - vec2(0.0, texelSize.y), depthU);
    vec3 posD = viewPosFromDepth(uv + vec2(0.0, texelSize.y), depthD);
    
    vec3 normal = normalize(cross(posR - posL, posD - posU));
    
    // Random rotation from noise texture
    vec2 noiseUV = uv * screenSize / 4.0;
    vec3 randomVec = texture(noiseTexture, noiseUV).xyz * 2.0 - 1.0;
    
    // Create TBN matrix for oriented hemisphere
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);
    
    // Sample hemisphere and accumulate occlusion
    float occlusion = 0.0;
    for (int i = 0; i < kernelSize; ++i) {
        // Get sample position in view space
        vec3 sampleDir = TBN * samples[i];
        vec3 samplePos = fragPos + sampleDir * radius;
        
        // Project sample to screen space
        vec4 offset = projection * vec4(samplePos, 1.0);
        offset.xy /= offset.w;
        offset.xy = offset.xy * 0.5 + 0.5;
        
        // Sample depth at offset
        float sampleDepth = texture(depthTexture, offset.xy).r;
        vec3 sampleViewPos = viewPosFromDepth(offset.xy, sampleDepth);
        
        // Range check and occlusion test
        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(fragPos.z - sampleViewPos.z));
        occlusion += (sampleViewPos.z >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
    }
    
    float occ = occlusion / float(kernelSize);
    occ = clamp(occ * intensity, 0.0, 1.0);
    fragColor = 1.0 - occ;
}
