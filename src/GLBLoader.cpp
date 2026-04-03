#include "GLBLoader.h"
#include <fstream>
#include <cstdint>
#include <iostream>
#include <vector>
#include <cctype>

// ---------------------------------------------------------------------------
// Minimal JSON scanner – only handles what Blender's glTF exporter produces.
// ---------------------------------------------------------------------------

static size_t skipWS(const std::string& s, size_t pos) {
    while (pos < s.size() &&
           (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n' || s[pos] == '\r'))
        ++pos;
    return pos;
}

// Locate the value span for `key` inside `json` starting at `from`.
// Returns false if not found.  valueStart/End span the raw value text
// (including brackets for arrays/objects).
static bool findKey(const std::string& json, const std::string& key,
                    size_t from,
                    size_t& valueStart, size_t& valueEnd)
{
    std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle, from);
    if (pos == std::string::npos) return false;

    pos = skipWS(json, pos + needle.size());
    if (pos >= json.size() || json[pos] != ':') return false;
    pos = skipWS(json, pos + 1);
    if (pos >= json.size()) return false;

    valueStart = pos;
    char opener = json[pos];

    if (opener == '[' || opener == '{') {
        char closer = (opener == '[') ? ']' : '}';
        int depth = 1;
        ++pos;
        while (pos < json.size() && depth > 0) {
            if (json[pos] == opener)  ++depth;
            else if (json[pos] == closer) --depth;
            ++pos;
        }
        valueEnd = pos;
    } else {
        // Number, bool, null, or quoted string
        while (pos < json.size() &&
               json[pos] != ',' && json[pos] != '}' &&
               json[pos] != ']' && json[pos] != '\n')
            ++pos;
        valueEnd = pos;
    }
    return true;
}

// Parse a JSON float array "[f, f, f, ...]" into a vector<float>.
static std::vector<float> parseFloatArray(const std::string& s,
                                          size_t start, size_t end)
{
    std::vector<float> out;
    size_t i = start;
    while (i < end) {
        // Skip to start of a number (digit or leading minus)
        while (i < end && s[i] != '-' && !std::isdigit(static_cast<unsigned char>(s[i])))
            ++i;
        if (i >= end) break;

        size_t j = i;
        if (s[j] == '-') ++j;
        while (j < end && (std::isdigit(static_cast<unsigned char>(s[j])) ||
                            s[j] == '.' || s[j] == 'e' || s[j] == 'E' ||
                            s[j] == '+' || s[j] == '-'))
            ++j;

        try { out.push_back(std::stof(s.substr(i, j - i))); }
        catch (...) {}
        i = j;
    }
    return out;
}

static float parseScalar(const std::string& s, size_t start, size_t end) {
    std::string v = s.substr(start, end - start);
    while (!v.empty() && (v.back() == ' ' || v.back() == '\r' || v.back() == '\n'))
        v.pop_back();
    try { return std::stof(v); } catch (...) { return 0.0f; }
}

// ---------------------------------------------------------------------------
Camera GLBLoader::loadCamera(const std::string& filepath)
{
    Camera cam;

    std::ifstream f(filepath, std::ios::binary);
    if (!f.is_open()) {
        std::cerr << "[GLBLoader] Cannot open: " << filepath << '\n';
        return cam;
    }

    // ---- GLB header (12 bytes) ------------------------------------------
    uint32_t magic, version, totalLen;
    f.read(reinterpret_cast<char*>(&magic),    4);
    f.read(reinterpret_cast<char*>(&version),  4);
    f.read(reinterpret_cast<char*>(&totalLen), 4);

    constexpr uint32_t GLB_MAGIC = 0x46546C67u; // "glTF"
    if (magic != GLB_MAGIC) {
        std::cerr << "[GLBLoader] Not a valid GLB (bad magic)\n";
        return cam;
    }

    // ---- JSON chunk (first chunk) ---------------------------------------
    uint32_t chunkLen, chunkType;
    f.read(reinterpret_cast<char*>(&chunkLen),  4);
    f.read(reinterpret_cast<char*>(&chunkType), 4);

    constexpr uint32_t CHUNK_JSON = 0x4E4F534Au; // "JSON"
    if (chunkType != CHUNK_JSON) {
        std::cerr << "[GLBLoader] First GLB chunk is not JSON\n";
        return cam;
    }

    std::string json(chunkLen, '\0');
    f.read(&json[0], static_cast<std::streamsize>(chunkLen));

    // ---- Parse camera perspective parameters ----------------------------
    {
        size_t vs, ve;
        if (findKey(json, "perspective", 0, vs, ve)) {
            std::string persp = json.substr(vs, ve - vs);
            size_t pvs, pve;
            if (findKey(persp, "yfov",  0, pvs, pve)) cam.fovY      = parseScalar(persp, pvs, pve);
            if (findKey(persp, "znear", 0, pvs, pve)) cam.nearPlane = parseScalar(persp, pvs, pve);
            if (findKey(persp, "zfar",  0, pvs, pve)) cam.farPlane  = parseScalar(persp, pvs, pve);
        }
    }

    // ---- Scan nodes for the one with a "camera" reference --------------
    // IMPORTANT: "nodes" also appears nested inside the "scenes" array.
    // We must skip past "scenes" first so findKey returns the real top-level
    // "nodes" array, not the integer-index list inside "scenes".
    size_t nodesVS, nodesVE;
    {
        size_t from = 0;
        size_t tempVS, tempVE;
        if (findKey(json, "scenes", 0, tempVS, tempVE)) from = tempVE;
        if (!findKey(json, "nodes", from, nodesVS, nodesVE)) {
            std::cerr << "[GLBLoader] No 'nodes' array in GLB\n";
            return cam;
        }
    }

    // Walk through each { … } object inside the nodes array
    size_t pos = nodesVS;
    while (pos < nodesVE) {
        while (pos < nodesVE && json[pos] != '{') ++pos;
        if (pos >= nodesVE) break;

        // Find the matching closing brace
        size_t nodeStart = pos;
        int depth = 1;
        ++pos;
        while (pos < nodesVE && depth > 0) {
            if      (json[pos] == '{') ++depth;
            else if (json[pos] == '}') --depth;
            ++pos;
        }
        size_t nodeEnd = pos;

        std::string node = json.substr(nodeStart, nodeEnd - nodeStart);

        // Only care about nodes that have a "camera" field
        size_t dummy1, dummy2;
        if (!findKey(node, "camera", 0, dummy1, dummy2)) continue;

        // Extract translation
        size_t tVS, tVE;
        if (findKey(node, "translation", 0, tVS, tVE)) {
            auto t = parseFloatArray(node, tVS, tVE);
            if (t.size() >= 3) { cam.px = t[0]; cam.py = t[1]; cam.pz = t[2]; }
        }

        // Extract rotation (quaternion [x,y,z,w])
        size_t rVS, rVE;
        if (findKey(node, "rotation", 0, rVS, rVE)) {
            auto r = parseFloatArray(node, rVS, rVE);
            if (r.size() >= 4) { cam.qx = r[0]; cam.qy = r[1]; cam.qz = r[2]; cam.qw = r[3]; }
        }

        cam.valid = true;

        std::cout << "[GLBLoader] Camera loaded from \"" << filepath << "\"\n"
                  << "  position : (" << cam.px << ", " << cam.py << ", " << cam.pz << ")\n"
                  << "  rotation : (" << cam.qx << ", " << cam.qy << ", " << cam.qz << ", " << cam.qw << ")\n"
                  << "  fovY     : " << cam.fovY << " rad (" << (cam.fovY * 57.2958f) << " deg)\n"
                  << "  near/far : " << cam.nearPlane << " / " << cam.farPlane << '\n';
        break;
    }

    if (!cam.valid)
        std::cerr << "[GLBLoader] No camera node found in GLB\n";

    return cam;
}
