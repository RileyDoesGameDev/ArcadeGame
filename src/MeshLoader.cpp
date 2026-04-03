#include "MeshLoader.h"
#include <fstream>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
#include <cctype>
#include <cmath>

// ---------------------------------------------------------------------------
// Lightweight JSON helpers (same pattern as GLBLoader, self-contained)
// ---------------------------------------------------------------------------

static size_t skipWS(const std::string& s, size_t p) {
    while (p < s.size() &&
           (s[p] == ' ' || s[p] == '\t' || s[p] == '\n' || s[p] == '\r')) ++p;
    return p;
}

static bool findKey(const std::string& json, const std::string& key,
                    size_t from, size_t& vs, size_t& ve)
{
    std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle, from);
    if (pos == std::string::npos) return false;
    pos = skipWS(json, pos + needle.size());
    if (pos >= json.size() || json[pos] != ':') return false;
    pos = skipWS(json, pos + 1);
    if (pos >= json.size()) return false;
    vs = pos;
    char open = json[pos];
    if (open == '[' || open == '{') {
        char close = (open == '[') ? ']' : '}';
        int depth = 1; ++pos;
        while (pos < json.size() && depth > 0) {
            if      (json[pos] == open)  ++depth;
            else if (json[pos] == close) --depth;
            ++pos;
        }
        ve = pos;
    } else {
        while (pos < json.size() && json[pos] != ',' &&
               json[pos] != '}' && json[pos] != ']' && json[pos] != '\n') ++pos;
        ve = pos;
    }
    return true;
}

// Return the Nth { } object inside a JSON array string
static std::string getNthObj(const std::string& arr, int n)
{
    int count = 0;
    size_t pos = 0;
    while (pos < arr.size()) {
        while (pos < arr.size() && arr[pos] != '{') ++pos;
        if (pos >= arr.size()) break;
        size_t start = pos;
        int depth = 1; ++pos;
        while (pos < arr.size() && depth > 0) {
            if      (arr[pos] == '{') ++depth;
            else if (arr[pos] == '}') --depth;
            ++pos;
        }
        if (count == n) return arr.substr(start, pos - start);
        ++count;
    }
    return "";
}

// Count { } objects in a JSON array string
static int countObjs(const std::string& arr)
{
    int count = 0;
    size_t pos = 0;
    while (pos < arr.size()) {
        while (pos < arr.size() && arr[pos] != '{') ++pos;
        if (pos >= arr.size()) break;
        int depth = 1; ++pos;
        while (pos < arr.size() && depth > 0) {
            if      (arr[pos] == '{') ++depth;
            else if (arr[pos] == '}') --depth;
            ++pos;
        }
        ++count;
    }
    return count;
}

static std::string getSubStr(const std::string& json, const std::string& key)
{
    size_t vs, ve;
    if (!findKey(json, key, 0, vs, ve)) return "";
    return json.substr(vs, ve - vs);
}

static int getInt(const std::string& json, const std::string& key, int def = -1)
{
    size_t vs, ve;
    if (!findKey(json, key, 0, vs, ve)) return def;
    std::string v = json.substr(vs, ve - vs);
    while (!v.empty() && (v.back() == ' ' || v.back() == '\r')) v.pop_back();
    try { return std::stoi(v); } catch (...) { return def; }
}

static std::vector<float> getFloatArr(const std::string& json, const std::string& key)
{
    size_t vs, ve;
    if (!findKey(json, key, 0, vs, ve)) return {};
    std::string arr = json.substr(vs, ve - vs);
    std::vector<float> result;
    size_t i = 0;
    while (i < arr.size()) {
        while (i < arr.size() && arr[i] != '-' &&
               !std::isdigit(static_cast<unsigned char>(arr[i]))) ++i;
        if (i >= arr.size()) break;
        size_t j = i;
        if (arr[j] == '-') ++j;
        while (j < arr.size() && (std::isdigit(static_cast<unsigned char>(arr[j])) ||
               arr[j]=='.' || arr[j]=='e' || arr[j]=='E' || arr[j]=='+' || arr[j]=='-')) ++j;
        try { result.push_back(std::stof(arr.substr(i, j-i))); } catch (...) {}
        i = j;
    }
    return result;
}

