#include "GameScene.h"
#include <cmath>
#include <algorithm>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static constexpr int   MAX_ROUNDS     = 10;
static constexpr float HIT_FLASH_DUR  = 0.25f;  // seconds target lingers after hit
static constexpr float ROUND_PAUSE    = 1.5f;    // seconds between rounds
static constexpr float BASE_SPEED     = 160.0f;  // px/s at round 1

// ---------------------------------------------------------------------------
GameScene::GameScene(GameContext& ctx)
    : m_ctx(ctx)
    , m_rng(std::random_device{}())
{
    // Distribution ranges will be set in spawnWave() once we know the window size
    m_randY   = std::uniform_real_distribution<float>(50.0f,
                     static_cast<float>(ctx.windowHeight) * 0.65f);
    m_randSpd = std::uniform_real_distribution<float>(BASE_SPEED * 0.8f,
                     BASE_SPEED * 1.2f);
    m_randVy  = std::uniform_real_distribution<float>(-40.0f, 40.0f);
    m_randDir = std::uniform_int_distribution<int>(0, 1);

    spawnWave();
}

// ---------------------------------------------------------------------------
void GameScene::handleEvent(const SDL_Event& event) {
    m_gun.handleEvent(event);
}

// ---------------------------------------------------------------------------
void GameScene::update(float dt) {
    m_gun.beginFrame();

    // ---- Round pause between waves -------------------------------------
    if (m_inRoundPause) {
        m_roundPauseTimer -= dt;
        if (m_roundPauseTimer <= 0.0f) {
            m_inRoundPause = false;
            advanceRound();
            spawnWave();
        }
        return;
    }

    // ---- Spawn staggered targets ---------------------------------------
    if (m_spawnedCount < m_waveSize) {
        m_spawnTimer -= dt;
        if (m_spawnTimer <= 0.0f) {
            spawnTarget();
            m_spawnTimer = m_spawnInterval;
        }
    }

    // ---- Process shot --------------------------------------------------
    if (m_gun.fired()) {
        m_flashAlpha = 40.0f; // brief screen flash
        for (auto& t : m_targets) {
            if (!t.alive || t.hit) continue;
            if (testShot(m_gun.x(), m_gun.y(), t)) {
                t.hit     = true;
                t.hitTime = HIT_FLASH_DUR;
                m_score  += t.points;
                break; // one bullet, one target
            }
        }
    }

    // ---- Move targets + check escapes ----------------------------------
    int aliveCount = 0;
    for (auto& t : m_targets) {
        if (!t.alive) continue;

        if (t.hit) {
            // Frozen after hit – just countdown removal
            t.hitTime -= dt;
            if (t.hitTime <= 0.0f) {
                t.alive = false;
                --m_targetsRemaining;
            }
            continue;
        }

        // Move
        t.x += t.vx * dt;
        t.y += t.vy * dt;

        // Bounce vertically off sky / ground boundaries
        if (t.y < 20.0f || t.y + t.h > m_ctx.windowHeight * 0.75f) {
            t.vy = -t.vy;
        }

        // Escaped off the side?
        bool escaped = (t.vx > 0.0f && t.x > m_ctx.windowWidth + 20.0f)
                    || (t.vx < 0.0f && t.x + t.w < -20.0f);
        if (escaped) {
            t.alive = false;
            --m_targetsRemaining;
            --m_lives;
        } else {
            ++aliveCount;
        }
    }

    // Fade flash
    if (m_flashAlpha > 0.0f) m_flashAlpha -= 200.0f * dt;

    // ---- Check game over -----------------------------------------------
    if (m_lives <= 0) {
        m_ctx.finalScore = m_score;
        m_ctx.finalRound = m_round;
        transitionTo(SceneID::EndGame);
        return;
    }

    // ---- Wave complete? ------------------------------------------------
    if (m_targetsRemaining <= 0 && m_spawnedCount >= m_waveSize) {
        if (m_round >= MAX_ROUNDS) {
            m_ctx.finalScore = m_score;
            m_ctx.finalRound = m_round;
            transitionTo(SceneID::EndGame);
        } else {
            m_inRoundPause    = true;
            m_roundPauseTimer = ROUND_PAUSE;
        }
    }
}

