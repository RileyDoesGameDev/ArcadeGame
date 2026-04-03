#include "TitleScene.h"
#include <cmath>

// ---------------------------------------------------------------------------
// Menu item layout (centres on screen)
// ---------------------------------------------------------------------------
static constexpr float ITEM_W = 280.0f;
static constexpr float ITEM_H =  60.0f;
static constexpr float ITEM_GAP = 20.0f;

static float itemX(const GameContext& ctx) {
    return (ctx.windowWidth - ITEM_W) * 0.5f;
}
static float itemY(const GameContext& ctx, TitleScene::MenuItem item) {
    const int idx = static_cast<int>(item);
    const float totalH = static_cast<int>(TitleScene::MenuItem::COUNT) * (ITEM_H + ITEM_GAP);
    const float startY = ctx.windowHeight * 0.55f - totalH * 0.5f;
    return startY + idx * (ITEM_H + ITEM_GAP);
}

// ---------------------------------------------------------------------------
TitleScene::TitleScene(GameContext& ctx)
    : m_ctx(ctx)
    , m_duckX(-80.0f)
{}

// ---------------------------------------------------------------------------
void TitleScene::beginFrame() {
    m_gun.beginFrame();
}

void TitleScene::handleEvent(const SDL_Event& event) {
    m_gun.handleEvent(event);
}

// ---------------------------------------------------------------------------
void TitleScene::update(float dt) {

    // Animate title duck
    m_duckX += m_duckSpd * dt;
    if (m_duckX > m_ctx.windowWidth + 80.0f) m_duckX = -80.0f;

    // Title colour pulse
    m_titlePulse += dt * 2.0f;

    if (m_gun.fired()) {
        if (gunOverItem(MenuItem::Start))
            transitionTo(SceneID::Game);
        else if (gunOverItem(MenuItem::Leaderboard))
            transitionTo(SceneID::Leaderboard);
        else if (gunOverItem(MenuItem::Quit))
            transitionTo(SceneID::None);
    }
}

// ---------------------------------------------------------------------------
void TitleScene::render(SDL_Renderer* renderer) {
    const int W = m_ctx.windowWidth;
    const int H = m_ctx.windowHeight;

    // ---- Sky gradient (two-band approximation) --------------------------
    SDL_FRect sky = { 0, 0, (float)W, H * 0.7f };
    SDL_SetRenderDrawColor(renderer, 30, 80, 160, 255);
    SDL_RenderFillRect(renderer, &sky);

    SDL_FRect ground = { 0, H * 0.7f, (float)W, H * 0.3f };
    SDL_SetRenderDrawColor(renderer, 34, 100, 34, 255);
    SDL_RenderFillRect(renderer, &ground);

    // ---- Animated demo duck (low-poly: just an orange triangle outline) --
    {
        float bx = m_duckX, by = m_duckY;
        // Body
        SDL_FRect body = { bx, by, 60.0f, 30.0f };
        SDL_SetRenderDrawColor(renderer, 180, 120, 40, 255);
        SDL_RenderFillRect(renderer, &body);
        // Head
        SDL_FRect head = { bx + 44.0f, by - 14.0f, 22.0f, 22.0f };
        SDL_SetRenderDrawColor(renderer, 60, 140, 60, 255);
        SDL_RenderFillRect(renderer, &head);
        // Wing flash
        SDL_FRect wing = { bx + 10.0f, by - 10.0f, 30.0f, 14.0f };
        SDL_SetRenderDrawColor(renderer, 220, 200, 180, 220);
        SDL_RenderFillRect(renderer, &wing);
    }

    // ---- Title banner ---------------------------------------------------
    {
        uint8_t r = static_cast<uint8_t>(180 + 75 * std::sin(m_titlePulse));
        uint8_t g = static_cast<uint8_t>(220 + 35 * std::sin(m_titlePulse * 0.7f));
        // Draw a big coloured banner box as a placeholder for actual text
        float bw = W * 0.6f, bh = 80.0f;
        float bx = (W - bw) * 0.5f, bY = H * 0.18f;
        SDL_FRect banner = { bx, bY, bw, bh };
        SDL_SetRenderDrawColor(renderer, 20, 20, 20, 180);
        SDL_RenderFillRect(renderer, &banner);
        // Inner highlight
        SDL_FRect inner = { bx + 4, bY + 4, bw - 8, bh - 8 };
        SDL_SetRenderDrawColor(renderer, r, g, 60, 255);
        SDL_RenderFillRect(renderer, &inner);
        // TODO: Replace with SDL_ttf text: "RETRO HUNT"
    }

    // ---- Menu items -----------------------------------------------------
    for (int i = 0; i < static_cast<int>(MenuItem::COUNT); ++i) {
        drawMenuItem(renderer, static_cast<MenuItem>(i));
    }

    // ---- Crosshair ------------------------------------------------------
    m_gun.renderCrosshair(renderer);
}

// ---------------------------------------------------------------------------
void TitleScene::drawMenuItem(SDL_Renderer* r, MenuItem item) const {
    float x = itemX(m_ctx);
    float y = itemY(m_ctx, item);

    bool hovered = gunOverItem(item);

    SDL_Color fill   = hovered ? SDL_Color{255,200,0,240} : SDL_Color{40,40,60,220};
    SDL_Color border = hovered ? SDL_Color{255,255,255,255} : SDL_Color{120,120,180,255};

    drawPanel(r, x, y, ITEM_W, ITEM_H, fill, border);

    // Colour-coded inner label band as text stand-in
    // (Replace with SDL_ttf RenderText once fonts are set up)
    SDL_Color labelCol = {};
    switch (item) {
        case MenuItem::Start:       labelCol = {50, 200, 50, 255};  break;
        case MenuItem::Leaderboard: labelCol = {50, 150, 255, 255}; break;
        case MenuItem::Quit:        labelCol = {220, 60, 60, 255};  break;
        default: break;
    }
    SDL_FRect label = { x + 20.0f, y + 14.0f, ITEM_W - 40.0f, ITEM_H - 28.0f };
    SDL_SetRenderDrawColor(r, labelCol.r, labelCol.g, labelCol.b, labelCol.a);
    SDL_RenderFillRect(r, &label);
    // TODO: draw text "START" / "LEADERBOARD" / "QUIT" via SDL_ttf
}

// ---------------------------------------------------------------------------
void TitleScene::drawPanel(SDL_Renderer* r, float x, float y, float w, float h,
                            SDL_Color fill, SDL_Color border) const {
    SDL_FRect rect = { x, y, w, h };
    SDL_SetRenderDrawColor(r, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(r, &rect);

    // 2-pixel border
    SDL_FRect top    = { x,         y,         w,    2.0f };
    SDL_FRect bot    = { x,         y + h - 2, w,    2.0f };
    SDL_FRect left   = { x,         y,         2.0f, h    };
    SDL_FRect right_ = { x + w - 2, y,         2.0f, h    };
    SDL_SetRenderDrawColor(r, border.r, border.g, border.b, border.a);
    SDL_RenderFillRect(r, &top);
    SDL_RenderFillRect(r, &bot);
    SDL_RenderFillRect(r, &left);
    SDL_RenderFillRect(r, &right_);
}

// ---------------------------------------------------------------------------
bool TitleScene::gunOverItem(MenuItem item) const {
    float x = itemX(m_ctx);
    float y = itemY(m_ctx, item);
    return m_gun.x() >= x && m_gun.x() <= x + ITEM_W &&
           m_gun.y() >= y && m_gun.y() <= y + ITEM_H;
}
