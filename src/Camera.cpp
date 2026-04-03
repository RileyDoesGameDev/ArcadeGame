#include "Camera.h"
#include <cmath>

// ---------------------------------------------------------------------------
// Rotate vector (vx,vy,vz) by quaternion (qx,qy,qz,qw).
// Formula: v' = v + 2*qw*(q_xyz × v) + 2*(q_xyz × (q_xyz × v))
// ---------------------------------------------------------------------------
static void quatRotate(float vx, float vy, float vz,
                       float qx, float qy, float qz, float qw,
                       float& ox, float& oy, float& oz)
{
    // t = 2 * cross(q_xyz, v)
    float tx = 2.0f * (qy * vz - qz * vy);
    float ty = 2.0f * (qz * vx - qx * vz);
    float tz = 2.0f * (qx * vy - qy * vx);

    // v' = v + qw * t + cross(q_xyz, t)
    ox = vx + qw * tx + (qy * tz - qz * ty);
    oy = vy + qw * ty + (qz * tx - qx * tz);
    oz = vz + qw * tz + (qx * ty - qy * tx);
}

// Rotate by the conjugate (inverse for unit quaternions)
static void quatRotateInv(float vx, float vy, float vz,
                          float qx, float qy, float qz, float qw,
                          float& ox, float& oy, float& oz)
{
    quatRotate(vx, vy, vz, -qx, -qy, -qz, qw, ox, oy, oz);
}

// ---------------------------------------------------------------------------
bool Camera::projectPoint(float wx, float wy, float wz,
                          int screenW, int screenH,
                          float& outX, float& outY, float& outDepth) const
{
    // 1. Translate into camera-relative space
    float rx = wx - px;
    float ry = wy - py;
    float rz = wz - pz;

    // 2. Rotate into camera local space (undo camera orientation)
    float cx, cy, cz;
    quatRotateInv(rx, ry, rz, qx, qy, qz, qw, cx, cy, cz);

    // 3. GLTF cameras look down -Z; reject anything at or behind the camera
    if (cz >= -nearPlane) return false;

    outDepth = -cz; // positive depth in front of camera

    // 4. Perspective divide
    float aspect = static_cast<float>(screenW) / static_cast<float>(screenH);
    float f      = 1.0f / std::tan(fovY * 0.5f); // focal length factor

    float ndcX =  (cx / outDepth) * (f / aspect);
    float ndcY =  (cy / outDepth) *  f;

    // Only reject vertices that are wildly behind or off-screen.
    // SDL_RenderGeometry clips triangles to the viewport, so don't over-clip here.
    // Keep a very loose guard to avoid near-zero divide artifacts.
    if (ndcX < -20.0f || ndcX > 20.0f || ndcY < -20.0f || ndcY > 20.0f)
        return false;

    // 5. Convert NDC [-1,1] to screen pixels (Y is flipped: +Y up in NDC, +Y down on screen)
    outX = ( ndcX + 1.0f) * 0.5f * static_cast<float>(screenW);
    outY = (1.0f - ndcY) * 0.5f * static_cast<float>(screenH);

    return true;
}

// ---------------------------------------------------------------------------
float Camera::worldScreenSize(float worldRadius, float depth, int screenH) const
{
    float f = 1.0f / std::tan(fovY * 0.5f);
    return (worldRadius / depth) * f * static_cast<float>(screenH) * 0.5f;
}

// ---------------------------------------------------------------------------
void Camera::getForward(float& fx, float& fy, float& fz) const {
    // Camera local forward = (0, 0, -1); rotate to world space
    quatRotate(0.0f, 0.0f, -1.0f, qx, qy, qz, qw, fx, fy, fz);
}

void Camera::getRight(float& rx, float& ry, float& rz) const {
    quatRotate(1.0f, 0.0f, 0.0f, qx, qy, qz, qw, rx, ry, rz);
}

void Camera::getUp(float& ux, float& uy, float& uz) const {
    quatRotate(0.0f, 1.0f, 0.0f, qx, qy, qz, qw, ux, uy, uz);
}
