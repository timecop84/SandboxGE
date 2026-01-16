#pragma once

#include <string>
#include <vector>

namespace cloth {
namespace io {

struct ObjMesh {
    std::vector<float> positions;   // xyz triples
    std::vector<float> normals;     // xyz triples
    std::vector<float> uvs;         // uv pairs
    std::vector<uint32_t> indices;  // triangle indices
};

bool loadOBJ(const std::string& path, ObjMesh& mesh, std::string& error);

} // namespace io
} // namespace cloth
