#include "EndGameScene.h"
#include "../Leaderboard.h"
#include <cmath>
#include <cstring>

// ---------------------------------------------------------------------------
// Layout constants
// ---------------------------------------------------------------------------
static constexpr float LETTER_W     = 60.0f;
static constexpr float LETTER_H     = 70.0f;
static constexpr float LETTER_GAP   = 14.0f;
// 26 letters per row, split into two rows of 13
static constexpr int   LETTERS_PER_ROW = 13;

// ---------------------------------------------------------------------------
EndGameScene::EndGameScene(GameContext& ctx)
    : m_ctx(ctx)
{
    m_isHighScore = ctx.leaderboard && ctx.leaderboard->isHighScore(ctx.finalScore);
    m_showButtons = !m_isHighScore;
}

// ---------------------------------------------------------------------------
void EndGameScene::handleEvent(const SDL_Event& event) {
    m_gun.handleEvent(event);
}

// ---------------------------------------------------------------------------
void EndGameScene::update(float dt) {
    m_gun.beginFrame();
    m_blinkTimer += dt;

    if (m_gun.fired()) {
        if (!m_showButtons && m_isHighScore && !m_nameEntered) {
            // ---- Name-entry: shoot a letter tile -----------------------
            int ltr = hoveredLetter();
            if (ltr >= 0) {
                m_letterChoice[m_cursorPos] = ltr;
                m_initials[m_cursorPos] = static_cast<char>('A' + ltr);
                confirmLetter();
            }
        } else {
            // ---- Button selection ---------------------------------------
            const float W = m_ctx.windowWidth;
            const float H = m_ctx.windowHeight;

            // Three buttons laid out horizontally near the bottom
            const float bw = 200.0f, bh = 50.0f, gap = 30.0f;
            const float totalW = 3 * bw + 2 * gap;
            float bx = (W - totalW) * 0.5f;
            float by = H * 0.82f;

            auto inBtn = [&](float ox) {
                return m_gun.x() >= ox && m_gun.x() <= ox + bw &&
                       m_gun.y() >= by && m_gun.y() <= by + bh;
            };

            if (inBtn(bx))                      transitionTo(SceneID::Leaderboard);
            else if (inBtn(bx + bw + gap))      transitionTo(SceneID::Game);
            else if (inBtn(bx + 2*(bw + gap)))  transitionTo(SceneID::Title);
        }
    }
}

// ---------------------------------------------------------------------------
void EndGameScene::render(SDL_Renderer* renderer) {
    const int W = m_ctx.windowWidth;
    const int H = m_ctx.windowHeight;

    // Background
    SDL_FRect bg = { 0, 0, (float)W, (float)H };
    SDL_SetRenderDrawColor(renderer, 10, 10, 30, 255);
    SDL_RenderFillRect(renderer, &bg);

    // Score banner
    {
        float bw = W * 0.5f, bh = 70.0f;
        SDL_FRect banner = { (W - bw) * 0.5f, 30.0f, bw, bh };
        SDL_SetRenderDrawColor(renderer, 255, 215, 0, 220);
        SDL_RenderFillRect(renderer, &banner);
        // TODO: SDL_ttf "FINAL SCORE: " + m_ctx.finalScore
    }

    // Round reached
    {
        float bw = W * 0.3f, bh = 40.0f;
        SDL_FRect rb = { (W - bw) * 0.5f, 110.0f, bw, bh };
        SDL_SetRenderDrawColor(renderer, 80, 160, 255, 180);
        SDL_RenderFillRect(renderer, &rb);
        // TODO: SDL_ttf "ROUND: " + m_ctx.finalRound
    }

    if (m_isHighScore && !m_nameEntered) {
        // High-score star flash
        float pulse = std::abs(std::sin(m_blinkTimer * 3.0f));
        uint8_t sc = static_cast<uint8_t>(100 + 155 * pulse);
        SDL_FRect star = { (W - 300.0f) * 0.5f, 160.0f, 300.0f, 30.0f };
        SDL_SetRenderDrawColor(renderer, sc, sc, 0, 255);
        SDL_RenderFillRect(renderer, &star);
        // TODO: SDL_ttf "NEW HIGH SCORE! ENTER YOUR NAME"

        renderNameEntry(renderer);
    } else {
        renderButtons(renderer);
    }

    m_gun.renderCrosshair(renderer);
}

// ---------------------------------------------------------------------------
void EndGameScene::confirmLetter() {
    ++m_cursorPos;
    if (m_cursorPos >= NAME_LEN) {
        // All initials entered – submit to leaderboard
        if (m_ctx.leaderboard) {
            m_ctx.leaderboard->addEntry(std::string(m_initials, NAME_LEN),
                                        m_ctx.finalScore);
        }
        m_ctx.playerName = std::string(m_initials, NAME_LEN);
        m_nameEntered = true;
        m_showButtons = true;
    }
}

