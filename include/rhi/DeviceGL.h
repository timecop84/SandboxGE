#pragma once

#include "rhi/Device.h"
#include <memory>

namespace sandbox::rhi {

class GLDevice final : public Device {
public:
    GLDevice() = default;
    ~GLDevice() override = default;

    std::unique_ptr<Buffer> createBuffer(size_t size, BufferUsage usage) override;
    std::unique_ptr<Texture> createTexture(int width, int height, TextureFormat format,
                                           TextureTarget target = TextureTarget::Texture2D) override;
    std::unique_ptr<Sampler> createSampler(const SamplerDesc& desc) override;
};

std::unique_ptr<Device> createOpenGLDevice();

} // namespace sandbox::rhi
