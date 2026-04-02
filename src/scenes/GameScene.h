#pragma once
#include "../Scene.h"
#include "../GameContext.h"
#include "../LightGun.h"
#include <vector>
#include <random>

// ---------------------------------------------------------------------------
// Target  –  a "duck" moving across the screen
//   Rendered as a low-poly coloured rectangle for now.
//   Swap in your 3-D model / sprite here later.
// ---------------------------------------------------------------------------
struct Target {
    float x, y;           // position (top-left of bounding box)
    float vx, vy;         // velocity (px/s)
    float w = 48.0f;
    float h = 32.0f;
    bool  alive   = true;
    bool  hit     = false; // true for a brief flash before removal
    float hitTime = 0.0f;  // countdown in seconds before removal after hit
    int   points  = 100;
    uint8_t r, g, b;       // colour (varied per round)
};

// ---------------------------------------------------------------------------
// GameScene
//   Core duck-hunt loop:
//     - Targets spawn and fly across the screen
//     - Player shoots with the LightGun
//     - Rounds increase speed and target count
//     - Lives lost when targets escape off-screen
// ---------------------------------------------------------------------------
class GameScene : public Scene {
public:
    explicit GameScene(GameContext& ctx);

    void handleEvent(const SDL_Event& event) override;
    void update(float dt) override;
    void render(SDL_Renderer* renderer) override;

private:
    // Gameplay helpers
    void spawnWave();
    void spawnTarget();
    bool testShot(float gx, float gy, Target& t) const;
    void advanceRound();

    // Render helpers
    void drawHUD(SDL_Renderer* r) const;
    void drawTarget(SDL_Renderer* r, const Target& t) const;
    void drawLives(SDL_Renderer* r) const;
    void drawFlash(SDL_Renderer* r) const;

    GameContext& m_ctx;
    LightGun     m_gun;

    // ---- Game state --------------------------------------------------
    int   m_score      = 0;
    int   m_lives      = 3;       // misses allowed before game over
    int   m_round      = 1;
    int   m_targetsRemaining = 0; // in this wave
    bool  m_waveActive = false;

    // Flash effect on kill
    float m_flashAlpha = 0.0f;    // 0-255

    // Round config (scales per round)
    float m_speedScale    = 1.0f;
    int   m_waveSize      = 3;    // targets per wave

    std::vector<Target> m_targets;

    // RNG
    std::mt19937                          m_rng;
    std::uniform_real_distribution<float> m_randY;
    std::uniform_real_distribution<float> m_randSpd;
    std::uniform_real_distribution<float> m_randVy;
    std::uniform_int_distribution<int>    m_randDir;

    // Inter-spawn timer
    float m_spawnTimer    = 0.0f;
    float m_spawnInterval = 1.2f;
    int   m_spawnedCount  = 0;

    // Brief end-of-round pause
    float m_roundPauseTimer = 0.0f;
    bool  m_inRoundPause    = false;
};