// ---------------------------------------------------------------------------
void EndGameScene::renderNameEntry(SDL_Renderer* r) const {
    const int W = m_ctx.windowWidth;

    // --- Show chosen initials so far ------------------------------------
    float iw = 3 * 70.0f + 2 * 10.0f;
    float ix = (W - iw) * 0.5f;
    float iy = 200.0f;

    for (int i = 0; i < NAME_LEN; ++i) {
        float cx = ix + i * 80.0f;
        SDL_FRect box = { cx, iy, 70.0f, 70.0f };

        bool isCurrent = (i == m_cursorPos);
        float blink    = std::abs(std::sin(m_blinkTimer * 4.0f));

        if (isCurrent) {
            uint8_t bc = static_cast<uint8_t>(200 + 55 * blink);
            SDL_SetRenderDrawColor(r, bc, bc, 0, 255);
        } else {
            SDL_SetRenderDrawColor(r, 40, 60, 120, 220);
        }
        SDL_RenderFillRect(r, &box);

        // Letter colour tile
        SDL_FRect letter = { cx + 10, iy + 10, 50.0f, 50.0f };
        SDL_SetRenderDrawColor(r, 255, 255, 255, 200);
        SDL_RenderFillRect(r, &letter);
        // TODO: SDL_ttf render m_initials[i]
    }

    // --- Alphabet grid --------------------------------------------------
    // Two rows of 13 letters
    float gw  = LETTERS_PER_ROW * (LETTER_W + LETTER_GAP) - LETTER_GAP;
    float gx0 = (W - gw) * 0.5f;
    float gy0 = 300.0f;

    int hovered = hoveredLetter();

    for (int i = 0; i < ALPHA_LEN; ++i) {
        int row = i / LETTERS_PER_ROW;
        int col = i % LETTERS_PER_ROW;
        float lx = gx0 + col * (LETTER_W + LETTER_GAP);
        float ly = gy0 + row * (LETTER_H + LETTER_GAP);

        bool isHov = (i == hovered);
        SDL_FRect tile = { lx, ly, LETTER_W, LETTER_H };
        SDL_SetRenderDrawColor(r,
            isHov ? 255 : 60,
            isHov ? 220 : 60,
            isHov ? 30  : 140,
            220);
        SDL_RenderFillRect(r, &tile);

        // Inner label (colour only – replace with SDL_ttf glyph)
        SDL_FRect inner = { lx + 6, ly + 6, LETTER_W - 12, LETTER_H - 12 };
        SDL_SetRenderDrawColor(r, 255, 255, 255, isHov ? 255 : 160);
        SDL_RenderFillRect(r, &inner);
        // TODO: SDL_ttf  char c = 'A' + i;  render glyph c at (lx, ly)
    }
}

// ---------------------------------------------------------------------------
void EndGameScene::renderButtons(SDL_Renderer* r) const {
    const float W = m_ctx.windowWidth;
    const float H = m_ctx.windowHeight;

    const float bw = 200.0f, bh = 50.0f, gap = 30.0f;
    const float totalW = 3 * bw + 2 * gap;
    float bx = (W - totalW) * 0.5f;
    float by = H * 0.82f;

    auto drawBtn = [&](float ox, SDL_Color c) {
        bool hov = m_gun.x() >= ox && m_gun.x() <= ox + bw &&
                   m_gun.y() >= by && m_gun.y() <= by + bh;
        SDL_FRect rect = { ox, by, bw, bh };
        SDL_SetRenderDrawColor(r,
            hov ? 255 : c.r, hov ? 230 : c.g, hov ? 50 : c.b, 230);
        SDL_RenderFillRect(r, &rect);
        // TODO: SDL_ttf label
    };

    drawBtn(bx,               { 50,100,220, 255 }); // Leaderboard (blue)
    drawBtn(bx + bw + gap,    { 50,180, 50, 255 }); // Play Again  (green)
    drawBtn(bx + 2*(bw+gap),  {180, 50, 50, 255 }); // Title       (red)
}

// ---------------------------------------------------------------------------
int EndGameScene::hoveredLetter() const {
    if (m_cursorPos >= NAME_LEN) return -1;
    const int W = m_ctx.windowWidth;
    float gw  = LETTERS_PER_ROW * (LETTER_W + LETTER_GAP) - LETTER_GAP;
    float gx0 = (W - gw) * 0.5f;
    float gy0 = 300.0f;

    for (int i = 0; i < ALPHA_LEN; ++i) {
        int row = i / LETTERS_PER_ROW;
        int col = i % LETTERS_PER_ROW;
        float lx = gx0 + col * (LETTER_W + LETTER_GAP);
        float ly = gy0 + row * (LETTER_H + LETTER_GAP);
        if (m_gun.x() >= lx && m_gun.x() <= lx + LETTER_W &&
            m_gun.y() >= ly && m_gun.y() <= ly + LETTER_H)
            return i;
    }
    return -1;
}
