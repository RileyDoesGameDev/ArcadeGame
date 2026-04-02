#include "LeaderboardScene.h"
#include "../Leaderboard.h"
#include <cmath>
#include <string>
#include <algorithm>

// ---------------------------------------------------------------------------
static constexpr float ROW_H   = 44.0f;
static constexpr float ROW_GAP = 6.0f;
static constexpr float BACK_W  = 180.0f;
static constexpr float BACK_H  =  50.0f;

// ---------------------------------------------------------------------------
LeaderboardScene::LeaderboardScene(GameContext& ctx)
    : m_ctx(ctx) {}

// ---------------------------------------------------------------------------
void LeaderboardScene::handleEvent(const SDL_Event& event) {
    m_gun.handleEvent(event);
}

// ---------------------------------------------------------------------------
void LeaderboardScene::update(float dt) {
    m_gun.beginFrame();
    m_rowAnim += dt;  // used to animate slide-in

    if (m_gun.fired()) {
        if (backButtonHit()) transitionTo(SceneID::Title);
    }
}

// ---------------------------------------------------------------------------
void LeaderboardScene::render(SDL_Renderer* renderer) {
    const int W = m_ctx.windowWidth;
    const int H = m_ctx.windowHeight;

    // Background
    SDL_FRect bg = { 0, 0, (float)W, (float)H };
    SDL_SetRenderDrawColor(renderer, 8, 8, 22, 255);
    SDL_RenderFillRect(renderer, &bg);

    // Header bar
    {
        float hw = W * 0.55f, hh = 60.0f;
        SDL_FRect hdr = { (W - hw) * 0.5f, 20.0f, hw, hh };
        SDL_SetRenderDrawColor(renderer, 255, 215, 0, 200);
        SDL_RenderFillRect(renderer, &hdr);
        // TODO: SDL_ttf "HIGH SCORES"
    }

    // Column headers
    {
        float cx = W * 0.12f, cy = 95.0f, ch = 28.0f;
        SDL_FRect rankH = { cx, cy, 60.0f, ch };
        SDL_FRect nameH = { cx + 80.0f, cy, 160.0f, ch };
        SDL_FRect scoH  = { cx + 280.0f, cy, 140.0f, ch };
        SDL_SetRenderDrawColor(renderer, 100, 140, 255, 180);
        SDL_RenderFillRect(renderer, &rankH);
        SDL_RenderFillRect(renderer, &nameH);
        SDL_RenderFillRect(renderer, &scoH);
        // TODO: SDL_ttf "#", "NAME", "SCORE"
    }

    // Scores
    const auto& entries = m_ctx.leaderboard ? m_ctx.leaderboard->entries()
                                            : std::vector<LeaderboardEntry>{};

    float startY = 135.0f;
    float rowW   = W * 0.76f;
    float rowX   = (W - rowW) * 0.5f;

    for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
        const auto& e = entries[i];

        // Animate: rows slide in from the left sequentially
        float slideProgress = std::min(1.0f, m_rowAnim * 4.0f - i * 0.15f);
        float offsetX = (1.0f - slideProgress) * (-rowW);

        float ry = startY + i * (ROW_H + ROW_GAP);

        // Alternate row colours; gold for top 3
        SDL_Color rowCol = (i == 0) ? SDL_Color{180,140, 0,200} :
                           (i == 1) ? SDL_Color{140,140,140,200} :
                           (i == 2) ? SDL_Color{140, 80, 30,200} :
                           (i % 2 == 0) ? SDL_Color{30,30,60,190} :
                                          SDL_Color{20,20,50,190};

        SDL_FRect row = { rowX + offsetX, ry, rowW, ROW_H };
        SDL_SetRenderDrawColor(renderer, rowCol.r, rowCol.g, rowCol.b, rowCol.a);
        SDL_RenderFillRect(renderer, &row);

        // Rank pip
        SDL_FRect rankPip = { rowX + offsetX + 8, ry + 8, 40.0f, ROW_H - 16 };
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 120);
        SDL_RenderFillRect(renderer, &rankPip);
        // TODO: SDL_ttf  std::to_string(i+1)

        // Name band
        SDL_FRect nameBand = { rowX + offsetX + 60, ry + 8, 180.0f, ROW_H - 16 };
        SDL_SetRenderDrawColor(renderer, 200, 220, 255, 150);
        SDL_RenderFillRect(renderer, &nameBand);
        // TODO: SDL_ttf  e.name

        // Score band
        SDL_FRect scoreBand = { rowX + offsetX + 260, ry + 8, 180.0f, ROW_H - 16 };
        // Fill proportionally for visual flair (relative to rank 1)
        float maxScore = entries.empty() ? 1.0f : static_cast<float>(entries[0].score);
        float ratio    = (maxScore > 0.0f) ? static_cast<float>(e.score) / maxScore : 0.0f;
        SDL_FRect scoreFill = { rowX + offsetX + 260, ry + 8, 180.0f * ratio, ROW_H - 16 };
        SDL_SetRenderDrawColor(renderer, 255, 215, 0, 180);
        SDL_RenderFillRect(renderer, &scoreFill);
        SDL_SetRenderDrawColor(renderer, 255, 255, 200, 80);
        SDL_RenderFillRect(renderer, &scoreBand); // outline
        // TODO: SDL_ttf  std::to_string(e.score)
    }

    if (entries.empty()) {
        // No scores yet
        SDL_FRect empty = { (W - 300.0f) * 0.5f, 200.0f, 300.0f, 50.0f };
        SDL_SetRenderDrawColor(renderer, 80, 80, 80, 200);
        SDL_RenderFillRect(renderer, &empty);
        // TODO: SDL_ttf "NO SCORES YET"
    }

    // BACK button
    {
        float bx = (W - BACK_W) * 0.5f;
        float by = H - BACK_H - 20.0f;
        bool hov = backButtonHit();
        SDL_FRect btn = { bx, by, BACK_W, BACK_H };
        SDL_SetRenderDrawColor(renderer, hov ? 255 : 180, hov ? 80 : 50, hov ? 50 : 50, 230);
        SDL_RenderFillRect(renderer, &btn);
        // TODO: SDL_ttf "< BACK"
    }

    m_gun.renderCrosshair(renderer);
}

// ---------------------------------------------------------------------------
bool LeaderboardScene::backButtonHit() const {
    float bx = (m_ctx.windowWidth - BACK_W) * 0.5f;
    float by = m_ctx.windowHeight - BACK_H - 20.0f;
    return m_gun.x() >= bx && m_gun.x() <= bx + BACK_W &&
           m_gun.y() >= by && m_gun.y() <= by + BACK_H;
}