// ---------------------------------------------------------------------------
// Load one primitive's geometry into outVerts / outIndices.
// indexOffset is added to all indices so we can merge multiple meshes.
// ---------------------------------------------------------------------------
static void loadPrimitive(const std::string& prim,
                          const std::string& accessorsArr,
                          const std::string& bufViewsArr,
                          const std::vector<uint8_t>& bin,
                          float tx, float ty, float tz,
                          float sx, float sy, float sz,
                          float qx, float qy, float qz, float qw,
                          float cr, float cg, float cb, float ca,
                          uint32_t indexOffset,
                          std::vector<MeshData::Vertex>& outVerts,
                          std::vector<uint32_t>& outIdx)
{
    std::string attribs = getSubStr(prim, "attributes");
    int posAccIdx = getInt(attribs, "POSITION");
    int idxAccIdx = getInt(prim, "indices");
    if (posAccIdx < 0 || idxAccIdx < 0) return;

    // ---- Position accessor ----
    std::string posAcc  = getNthObj(accessorsArr, posAccIdx);
    int posBV           = getInt(posAcc, "bufferView", 0);
    int posCount        = getInt(posAcc, "count", 0);
    int posByteOff      = getInt(posAcc, "byteOffset", 0);
    if (posByteOff < 0) posByteOff = 0;

    std::string posBVObj = getNthObj(bufViewsArr, posBV);
    int posBVOff   = getInt(posBVObj, "byteOffset", 0);
    int posBVStride = getInt(posBVObj, "byteStride");
    if (posBVStride <= 0) posBVStride = 12; // VEC3 FLOAT = 3×4 bytes
    if (posBVOff < 0) posBVOff = 0;

    // Quaternion rotate helper (inline lambda)
    auto qRot = [&](float vx, float vy, float vz, float& ox, float& oy, float& oz) {
        float tx2 = 2.0f * (qy*vz - qz*vy);
        float ty2 = 2.0f * (qz*vx - qx*vz);
        float tz2 = 2.0f * (qx*vy - qy*vx);
        ox = vx + qw*tx2 + (qy*tz2 - qz*ty2);
        oy = vy + qw*ty2 + (qz*tx2 - qx*tz2);
        oz = vz + qw*tz2 + (qx*ty2 - qy*tx2);
    };

    uint32_t vertBase = static_cast<uint32_t>(outVerts.size());
    outVerts.reserve(outVerts.size() + posCount);

    for (int i = 0; i < posCount; ++i) {
        int off = posBVOff + posByteOff + i * posBVStride;
        if (off + 12 > static_cast<int>(bin.size())) break;
        float lx, ly, lz;
        std::memcpy(&lx, &bin[off + 0], 4);
        std::memcpy(&ly, &bin[off + 4], 4);
        std::memcpy(&lz, &bin[off + 8], 4);

        // Apply TRS transform: world = rotate(scale * local) + translate
        float rx2, ry2, rz2;
        qRot(lx*sx, ly*sy, lz*sz, rx2, ry2, rz2);
        outVerts.push_back({ rx2 + tx, ry2 + ty, rz2 + tz, cr, cg, cb, ca });
    }

    // ---- Index accessor ----
    std::string idxAcc  = getNthObj(accessorsArr, idxAccIdx);
    int idxBV           = getInt(idxAcc, "bufferView", 0);
    int idxCount        = getInt(idxAcc, "count", 0);
    int idxByteOff      = getInt(idxAcc, "byteOffset", 0);
    int idxCompType     = getInt(idxAcc, "componentType"); // 5123=USHORT 5125=UINT
    if (idxByteOff < 0) idxByteOff = 0;

    std::string idxBVObj = getNthObj(bufViewsArr, idxBV);
    int idxBVOff = getInt(idxBVObj, "byteOffset", 0);
    if (idxBVOff < 0) idxBVOff = 0;

    int elemSize = (idxCompType == 5125) ? 4 : 2;
    outIdx.reserve(outIdx.size() + idxCount);

    for (int i = 0; i < idxCount; ++i) {
        int off = idxBVOff + idxByteOff + i * elemSize;
        if (off + elemSize > static_cast<int>(bin.size())) break;
        uint32_t idx = 0;
        if (idxCompType == 5125) {
            std::memcpy(&idx, &bin[off], 4);
        } else {
            uint16_t u16;
            std::memcpy(&u16, &bin[off], 2);
            idx = u16;
        }
        outIdx.push_back(vertBase + indexOffset + idx);
    }
}