// ---------------------------------------------------------------------------
void GameScene::render(SDL_Renderer* renderer) {
    const int W = m_ctx.windowWidth;
    const int H = m_ctx.windowHeight;

    // Sky
    SDL_FRect sky = { 0, 0, (float)W, H * 0.75f };
    SDL_SetRenderDrawColor(renderer, 35, 100, 190, 255);
    SDL_RenderFillRect(renderer, &sky);

    // Ground
    SDL_FRect ground = { 0, H * 0.75f, (float)W, H * 0.25f };
    SDL_SetRenderDrawColor(renderer, 34, 100, 34, 255);
    SDL_RenderFillRect(renderer, &ground);

    // Targets
    for (const auto& t : m_targets) {
        if (t.alive) drawTarget(renderer, t);
    }

    // HUD (score, round, lives)
    drawHUD(renderer);
    drawLives(renderer);

    // Screen-flash on shot
    drawFlash(renderer);

    // Round pause overlay
    if (m_inRoundPause) {
        SDL_FRect overlay = { 0, 0, (float)W, (float)H };
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 100);
        SDL_RenderFillRect(renderer, &overlay);
        // TODO: draw "ROUND CLEAR!" text via SDL_ttf
    }

    // Gun crosshair on top
    m_gun.renderCrosshair(renderer);
}

// ---------------------------------------------------------------------------
void GameScene::spawnWave() {
    m_targets.clear();
    m_spawnedCount = 0;
    m_spawnTimer   = 0.0f; // spawn first target immediately
    // targetsRemaining tracked as targets spawn/die
    m_targetsRemaining = m_waveSize;

    // Reserves space to avoid reallocs mid-wave
    m_targets.reserve(m_waveSize);
}

// ---------------------------------------------------------------------------
void GameScene::spawnTarget() {
    Target t;

    // Direction: 0 = left-to-right, 1 = right-to-left
    bool leftToRight = (m_randDir(m_rng) == 0);
    float spd = m_randSpd(m_rng) * m_speedScale;

    t.x  = leftToRight ? -t.w : static_cast<float>(m_ctx.windowWidth);
    t.y  = m_randY(m_rng);
    t.vx = leftToRight ? spd : -spd;
    t.vy = m_randVy(m_rng);

    // Colour varies per round to give visual feedback of difficulty
    static const uint8_t COLS[][3] = {
        {200,180, 80}, {80,180,200}, {200, 80, 80},
        {80,200, 80},  {200, 80,200},{255,160, 20},
        {20,200,160},  {255,255, 80},{180, 80,255},
        {255,100,100}
    };
    int ci = std::min(m_round - 1, 9);
    t.r = COLS[ci][0]; t.g = COLS[ci][1]; t.b = COLS[ci][2];

    // Score bonus for higher rounds
    t.points = 100 * m_round;

    m_targets.push_back(t);
    ++m_spawnedCount;
}

// ---------------------------------------------------------------------------
bool GameScene::testShot(float gx, float gy, Target& t) const {
    return gx >= t.x && gx <= t.x + t.w &&
           gy >= t.y && gy <= t.y + t.h;
}

// ---------------------------------------------------------------------------
void GameScene::advanceRound() {
    ++m_round;
    m_speedScale    = 1.0f + (m_round - 1) * 0.15f;   // +15% speed per round
    m_waveSize      = 3 + (m_round - 1);                // +1 target per round
    m_spawnInterval = std::max(0.4f, 1.2f - (m_round - 1) * 0.08f);
}

