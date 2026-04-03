#pragma once
#include "../Scene.h"
#include "../GameContext.h"
#include "../LightGun.h"
#include "../Camera.h"
#include "../MeshLoader.h"
#include <SDL3/SDL.h>
#include <vector>
#include <random>

// ---------------------------------------------------------------------------
// Target3D  –  a duck with a position and velocity in 3-D world space.
//
//   The camera from booth.glb is at roughly (4.48, 2.14, 0) looking in
//   the -X direction.  Targets therefore fly across in the Z axis and bob
//   up/down in Y, at varying X depths in front of the camera.
//
//   worldRadius – half-width of the target in world units; controls how
//                 large it appears on screen at a given depth.
// ---------------------------------------------------------------------------
struct Target3D {
    float x, y, z;           // world-space centre
    float vz;                // left/right velocity (world units/sec)
    float vy;                // vertical bobbing velocity
    float worldRadius = 0.12f;

    bool  alive   = true;
    bool  hit     = false;
    float hitTimer = 0.0f;   // countdown before removal after hit
    int   points  = 100;

    // Colour
    uint8_t r = 200, g = 140, b = 40;
};

// ---------------------------------------------------------------------------
// GameScene
// ---------------------------------------------------------------------------
class GameScene : public Scene {
public:
    explicit GameScene(GameContext& ctx);

    void beginFrame() override;
    void handleEvent(const SDL_Event& event) override;
    void update(float dt) override;
    void render(SDL_Renderer* renderer) override;

private:
    void spawnWave();
    void spawnTarget();
    void advanceRound();
    bool testShot(float gx, float gy, const Target3D& t) const;

    void buildMeshRenderData();
    void drawBackground(SDL_Renderer* r) const;
    void drawMesh(SDL_Renderer* r) const;
    void drawTarget(SDL_Renderer* r, const Target3D& t) const;
    void drawHUD(SDL_Renderer* r) const;
    void drawLives(SDL_Renderer* r) const;
    void drawFlash(SDL_Renderer* r) const;

    // ---- Booth mesh (pre-projected at load time, camera is static) ------
    MeshData             m_mesh;
    std::vector<SDL_Vertex> m_meshVerts; // flat list: 3 verts per triangle, sorted back-to-front

    GameContext& m_ctx;
    LightGun     m_gun;
    Camera       m_cam;   // copy from ctx; falls back to default if ctx.camera invalid

    // ---- Game state --------------------------------------------------
    int   m_score  = 0;
    int   m_lives  = 3;
    int   m_round  = 1;
    int   m_waveSize         = 3;
    int   m_spawnedCount     = 0;
    int   m_targetsRemaining = 0;
    float m_speedScale       = 1.0f;
    float m_spawnTimer       = 0.0f;
    float m_spawnInterval    = 1.2f;
    float m_flashAlpha       = 0.0f;
    bool  m_inRoundPause     = false;
    float m_roundPauseTimer  = 0.0f;

    std::vector<Target3D> m_targets;

    // RNG
    std::mt19937                          m_rng;
    std::uniform_real_distribution<float> m_randY;
    std::uniform_real_distribution<float> m_randX;
    std::uniform_real_distribution<float> m_randSpd;
    std::uniform_real_distribution<float> m_randVy;
    std::uniform_int_distribution<int>    m_randDir;

    // ---- Spawn-area parameters (world units, derived from camera) ----
    // Targets fly across the Z axis in front of the camera.
    float m_spawnZMin =  3.0f;  // far left / far right edge
    float m_spawnZMax = -3.0f;
    float m_spawnXMin =  0.8f;  // closest to camera (big targets)
    float m_spawnXMax =  3.2f;  // furthest from camera (small targets)
    float m_spawnYMin =  0.4f;
    float m_spawnYMax =  4.0f;
    float m_baseSpeed =  1.2f;  // world units/sec at round 1
};
