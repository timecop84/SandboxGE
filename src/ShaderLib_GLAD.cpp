#include <glad/gl.h>
#include "rhi/Device.h"
#include "utils/ShaderLib.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <glm/gtc/type_ptr.hpp>
#ifdef _WIN32
#include <windows.h>
#endif

static std::string getExecutableDir() {
#ifdef _WIN32
    char path[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, path, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        std::string exePath(path);
        size_t lastSlash = exePath.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            return exePath.substr(0, lastSlash + 1);
        }
    }
#endif
    return "";
}

namespace sandbox {

// Static singleton instance pointer
ShaderLib* ShaderLib::s_instance = nullptr;

ShaderLib* ShaderLib::instance() {
    if (!s_instance) {
        s_instance = new ShaderLib();
    }
    return s_instance;
}

ShaderLib::~ShaderLib() = default;

void ShaderLib::setDevice(rhi::Device* device) {
    m_device = device;
}

void ShaderLib::createShader(const std::string& name) {
    (void)name;
    // Individual shaders are created when attachShader is called
}

void ShaderLib::createShaderProgram(const std::string& name) {
    unsigned int programId = glCreateProgram();
    if (programId == 0) {
        std::cerr << "Failed to create OpenGL shader program: " << name << std::endl;
        return;
    }
    
    auto wrapper = std::make_unique<ProgramWrapper>(programId);
    m_wrappers[name] = std::move(wrapper);
    m_programs[name] = programId;
}

void ShaderLib::attachShader(const std::string& name, int type) {
    unsigned int shaderId = glCreateShader(glShaderType(type));
    if (shaderId == 0) {
        std::cerr << "Failed to create OpenGL shader: " << name << std::endl;
        return;
    }
    
    m_shaders[name] = shaderId;
    m_shaderTypes[name] = type;
}

void ShaderLib::loadShaderSource(const std::string& name, const std::string& filename) {
    std::string source;
    if (loadShaderFromFile(filename, source)) {
        m_shaderSources[name] = source;
    } else {
        std::cerr << "Failed to load shader source: " << filename << std::endl;
    }
}

void ShaderLib::compileShader(const std::string& name) {
    auto shaderIt = m_shaders.find(name);
    auto sourceIt = m_shaderSources.find(name);
    
    if (shaderIt != m_shaders.end() && sourceIt != m_shaderSources.end()) {
        const char* sourcePtr = sourceIt->second.c_str();
        glShaderSource(shaderIt->second, 1, &sourcePtr, nullptr);
        glCompileShader(shaderIt->second);
        
        if (!checkShaderCompilation(shaderIt->second, name)) {
            std::cerr << "Failed to compile shader: " << name << std::endl;
        }
    }
}

void ShaderLib::attachShaderToProgram(const std::string& program, const std::string& shader) {
    auto programIt = m_programs.find(program);
    auto shaderIt = m_shaders.find(shader);
    
    if (programIt != m_programs.end() && shaderIt != m_shaders.end()) {
        // Check if already attached
        auto& attachments = m_programShaderAttachments[program];
        if (attachments.find(shader) != attachments.end()) {
            return; // Already attached
        }
        
        glAttachShader(programIt->second, shaderIt->second);
        attachments.insert(shader);
    } else {
        std::cerr << "Failed to attach shader " << shader << " to program " << program << std::endl;
    }
}

void ShaderLib::bindAttribute(const std::string& program, int index, const std::string& name) {
    auto programIt = m_programs.find(program);
    if (programIt != m_programs.end()) {
        glBindAttribLocation(programIt->second, index, name.c_str());
    } else {
        std::cerr << "Shader program not found: " << program << std::endl;
    }
}

void ShaderLib::linkProgramObject(const std::string& name) {
    auto programIt = m_programs.find(name);
    if (programIt != m_programs.end()) {
        glLinkProgram(programIt->second);
        if (!checkProgramLinking(programIt->second, name)) {
            std::cerr << "Failed to link program: " << name << std::endl;
        }
    } else {
        std::cerr << "Shader program not found: " << name << std::endl;
    }
}

void ShaderLib::use(const std::string& name) {
    auto programIt = m_programs.find(name);
    if (programIt != m_programs.end()) {
        glUseProgram(programIt->second);
        m_currentShader = name;
        auto wrapperIt = m_wrappers.find(name);
        if (wrapperIt != m_wrappers.end()) {
            m_currentWrapper = wrapperIt->second.get();
        }
    } else {
        std::cerr << "Shader program not found: " << name << std::endl;
    }
}

// ProgramWrapper implementation
ShaderLib::ProgramWrapper::ProgramWrapper(unsigned int programId) : m_programId(programId) {}

void ShaderLib::ProgramWrapper::use() {
    glUseProgram(m_programId);
}

int ShaderLib::ProgramWrapper::getUniformLocation(const std::string& name) {
    return glGetUniformLocation(m_programId, name.c_str());
}

void ShaderLib::ProgramWrapper::setUniform(const std::string& name, const glm::mat4& value) {
    int location = getUniformLocation(name);
    if (location != -1) {
        glUniformMatrix4fv(location, 1, false, glm::value_ptr(value));
    }
}

void ShaderLib::ProgramWrapper::setUniform(const std::string& name, const glm::mat3& value) {
    int location = getUniformLocation(name);
    if (location != -1) {
        glUniformMatrix3fv(location, 1, false, glm::value_ptr(value));
    }
}

void ShaderLib::ProgramWrapper::setUniform(const std::string& name, const glm::vec4& value) {
    int location = getUniformLocation(name);
    if (location != -1) {
        glUniform4f(location, value.x, value.y, value.z, value.w);
    }
}

void ShaderLib::ProgramWrapper::setUniform(const std::string& name, const glm::vec3& value) {
    int location = getUniformLocation(name);
    if (location != -1) {
        glUniform3f(location, value.x, value.y, value.z);
    }
}

void ShaderLib::ProgramWrapper::setUniform(const std::string& name, float value) {
    int location = getUniformLocation(name);
    if (location != -1) {
        glUniform1f(location, value);
    }
}

void ShaderLib::ProgramWrapper::setUniform(const std::string& name, int value) {
    int location = getUniformLocation(name);
    if (location != -1) {
        glUniform1i(location, value);
    }
}

// Legacy setShaderParam functions for backward compatibility
void ShaderLib::setShaderParam(const std::string& paramName, const glm::mat4& value) {
    if (m_currentWrapper) m_currentWrapper->setUniform(paramName, value);
}

void ShaderLib::setShaderParam(const std::string& paramName, const glm::mat3& value) {
    if (m_currentWrapper) m_currentWrapper->setUniform(paramName, value);
}

void ShaderLib::setShaderParam(const std::string& paramName, const glm::vec4& value) {
    if (m_currentWrapper) m_currentWrapper->setUniform(paramName, value);
}

void ShaderLib::setShaderParam(const std::string& paramName, const glm::vec3& value) {
    if (m_currentWrapper) m_currentWrapper->setUniform(paramName, value);
}

void ShaderLib::setShaderParam(const std::string& paramName, float value) {
    if (m_currentWrapper) m_currentWrapper->setUniform(paramName, value);
}

void ShaderLib::setShaderParam(const std::string& paramName, int value) {
    if (m_currentWrapper) m_currentWrapper->setUniform(paramName, value);
}

void ShaderLib::setShaderParam3f(const std::string& paramName, float x, float y, float z) {
    if (m_currentWrapper) m_currentWrapper->setUniform(paramName, glm::vec3(x, y, z));
}

void ShaderLib::setShaderParam4f(const std::string& paramName, float x, float y, float z, float w) {
    if (m_currentWrapper) m_currentWrapper->setUniform(paramName, glm::vec4(x, y, z, w));
}

void ShaderLib::setShaderParamFromMatrix(const std::string& paramName, const glm::mat4& matrix) {
    if (m_currentWrapper) m_currentWrapper->setUniform(paramName, matrix);
}

void ShaderLib::setShaderParamFromMat3x3(const std::string& paramName, const glm::mat3& matrix) {
    if (m_currentWrapper) m_currentWrapper->setUniform(paramName, matrix);
}

ShaderLib::ProgramWrapper* ShaderLib::operator[](const std::string& name) {
    // First, try to find existing wrapper
    auto it = m_wrappers.find(name);
    if (it != m_wrappers.end()) {
        m_currentShader = name;
        m_currentWrapper = it->second.get();
        return it->second.get();
    }
    
    // Auto-create PhongUBO shader with UBO support
    if (name == "PhongUBO") {
        createShaderProgram("PhongUBO");
        
        attachShader("PhongUBOVertex", VERTEX);
        loadShaderSource("PhongUBOVertex", "shaders/PhongUBO.vs");
        compileShader("PhongUBOVertex");
        
        attachShader("PhongUBOFragment", FRAGMENT);
        loadShaderSource("PhongUBOFragment", "shaders/PhongUBO.fs");
        compileShader("PhongUBOFragment");
        
        attachShaderToProgram("PhongUBO", "PhongUBOVertex");
        attachShaderToProgram("PhongUBO", "PhongUBOFragment");
        
        bindAttribute("PhongUBO", 0, "inVert");
        bindAttribute("PhongUBO", 1, "inUV");
        bindAttribute("PhongUBO", 2, "inNormal");
        
        linkProgramObject("PhongUBO");
        
        return m_wrappers["PhongUBO"].get();
    }
    
    // Auto-create SilkUBO shader with UBO support
    if (name == "SilkUBO") {
        createShaderProgram("SilkUBO");
        
        attachShader("SilkUBOVertex", VERTEX);
        loadShaderSource("SilkUBOVertex", "shaders/SilkUBO.vs");
        compileShader("SilkUBOVertex");
        
        attachShader("SilkUBOFragment", FRAGMENT);
        loadShaderSource("SilkUBOFragment", "shaders/SilkUBO.fs");
        compileShader("SilkUBOFragment");
        
        attachShaderToProgram("SilkUBO", "SilkUBOVertex");
        attachShaderToProgram("SilkUBO", "SilkUBOFragment");
        
        bindAttribute("SilkUBO", 0, "inVert");
        bindAttribute("SilkUBO", 2, "inNormal");
        
        linkProgramObject("SilkUBO");
        
        return m_wrappers["SilkUBO"].get();
    }
    
    // Auto-create SilkPBR_UBO shader with UBO support
    if (name == "SilkPBR_UBO") {
        createShaderProgram("SilkPBR_UBO");
        
        attachShader("SilkPBRUBOVertex", VERTEX);
        loadShaderSource("SilkPBRUBOVertex", "shaders/SilkPBR_UBO.vs");
        compileShader("SilkPBRUBOVertex");
        
        attachShader("SilkPBRUBOFragment", FRAGMENT);
        loadShaderSource("SilkPBRUBOFragment", "shaders/SilkPBR_UBO.fs");
        compileShader("SilkPBRUBOFragment");
        
        attachShaderToProgram("SilkPBR_UBO", "SilkPBRUBOVertex");
        attachShaderToProgram("SilkPBR_UBO", "SilkPBRUBOFragment");
        
        bindAttribute("SilkPBR_UBO", 0, "inVert");
        bindAttribute("SilkPBR_UBO", 2, "inNormal");
        
        linkProgramObject("SilkPBR_UBO");
        
        return m_wrappers["SilkPBR_UBO"].get();
    }
    
    // Auto-create a simple Phong shader if requested (legacy uniforms)
    if (name == "Phong") {
        // Create the shader program
        createShaderProgram("Phong");
        
        // Create and compile vertex shader
        attachShader("PhongVertex", VERTEX);
        loadShaderSource("PhongVertex", "shaders/Phong.vs");
        compileShader("PhongVertex");
        
        // Create and compile fragment shader
        attachShader("PhongFragment", FRAGMENT);
        loadShaderSource("PhongFragment", "shaders/Phong.fs");
        compileShader("PhongFragment");
        
        // Attach and link
        attachShaderToProgram("Phong", "PhongVertex");
        attachShaderToProgram("Phong", "PhongFragment");
        
        // Bind attributes
        bindAttribute("Phong", 0, "inVert");
        bindAttribute("Phong", 1, "inUV");
        bindAttribute("Phong", 2, "inNormal");
        
        linkProgramObject("Phong");
        
        return m_wrappers["Phong"].get();
    }
    
    // Auto-create Silk shader if requested
    if (name == "Silk") {
        // Create the shader program
        createShaderProgram("Silk");
        
        // Create and compile vertex shader
        attachShader("SilkVertex", VERTEX);
        loadShaderSource("SilkVertex", "shaders/Silk.vs");
        compileShader("SilkVertex");
        
        // Create and compile fragment shader
        attachShader("SilkFragment", FRAGMENT);
        loadShaderSource("SilkFragment", "shaders/Silk.fs");
        compileShader("SilkFragment");
        
        // Attach and link
        attachShaderToProgram("Silk", "SilkVertex");
        attachShaderToProgram("Silk", "SilkFragment");
        
        // Bind attributes
        bindAttribute("Silk", 0, "inVert");
        bindAttribute("Silk", 1, "inUV");
        bindAttribute("Silk", 2, "inNormal");
        
        linkProgramObject("Silk");
        
        return m_wrappers["Silk"].get();
    }
    
    // Auto-create SilkPBR shader for PBR fabric rendering
    if (name == "SilkPBR") {
        createShaderProgram("SilkPBR");
        
        attachShader("SilkPBRVertex", VERTEX);
        loadShaderSource("SilkPBRVertex", "shaders/SilkPBR.vs");
        compileShader("SilkPBRVertex");
        
        attachShader("SilkPBRFragment", FRAGMENT);
        loadShaderSource("SilkPBRFragment", "shaders/SilkPBR.fs");
        compileShader("SilkPBRFragment");
        
        attachShaderToProgram("SilkPBR", "SilkPBRVertex");
        attachShaderToProgram("SilkPBR", "SilkPBRFragment");
        
        bindAttribute("SilkPBR", 0, "inVert");
        bindAttribute("SilkPBR", 1, "inUV");
        bindAttribute("SilkPBR", 2, "inNormal");
        
        linkProgramObject("SilkPBR");
        
        return m_wrappers["SilkPBR"].get();
    }

    // Auto-create Refraction shader for per-mesh effects
    if (name == "Refraction") {
        createShaderProgram("Refraction");

        attachShader("RefractionVertex", VERTEX);
        loadShaderSource("RefractionVertex", "shaders/Refraction.vs");
        compileShader("RefractionVertex");

        attachShader("RefractionFragment", FRAGMENT);
        loadShaderSource("RefractionFragment", "shaders/Refraction.fs");
        compileShader("RefractionFragment");

        attachShaderToProgram("Refraction", "RefractionVertex");
        attachShaderToProgram("Refraction", "RefractionFragment");

        bindAttribute("Refraction", 0, "aPos");
        bindAttribute("Refraction", 1, "aTexCoords");
        bindAttribute("Refraction", 2, "aNormal");

        linkProgramObject("Refraction");

        return m_wrappers["Refraction"].get();
    }
    
    // Auto-create PhongInstanced shader for instanced particle rendering
    if (name == "PhongInstanced") {
        createShaderProgram("PhongInstanced");
        
        attachShader("PhongInstancedVertex", VERTEX);
        loadShaderSource("PhongInstancedVertex", "shaders/PhongInstanced.vs");
        compileShader("PhongInstancedVertex");
        
        // Reuse Phong fragment shader
        attachShader("PhongInstancedFragment", FRAGMENT);
        loadShaderSource("PhongInstancedFragment", "shaders/Phong.fs");
        compileShader("PhongInstancedFragment");
        
        attachShaderToProgram("PhongInstanced", "PhongInstancedVertex");
        attachShaderToProgram("PhongInstanced", "PhongInstancedFragment");
        
        // Standard attributes + instanced position at location 3
        bindAttribute("PhongInstanced", 0, "inVert");
        bindAttribute("PhongInstanced", 1, "inUV");
        bindAttribute("PhongInstanced", 2, "inNormal");
        bindAttribute("PhongInstanced", 3, "instancePos");
        
        linkProgramObject("PhongInstanced");
        
        return m_wrappers["PhongInstanced"].get();
    }
    
    std::cerr << "Shader program not found: " << name << std::endl;
    return nullptr;
}

unsigned int ShaderLib::glShaderType(int type) {
    switch (type) {
        case VERTEX: return GL_VERTEX_SHADER;
        case FRAGMENT: return GL_FRAGMENT_SHADER;
        case GEOMETRY: return GL_GEOMETRY_SHADER;
        case COMPUTE: return GL_COMPUTE_SHADER;
        default: return GL_VERTEX_SHADER;
    }
}

bool ShaderLib::checkShaderCompilation(unsigned int shaderId, const std::string& name) {
    int success;
    glGetShaderiv(shaderId, GL_COMPILE_STATUS, &success);
    
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shaderId, 512, nullptr, infoLog);
        std::cerr << "Shader compilation error (" << name << "): " << infoLog << std::endl;
        return false;
    }
    return true;
}

