#include "io/ObjParser.h"

#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace cloth {
namespace io {

namespace {
struct VertexKey {
    int position = -1;
    int texcoord = -1;
    int normal = -1;

    bool operator==(const VertexKey& other) const {
        return position == other.position &&
               texcoord == other.texcoord &&
               normal == other.normal;
    }
};

struct VertexKeyHash {
    size_t operator()(const VertexKey& key) const {
        size_t h = static_cast<size_t>(key.position & 0xFFFF);
        h = (h << 21) ^ static_cast<size_t>(key.texcoord & 0xFFFF);
        h = (h << 21) ^ static_cast<size_t>(key.normal & 0xFFFF);
        return h;
    }
};

int resolveIndex(int idx, int count) {
    if (idx > 0) return idx - 1;
    if (idx < 0) return count + idx;
    return -1;
}

bool parseFaceToken(const std::string& token,
                    int& positionIndex,
                    int& texIndex,
                    int& normalIndex) {
    positionIndex = 0;
    texIndex = 0;
    normalIndex = 0;
    try {
        size_t firstSlash = token.find('/');
        if (firstSlash == std::string::npos) {
            positionIndex = std::stoi(token);
            return true;
        }

        std::string posPart = token.substr(0, firstSlash);
        if (!posPart.empty()) {
            positionIndex = std::stoi(posPart);
        }

        size_t secondSlash = token.find('/', firstSlash + 1);
        std::string texPart;
        std::string normPart;
        if (secondSlash == std::string::npos) {
            texPart = token.substr(firstSlash + 1);
        } else {
            if (secondSlash > firstSlash + 1) {
                texPart = token.substr(firstSlash + 1, secondSlash - firstSlash - 1);
            }
            if (secondSlash + 1 < token.size()) {
                normPart = token.substr(secondSlash + 1);
            }
        }

        if (!texPart.empty()) {
            texIndex = std::stoi(texPart);
        }
        if (!normPart.empty()) {
            normalIndex = std::stoi(normPart);
        }
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void ensureNormals(cloth::io::ObjMesh& mesh) {
    if (mesh.positions.empty() || mesh.indices.empty()) return;
    if (mesh.normals.size() == mesh.positions.size()) return;

    mesh.normals.assign(mesh.positions.size(), 0.0f);
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        uint32_t i0 = mesh.indices[i];
        uint32_t i1 = mesh.indices[i + 1];
        uint32_t i2 = mesh.indices[i + 2];
        size_t p0 = static_cast<size_t>(i0) * 3;
        size_t p1 = static_cast<size_t>(i1) * 3;
        size_t p2 = static_cast<size_t>(i2) * 3;
        float ax = mesh.positions[p1]     - mesh.positions[p0];
        float ay = mesh.positions[p1 + 1] - mesh.positions[p0 + 1];
        float az = mesh.positions[p1 + 2] - mesh.positions[p0 + 2];
        float bx = mesh.positions[p2]     - mesh.positions[p0];
        float by = mesh.positions[p2 + 1] - mesh.positions[p0 + 1];
        float bz = mesh.positions[p2 + 2] - mesh.positions[p0 + 2];
        float nx = ay * bz - az * by;
        float ny = az * bx - ax * bz;
        float nz = ax * by - ay * bx;
        mesh.normals[p0]     += nx;
        mesh.normals[p0 + 1] += ny;
        mesh.normals[p0 + 2] += nz;
        mesh.normals[p1]     += nx;
        mesh.normals[p1 + 1] += ny;
        mesh.normals[p1 + 2] += nz;
        mesh.normals[p2]     += nx;
        mesh.normals[p2 + 1] += ny;
        mesh.normals[p2 + 2] += nz;
    }

    for (size_t i = 0; i < mesh.normals.size(); i += 3) {
        float nx = mesh.normals[i];
        float ny = mesh.normals[i + 1];
        float nz = mesh.normals[i + 2];
        float len = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (len > 0.00001f) {
            mesh.normals[i]     = nx / len;
            mesh.normals[i + 1] = ny / len;
            mesh.normals[i + 2] = nz / len;
        } else {
            mesh.normals[i] = 0.0f;
            mesh.normals[i + 1] = 1.0f;
            mesh.normals[i + 2] = 0.0f;
        }
    }
}
} // namespace

bool loadOBJ(const std::string& path, ObjMesh& mesh, std::string& error) {
    std::ifstream file(path);
    if (!file.is_open()) {
        error = "Failed to open OBJ file: " + path;
        return false;
    }

    std::vector<float> rawPositions;
    std::vector<float> rawTexcoords;
    std::vector<float> rawNormals;
    std::string line;

    struct FaceVertex { int posIndex; int texIndex; int normIndex; };

    std::vector<FaceVertex> faceVertices;
    std::unordered_map<VertexKey, uint32_t, VertexKeyHash> vertexMap;

    auto addVertex = [&](int posIndex, int texIndex, int normIndex) -> uint32_t {
        VertexKey key{posIndex, texIndex, normIndex};
        auto it = vertexMap.find(key);
        if (it != vertexMap.end()) {
            return it->second;
        }

        size_t rawPosIndex = static_cast<size_t>(posIndex) * 3;
        mesh.positions.push_back(rawPositions[rawPosIndex + 0]);
        mesh.positions.push_back(rawPositions[rawPosIndex + 1]);
        mesh.positions.push_back(rawPositions[rawPosIndex + 2]);

        if (!rawNormals.empty() && normIndex >= 0) {
            size_t rawNormIndex = static_cast<size_t>(normIndex) * 3;
            mesh.normals.push_back(rawNormals[rawNormIndex + 0]);
            mesh.normals.push_back(rawNormals[rawNormIndex + 1]);
            mesh.normals.push_back(rawNormals[rawNormIndex + 2]);
        } else {
            mesh.normals.push_back(0.0f);
            mesh.normals.push_back(1.0f);
            mesh.normals.push_back(0.0f);
        }

        if (!rawTexcoords.empty() && texIndex >= 0) {
            size_t rawTexIndex = static_cast<size_t>(texIndex) * 2;
            mesh.uvs.push_back(rawTexcoords[rawTexIndex + 0]);
            mesh.uvs.push_back(rawTexcoords[rawTexIndex + 1]);
        } else {
            mesh.uvs.push_back(0.0f);
            mesh.uvs.push_back(0.0f);
        }

        uint32_t newIndex = static_cast<uint32_t>(mesh.positions.size() / 3 - 1);
        vertexMap[key] = newIndex;
        return newIndex;
    };

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream ss(line);
        std::string tag;
        ss >> tag;
        if (tag == "v") {
            float x, y, z;
            ss >> x >> y >> z;
            rawPositions.push_back(x);
            rawPositions.push_back(y);
            rawPositions.push_back(z);
        } else if (tag == "vn") {
            float nx, ny, nz;
            ss >> nx >> ny >> nz;
            rawNormals.push_back(nx);
            rawNormals.push_back(ny);
            rawNormals.push_back(nz);
        } else if (tag == "vt") {
            float u, v;
            ss >> u >> v;
            rawTexcoords.push_back(u);
            rawTexcoords.push_back(v);
        } else if (tag == "f") {
            faceVertices.clear();
            std::string vertToken;
            while (ss >> vertToken) {
                int positionIndex = 0;
                int texIndex = 0;
                int normalIndex = 0;
                if (!parseFaceToken(vertToken, positionIndex, texIndex, normalIndex)) {
                    error = "Malformed face token: " + vertToken;
                    return false;
                }
                int resolvedPos = resolveIndex(positionIndex, static_cast<int>(rawPositions.size() / 3));
                if (resolvedPos < 0 || resolvedPos * 3 >= static_cast<int>(rawPositions.size())) {
                    error = "Position index out of range in face definition.";
                    return false;
                }
                int resolvedTex = -1;
                if (!rawTexcoords.empty() && texIndex != 0) {
                    resolvedTex = resolveIndex(texIndex, static_cast<int>(rawTexcoords.size() / 2));
                    if (resolvedTex < 0 || resolvedTex * 2 >= static_cast<int>(rawTexcoords.size())) {
                        error = "Texcoord index out of range in face definition.";
                        return false;
                    }
                }
                int resolvedNorm = -1;
                if (!rawNormals.empty() && normalIndex != 0) {
                    resolvedNorm = resolveIndex(normalIndex, static_cast<int>(rawNormals.size() / 3));
                    if (resolvedNorm < 0 || resolvedNorm * 3 >= static_cast<int>(rawNormals.size())) {
                        error = "Normal index out of range in face definition.";
                        return false;
                    }
                }
                faceVertices.push_back({resolvedPos, resolvedTex, resolvedNorm});
            }

            if (faceVertices.size() < 3) {
                continue;
            }

            // Fan triangulation
            uint32_t first = addVertex(faceVertices[0].posIndex,
                                       faceVertices[0].texIndex,
                                       faceVertices[0].normIndex);
            for (size_t i = 1; i + 1 < faceVertices.size(); ++i) {
                uint32_t second = addVertex(faceVertices[i].posIndex,
                                            faceVertices[i].texIndex,
                                            faceVertices[i].normIndex);
                uint32_t third = addVertex(faceVertices[i + 1].posIndex,
                                           faceVertices[i + 1].texIndex,
                                           faceVertices[i + 1].normIndex);
                mesh.indices.push_back(first);
                mesh.indices.push_back(second);
                mesh.indices.push_back(third);
            }
        }
    }

    if (mesh.positions.empty() || mesh.indices.empty()) {
        error = "OBJ file contains no geometry: " + path;
        return false;
    }

    ensureNormals(mesh);

    return true;
}

} // namespace io
} // namespace cloth