// ---------------------------------------------------------------------------
void GameScene::drawTarget(SDL_Renderer* r, const Target& t) const {
    uint8_t alpha = t.hit ? 120 : 255;

    // Body
    SDL_FRect body = { t.x, t.y, t.w, t.h };
    SDL_SetRenderDrawColor(r, t.r, t.g, t.b, alpha);
    SDL_RenderFillRect(r, &body);

    // Wing flash when hit
    if (t.hit) {
        SDL_FRect flash = { t.x + 4, t.y + 4, t.w - 8, t.h - 8 };
        SDL_SetRenderDrawColor(r, 255, 255, 255, 180);
        SDL_RenderFillRect(r, &flash);
    }

    // Outline
    SDL_SetRenderDrawColor(r, 0, 0, 0, 200);
    SDL_FRect outlines[4] = {
        { t.x,           t.y,           t.w,    2.0f },
        { t.x,           t.y + t.h - 2, t.w,    2.0f },
        { t.x,           t.y,           2.0f,   t.h  },
        { t.x + t.w - 2, t.y,           2.0f,   t.h  }
    };
    for (auto& o : outlines) SDL_RenderFillRect(r, &o);
}

// ---------------------------------------------------------------------------
void GameScene::drawHUD(SDL_Renderer* r) const {
    // Score and round bars at top
    //  - Replace the coloured rects below with SDL_ttf text
    float barW = 200.0f, barH = 36.0f, pad = 10.0f;

    // Score panel
    SDL_FRect sPanel = { pad, pad, barW, barH };
    SDL_SetRenderDrawColor(r, 0, 0, 0, 160);
    SDL_RenderFillRect(r, &sPanel);
    // Score fill (visual indicator of score magnitude, caps at window width)
    float maxScore = 1000.0f * m_round;
    float fill = std::min(1.0f, m_score / maxScore);
    SDL_FRect sFill = { pad + 2, pad + 2, (barW - 4) * fill, barH - 4 };
    SDL_SetRenderDrawColor(r, 255, 215, 0, 220);
    SDL_RenderFillRect(r, &sFill);
    // TODO: SDL_ttf "SCORE: X"

    // Round panel
    SDL_FRect rPanel = { pad * 2 + barW, pad, 100.0f, barH };
    SDL_SetRenderDrawColor(r, 0, 0, 0, 160);
    SDL_RenderFillRect(r, &rPanel);
    // Inner colour per round
    SDL_FRect rFill = { pad * 2 + barW + 2, pad + 2, 96.0f, barH - 4 };
    SDL_SetRenderDrawColor(r, 80, 160, 255, 220);
    SDL_RenderFillRect(r, &rFill);
    // TODO: SDL_ttf "RND X"
}

// ---------------------------------------------------------------------------
void GameScene::drawLives(SDL_Renderer* r) const {
    // Draw life pips in the top-right
    constexpr float PIP = 20.0f;
    constexpr float GAP = 8.0f;
    float startX = m_ctx.windowWidth - (PIP + GAP) * 3 - 10.0f;

    for (int i = 0; i < 3; ++i) {
        SDL_FRect pip = { startX + i * (PIP + GAP), 10.0f, PIP, PIP };
        if (i < m_lives)
            SDL_SetRenderDrawColor(r, 255, 60, 60, 255);
        else
            SDL_SetRenderDrawColor(r, 80, 40, 40, 200);
        SDL_RenderFillRect(r, &pip);
    }
}

// ---------------------------------------------------------------------------
void GameScene::drawFlash(SDL_Renderer* r) const {
    if (m_flashAlpha <= 0.0f) return;
    SDL_FRect full = { 0, 0, (float)m_ctx.windowWidth, (float)m_ctx.windowHeight };
    SDL_SetRenderDrawColor(r, 255, 255, 200,
                           static_cast<uint8_t>(std::min(255.0f, m_flashAlpha)));
    SDL_RenderFillRect(r, &full);
}
