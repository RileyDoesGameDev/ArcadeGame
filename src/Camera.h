#pragma once

// ---------------------------------------------------------------------------
// Camera
//   Holds the 3-D camera parameters loaded from the GLB file.
//   Uses software perspective projection so we don't need OpenGL –
//   SDL_Renderer keeps working for every other scene.
//
//   Coordinate system: GLTF / right-handed, Y-up, camera looks down -Z
//   in its own local space.  After rotating to world space the camera in
//   booth.glb looks roughly in the -X direction.
// ---------------------------------------------------------------------------
struct Camera {
    // World-space position (GLB node "translation")
    float px = 0.0f, py = 0.0f, pz = 0.0f;

    // Orientation as unit quaternion [x, y, z, w] (GLB node "rotation")
    float qx = 0.0f, qy = 0.0f, qz = 0.0f, qw = 1.0f;

    // Perspective parameters (GLB camera "perspective")
    float fovY      = 0.6911f; // radians  (~39.6 degrees)
    float nearPlane = 0.1f;
    float farPlane  = 1000.0f;

    // Set to true when successfully loaded from a GLB
    bool valid = false;

    // -----------------------------------------------------------------------
    // projectPoint
    //   Transform a 3-D world-space point into 2-D screen pixel coordinates.
    //   Returns false if the point is behind the camera (don't draw it).
    //
    //   outDepth – distance along the camera's forward axis, useful for
    //              computing how large a world-space object looks on screen.
    // -----------------------------------------------------------------------
    bool projectPoint(float wx, float wy, float wz,
                      int screenW, int screenH,
                      float& outX, float& outY, float& outDepth) const;

    // -----------------------------------------------------------------------
    // worldScreenSize
    //   Given a world-space radius and a depth (from projectPoint), returns
    //   the radius in screen pixels.  Use for scaling sprites / targets.
    // -----------------------------------------------------------------------
    float worldScreenSize(float worldRadius, float depth, int screenH) const;

    // World-space direction vectors derived from the quaternion
    void getForward(float& fx, float& fy, float& fz) const;
    void getRight  (float& rx, float& ry, float& rz) const;
    void getUp     (float& ux, float& uy, float& uz) const;
};