// ---------------------------------------------------------------------------
MeshData MeshLoader::loadMesh(const std::string& filepath)
{
    MeshData result;

    std::ifstream f(filepath, std::ios::binary);
    if (!f.is_open()) {
        std::cerr << "[MeshLoader] Cannot open: " << filepath << '\n';
        return result;
    }

    // ---- GLB header ----
    uint32_t magic, version, totalLen;
    f.read(reinterpret_cast<char*>(&magic),    4);
    f.read(reinterpret_cast<char*>(&version),  4);
    f.read(reinterpret_cast<char*>(&totalLen), 4);
    if (magic != 0x46546C67u) {
        std::cerr << "[MeshLoader] Not a valid GLB file\n";
        return result;
    }

    // ---- JSON chunk ----
    uint32_t c0len, c0type;
    f.read(reinterpret_cast<char*>(&c0len),  4);
    f.read(reinterpret_cast<char*>(&c0type), 4);
    if (c0type != 0x4E4F534Au) {
        std::cerr << "[MeshLoader] First chunk is not JSON\n";
        return result;
    }
    std::string json(c0len, '\0');
    f.read(&json[0], static_cast<std::streamsize>(c0len));

    // ---- Binary chunk (4-byte aligned after JSON chunk) ----
    uint32_t jsonEnd  = 12 + 8 + c0len;
    uint32_t binStart = (jsonEnd + 3u) & ~3u;
    f.seekg(static_cast<std::streamoff>(binStart));
    uint32_t c1len, c1type;
    f.read(reinterpret_cast<char*>(&c1len),  4);
    f.read(reinterpret_cast<char*>(&c1type), 4);
    std::vector<uint8_t> bin(c1len);
    f.read(reinterpret_cast<char*>(bin.data()), static_cast<std::streamsize>(c1len));

    // ---- Pre-extract shared arrays ----
    std::string meshesArr    = getSubStr(json, "meshes");
    std::string accessorsArr = getSubStr(json, "accessors");
    std::string bufViewsArr  = getSubStr(json, "bufferViews");

    // ---- Parse material base colours (pbrMetallicRoughness.baseColorFactor) ----
    struct MatColor { float r, g, b, a; };
    std::vector<MatColor> matColors;
    {
        size_t vs, ve;
        if (findKey(json, "materials", 0, vs, ve)) {
            std::string matsArr = json.substr(vs, ve - vs);
            int n = countObjs(matsArr);
            std::cout << "[MeshLoader] Parsing " << n << " materials\n";
            for (int i = 0; i < n; ++i) {
                std::string mat = getNthObj(matsArr, i);
                // Get the pbrMetallicRoughness object, then baseColorFactor inside it.
                // We search the raw material string directly for baseColorFactor to
                // avoid a double-extraction that could go wrong.
                auto cf = getFloatArr(mat, "baseColorFactor");
                MatColor mc;
                if      (cf.size() >= 4) mc = {cf[0], cf[1], cf[2], cf[3]};
                else if (cf.size() >= 3) mc = {cf[0], cf[1], cf[2], 1.f};
                else                     mc = {1.f, 1.f, 1.f, 1.f};
                matColors.push_back(mc);
                std::cout << "  mat[" << i << "] rgba=("
                          << mc.r << "," << mc.g << "," << mc.b << "," << mc.a << ")\n";
            }
        } else {
            std::cout << "[MeshLoader] No 'materials' key found — using white\n";
        }
    }

    // "nodes" also appears nested inside the "scenes" array as an integer list.
    // Skip past "scenes" first so we find the real top-level object array.
    std::string nodesArr;
    {
        size_t from = 0, tempVS, tempVE;
        if (findKey(json, "scenes", 0, tempVS, tempVE)) from = tempVE;
        if (findKey(json, "nodes", from, tempVS, tempVE))
            nodesArr = json.substr(tempVS, tempVE - tempVS);
    }

    int nodeCount = countObjs(nodesArr);

    // ---- Walk every node; process those that reference a mesh ----
    for (int ni = 0; ni < nodeCount; ++ni) {
        std::string node = getNthObj(nodesArr, ni);

        int meshIdx = getInt(node, "mesh");
        if (meshIdx < 0) continue; // camera or other non-mesh node

        // Node transform
        float tx = 0, ty = 0, tz = 0;
        float sx = 1, sy = 1, sz = 1;
        float qx = 0, qy = 0, qz = 0, qw = 1;

        auto t = getFloatArr(node, "translation");
        if (t.size() >= 3) { tx = t[0]; ty = t[1]; tz = t[2]; }

        auto s = getFloatArr(node, "scale");
        if (s.size() >= 3) { sx = s[0]; sy = s[1]; sz = s[2]; }

        auto r = getFloatArr(node, "rotation");
        if (r.size() >= 4) { qx = r[0]; qy = r[1]; qz = r[2]; qw = r[3]; }

        // Get this mesh's primitives
        std::string meshObj  = getNthObj(meshesArr, meshIdx);
        std::string primsArr = getSubStr(meshObj, "primitives");
        int primCount        = countObjs(primsArr);

        for (int pi = 0; pi < primCount; ++pi) {
            std::string prim = getNthObj(primsArr, pi);

            // Look up this primitive's material colour (default white if missing)
            int matIdx = getInt(prim, "material", -1);
            float cr = 1.f, cg = 1.f, cb = 1.f, ca = 1.f;
            if (matIdx >= 0 && matIdx < (int)matColors.size()) {
                cr = matColors[matIdx].r;
                cg = matColors[matIdx].g;
                cb = matColors[matIdx].b;
                ca = matColors[matIdx].a;
            }

            loadPrimitive(prim, accessorsArr, bufViewsArr, bin,
                          tx, ty, tz, sx, sy, sz, qx, qy, qz, qw,
                          cr, cg, cb, ca,
                          0u,
                          result.vertices, result.indices);
        }

        std::cout << "[MeshLoader] Node " << ni << " mesh " << meshIdx
                  << " loaded (" << primCount << " primitive(s))\n";
    }

    if (result.vertices.empty()) {
        std::cerr << "[MeshLoader] No mesh geometry found in " << filepath << '\n';
        return result;
    }

    result.valid = true;
    std::cout << "[MeshLoader] Total: " << result.vertices.size() << " verts, "
              << (result.indices.size() / 3) << " triangles\n";
    return result;
}
