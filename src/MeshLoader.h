#pragma once
#include <string>
#include <vector>
#include <cstdint>

// ---------------------------------------------------------------------------
// MeshData
//   Vertex positions in world space (node transform already applied).
//   indices is a flat triangle list: every 3 entries = one triangle.
// ---------------------------------------------------------------------------
struct MeshData {
    // Position (world space, node transform applied) + material base colour.
    struct Vertex { float x, y, z; float r, g, b, a; };

    std::vector<Vertex>   vertices;
    std::vector<uint32_t> indices;   // 3 per triangle

    bool valid = false;
};

// ---------------------------------------------------------------------------
// MeshLoader
//   Reads all mesh nodes from a binary glTF (.glb) file, applies their
//   node transforms, and merges them into a single MeshData.
// ---------------------------------------------------------------------------
class MeshLoader {
public:
    static MeshData loadMesh(const std::string& filepath);
};
