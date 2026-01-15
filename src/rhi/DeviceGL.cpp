#include "rhi/DeviceGL.h"
#include <glad/gl.h>

namespace sandbox::rhi {

namespace {

GLenum toGlBufferUsage(BufferUsage usage) {
    switch (usage) {
        case BufferUsage::Dynamic: return GL_DYNAMIC_DRAW;
        case BufferUsage::Stream: return GL_STREAM_DRAW;
        case BufferUsage::Static:
        default: return GL_STATIC_DRAW;
    }
}

GLenum toGlBufferTarget(BufferBindTarget target) {
    switch (target) {
        case BufferBindTarget::Element: return GL_ELEMENT_ARRAY_BUFFER;
        case BufferBindTarget::Uniform: return GL_UNIFORM_BUFFER;
        case BufferBindTarget::ShaderStorage: return GL_SHADER_STORAGE_BUFFER;
        case BufferBindTarget::Array:
        default: return GL_ARRAY_BUFFER;
    }
}

GLenum toGlTextureTarget(TextureTarget target) {
    switch (target) {
        case TextureTarget::Texture2D:
        default: return GL_TEXTURE_2D;
    }
}

GLenum toGlSamplerFilter(SamplerFilter filter) {
    switch (filter) {
        case SamplerFilter::Nearest: return GL_NEAREST;
        case SamplerFilter::Linear:
        default: return GL_LINEAR;
    }
}

GLenum toGlSamplerWrap(SamplerWrap wrap) {
    switch (wrap) {
        case SamplerWrap::Repeat: return GL_REPEAT;
        case SamplerWrap::ClampToBorder: return GL_CLAMP_TO_BORDER;
        case SamplerWrap::ClampToEdge:
        default: return GL_CLAMP_TO_EDGE;
    }
}

GLenum toGlCompareOp(SamplerCompareOp op) {
    switch (op) {
        case SamplerCompareOp::Never: return GL_NEVER;
        case SamplerCompareOp::Less: return GL_LESS;
        case SamplerCompareOp::Lequal: return GL_LEQUAL;
        case SamplerCompareOp::Equal: return GL_EQUAL;
        case SamplerCompareOp::Gequal: return GL_GEQUAL;
        case SamplerCompareOp::Greater: return GL_GREATER;
        case SamplerCompareOp::NotEqual: return GL_NOTEQUAL;
        case SamplerCompareOp::Always: return GL_ALWAYS;
        default: return GL_LEQUAL;
    }
}

GLenum toGlInternalFormat(TextureFormat format) {
    switch (format) {
        case TextureFormat::R8: return GL_R8;
        case TextureFormat::R16F: return GL_R16F;
        case TextureFormat::R32F: return GL_R32F;
        case TextureFormat::RG16F: return GL_RG16F;
        case TextureFormat::RGB16F: return GL_RGB16F;
        case TextureFormat::RGBA16F: return GL_RGBA16F;
        case TextureFormat::Depth24: return GL_DEPTH_COMPONENT24;
        case TextureFormat::Depth32F: return GL_DEPTH_COMPONENT32F;
        case TextureFormat::RGBA8:
        default: return GL_RGBA8;
    }
}

GLenum toGlDataFormat(TextureFormat format) {
    switch (format) {
        case TextureFormat::Depth24:
        case TextureFormat::Depth32F:
            return GL_DEPTH_COMPONENT;
        case TextureFormat::R8:
        case TextureFormat::R16F:
        case TextureFormat::R32F:
            return GL_RED;
        case TextureFormat::RG16F:
            return GL_RG;
        case TextureFormat::RGB16F:
            return GL_RGB;
        case TextureFormat::RGBA16F:
        case TextureFormat::RGBA8:
        default:
            return GL_RGBA;
    }
}

GLenum toGlDataType(TextureFormat format) {
    switch (format) {
        case TextureFormat::Depth32F:
            return GL_FLOAT;
        case TextureFormat::Depth24:
            return GL_UNSIGNED_INT;
        case TextureFormat::R16F:
        case TextureFormat::RG16F:
        case TextureFormat::RGB16F:
        case TextureFormat::RGBA16F:
            return GL_HALF_FLOAT;
        case TextureFormat::R32F:
            return GL_FLOAT;
        case TextureFormat::R8:
        case TextureFormat::RGBA8:
        default:
            return GL_UNSIGNED_BYTE;
    }
}

class GLBuffer final : public Buffer {
public:
    GLBuffer(size_t size, BufferUsage usage) : m_size(size) {
        glGenBuffers(1, &m_id);
        glBindBuffer(GL_ARRAY_BUFFER, m_id);
        glBufferData(GL_ARRAY_BUFFER, m_size, nullptr, toGlBufferUsage(usage));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    ~GLBuffer() override {
        if (m_id != 0) {
            glDeleteBuffers(1, &m_id);
        }
    }

    void bind(BufferBindTarget target) const override {
        glBindBuffer(toGlBufferTarget(target), m_id);
    }

    void update(const void* data, size_t size, size_t offset) override {
        glBindBuffer(GL_ARRAY_BUFFER, m_id);
        glBufferSubData(GL_ARRAY_BUFFER, offset, size, data);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    std::uintptr_t nativeHandle() const override {
        return static_cast<std::uintptr_t>(m_id);
    }

private:
    GLuint m_id = 0;
    size_t m_size = 0;
};

class GLTexture final : public Texture {
public:
    GLTexture(int width, int height, TextureFormat format, TextureTarget target)
        : m_glTarget(toGlTextureTarget(target)) {
        glGenTextures(1, &m_id);
        glBindTexture(m_glTarget, m_id);
        glTexParameteri(m_glTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(m_glTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(m_glTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(m_glTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        GLenum internalFormat = toGlInternalFormat(format);
        GLenum dataFormat = toGlDataFormat(format);
        GLenum dataType = toGlDataType(format);
        glTexImage2D(m_glTarget, 0, internalFormat, width, height, 0, dataFormat, dataType, nullptr);

        glBindTexture(m_glTarget, 0);
    }

    ~GLTexture() override {
        if (m_id != 0) {
            glDeleteTextures(1, &m_id);
        }
    }

    void bind(std::uint32_t slot) const override {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(m_glTarget, m_id);
    }

    void unbind() const override {
        glBindTexture(m_glTarget, 0);
    }

    std::uintptr_t nativeHandle() const override {
        return static_cast<std::uintptr_t>(m_id);
    }

private:
    GLuint m_id = 0;
    GLenum m_glTarget = GL_TEXTURE_2D;
};

class GLSampler final : public Sampler {
public:
    explicit GLSampler(const SamplerDesc& desc) {
        glGenSamplers(1, &m_id);
        glSamplerParameteri(m_id, GL_TEXTURE_MIN_FILTER, toGlSamplerFilter(desc.minFilter));
        glSamplerParameteri(m_id, GL_TEXTURE_MAG_FILTER, toGlSamplerFilter(desc.magFilter));
        glSamplerParameteri(m_id, GL_TEXTURE_WRAP_S, toGlSamplerWrap(desc.wrapS));
        glSamplerParameteri(m_id, GL_TEXTURE_WRAP_T, toGlSamplerWrap(desc.wrapT));

        if (desc.compareEnabled) {
            glSamplerParameteri(m_id, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
            glSamplerParameteri(m_id, GL_TEXTURE_COMPARE_FUNC, toGlCompareOp(desc.compareOp));
        } else {
            glSamplerParameteri(m_id, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        }

        if (desc.wrapS == SamplerWrap::ClampToBorder || desc.wrapT == SamplerWrap::ClampToBorder) {
            glSamplerParameterfv(m_id, GL_TEXTURE_BORDER_COLOR, desc.borderColor);
        }
    }

    ~GLSampler() override {
        if (m_id != 0) {
            glDeleteSamplers(1, &m_id);
        }
    }

    void bind(std::uint32_t slot) const override {
        glBindSampler(slot, m_id);
    }

    void unbind(std::uint32_t slot) const override {
        glBindSampler(slot, 0);
    }

    std::uintptr_t nativeHandle() const override {
        return static_cast<std::uintptr_t>(m_id);
    }

private:
    GLuint m_id = 0;
};

} // namespace

std::unique_ptr<Buffer> GLDevice::createBuffer(size_t size, BufferUsage usage) {
    return std::make_unique<GLBuffer>(size, usage);
}

std::unique_ptr<Texture> GLDevice::createTexture(int width, int height, TextureFormat format,
                                                  TextureTarget target) {
    return std::make_unique<GLTexture>(width, height, format, target);
}

std::unique_ptr<Sampler> GLDevice::createSampler(const SamplerDesc& desc) {
    return std::make_unique<GLSampler>(desc);
}

std::unique_ptr<Device> createOpenGLDevice() {
    return std::make_unique<GLDevice>();
}

} // namespace sandbox::rhi
