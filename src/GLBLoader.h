#pragma once
#include "Camera.h"
#include <string>

// ---------------------------------------------------------------------------
// GLBLoader
//   Reads a binary glTF (.glb) file and extracts the first camera it finds.
//   No external JSON library required – uses a lightweight string scanner
//   tuned to the structure Blender exports.
// ---------------------------------------------------------------------------
class GLBLoader {
public:
    // Returns a Camera with valid=true on success, valid=false on failure.
    static Camera loadCamera(const std::string& filepath);
};
