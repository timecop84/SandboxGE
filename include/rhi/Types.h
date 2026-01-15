#pragma once

#include <cstdint>

namespace sandbox::rhi {

enum class BufferUsage : std::uint8_t {
    Static,
    Dynamic,
    Stream
};

enum class BufferBindTarget : std::uint8_t {
    Array,
    Element,
    Uniform,
    ShaderStorage
};

enum class TextureFormat : std::uint8_t {
    R8,
    R16F,
    R32F,
    RG16F,
    RGB16F,
    RGBA16F,
    RGBA8,
    Depth24,
    Depth32F
};

enum class TextureTarget : std::uint8_t {
    Texture2D
};

enum class SamplerFilter : std::uint8_t {
    Nearest,
    Linear
};

enum class SamplerWrap : std::uint8_t {
    Repeat,
    ClampToEdge,
    ClampToBorder
};

enum class SamplerCompareOp : std::uint8_t {
    Never,
    Less,
    Lequal,
    Equal,
    Gequal,
    Greater,
    NotEqual,
    Always
};

struct SamplerDesc {
    SamplerFilter minFilter = SamplerFilter::Linear;
    SamplerFilter magFilter = SamplerFilter::Linear;
    SamplerWrap wrapS = SamplerWrap::ClampToEdge;
    SamplerWrap wrapT = SamplerWrap::ClampToEdge;
    bool compareEnabled = false;
    SamplerCompareOp compareOp = SamplerCompareOp::Lequal;
    float borderColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
};

using Handle = std::uint32_t;

} // namespace sandbox::rhi