bool ShaderLib::checkProgramLinking(unsigned int programId, const std::string& name) {
    int success;
    glGetProgramiv(programId, GL_LINK_STATUS, &success);
    
    if (!success) {
        int logLength;
        glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &logLength);
        if (logLength > 0) {
            std::vector<char> infoLog(logLength);
            glGetProgramInfoLog(programId, logLength, nullptr, infoLog.data());
            std::cerr << "Program linking error (" << name << "): " << infoLog.data() << std::endl;
        }
        return false;
    }
    return true;
}

bool ShaderLib::loadShaderFromFile(const std::string& filename, std::string& source) {
    std::string exeDir = getExecutableDir();
    std::vector<std::string> possiblePaths = {
        exeDir + filename,
        exeDir + "shaders/" + filename,
        exeDir + "modules/graphics_engine/" + filename,
        exeDir + "modules/graphics_engine/shaders/" + filename,
        filename,
        "shaders/" + filename,
        "modules/graphics_engine/" + filename,
        "modules/graphics_engine/shaders/" + filename,
        "../" + filename,
        "../shaders/" + filename,
        "../modules/graphics_engine/shaders/" + filename,
        "../../" + filename,
        "../../shaders/" + filename
    };
    
    for (const auto& path : possiblePaths) {
        std::ifstream file(path);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            source = buffer.str();
            // Downgrade GLSL version if necessary (WSLg often supports up to 4.20)
            const char* highVer = "#version 460";
            const char* downgraded = "#version 420";
            size_t pos = source.find(highVer);
            if (pos != std::string::npos) {
                source.replace(pos, std::strlen(highVer), downgraded);
            }
            file.close();
            if (!source.empty()) {
                return true;
            }
        }
    }
    
    std::cerr << "Cannot open shader file: " << filename << std::endl;
    return false;
}

