#pragma once

#include "rhi/Types.h"
#include <cstddef>
#include <cstdint>
#include <memory>

namespace sandbox::rhi {

class Buffer {
public:
    virtual ~Buffer() = default;
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    virtual void bind(BufferBindTarget target) const = 0;
    virtual void update(const void* data, size_t size, size_t offset = 0) = 0;
    virtual std::uintptr_t nativeHandle() const = 0;

protected:
    Buffer() = default;
};

class Texture {
public:
    virtual ~Texture() = default;
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    virtual void bind(std::uint32_t slot = 0) const = 0;
    virtual void unbind() const = 0;
    virtual std::uintptr_t nativeHandle() const = 0;

protected:
    Texture() = default;
};

class Sampler {
public:
    virtual ~Sampler() = default;
    Sampler(const Sampler&) = delete;
    Sampler& operator=(const Sampler&) = delete;

    virtual void bind(std::uint32_t slot) const = 0;
    virtual void unbind(std::uint32_t slot) const = 0;
    virtual std::uintptr_t nativeHandle() const = 0;

protected:
    Sampler() = default;
};

class Device {
public:
    virtual ~Device() = default;

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    virtual std::unique_ptr<Buffer> createBuffer(size_t size, BufferUsage usage) = 0;
    virtual std::unique_ptr<Texture> createTexture(int width, int height, TextureFormat format,
                                                   TextureTarget target = TextureTarget::Texture2D) = 0;
    virtual std::unique_ptr<Sampler> createSampler(const SamplerDesc& desc) = 0;

protected:
    Device() = default;
};

} // namespace sandbox::rhi