// UBO functions
unsigned int ShaderLib::createUBO(const std::string& name, size_t size) {
    if (m_device) {
        auto buffer = m_device->createBuffer(size, rhi::BufferUsage::Dynamic);
        if (buffer) {
            unsigned int id = static_cast<unsigned int>(buffer->nativeHandle());
            m_ubos[name] = id;
            m_uboBuffers[name] = std::move(buffer);
            return id;
        }
    }

    unsigned int ubo;
    glGenBuffers(1, &ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    glBufferData(GL_UNIFORM_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    m_ubos[name] = ubo;
    return ubo;
}

void ShaderLib::bindUBOToBindingPoint(const std::string& uboName, unsigned int bindingPoint) {
    auto it = m_ubos.find(uboName);
    if (it != m_ubos.end()) {
        glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, it->second);
    }
}

void ShaderLib::updateUBO(const std::string& name, const void* data, size_t size, size_t offset) {
    auto it = m_ubos.find(name);
    if (it == m_ubos.end()) {
        std::cerr << "UBO not found: " << name << std::endl;
        return;
    }

    auto bufferIt = m_uboBuffers.find(name);
    if (bufferIt != m_uboBuffers.end()) {
        bufferIt->second->update(data, size, offset);
        return;
    }
    
    glBindBuffer(GL_UNIFORM_BUFFER, it->second);
    glBufferSubData(GL_UNIFORM_BUFFER, offset, size, data);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void ShaderLib::bindUniformBlockToBindingPoint(const std::string& programName, const std::string& blockName, unsigned int bindingPoint) {
    auto programIt = m_programs.find(programName);
    if (programIt == m_programs.end()) {
        std::cerr << "Program not found: " << programName << std::endl;
        return;
    }
    
    unsigned int blockIndex = glGetUniformBlockIndex(programIt->second, blockName.c_str());
    if (blockIndex != GL_INVALID_INDEX) {
        glUniformBlockBinding(programIt->second, blockIndex, bindingPoint);
    }
}

void ShaderLib::deleteUBO(const std::string& name) {
    auto it = m_ubos.find(name);
    if (it == m_ubos.end()) {
        return;
    }

    auto bufferIt = m_uboBuffers.find(name);
    if (bufferIt != m_uboBuffers.end()) {
        m_uboBuffers.erase(bufferIt);
        m_ubos.erase(it);
        return;
    }

    glDeleteBuffers(1, &it->second);
    m_ubos.erase(it);
}

} // namespace sandbox
