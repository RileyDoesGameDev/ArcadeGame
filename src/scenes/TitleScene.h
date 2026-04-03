#pragma once
#include "../Scene.h"
#include "../GameContext.h"
#include "../LightGun.h"

// ---------------------------------------------------------------------------
// TitleScene
//   Three menu options: START, LEADERBOARD, QUIT
//   The gun (or mouse) selects an option by aiming at it and firing.
// ---------------------------------------------------------------------------
class TitleScene : public Scene {
public:
    explicit TitleScene(GameContext& ctx);

    void beginFrame() override;
    void handleEvent(const SDL_Event& event) override;
    void update(float dt) override;
    void render(SDL_Renderer* renderer) override;

    enum class MenuItem { Start, Leaderboard, Quit, COUNT };

private:

    void drawPanel(SDL_Renderer* r, float x, float y, float w, float h,
                   SDL_Color fill, SDL_Color border) const;
    void drawMenuItem(SDL_Renderer* r, MenuItem item) const;
    bool gunOverItem(MenuItem item) const;

    GameContext& m_ctx;
    LightGun     m_gun;

    // Simple animated title "duck" flying across the screen
    float m_duckX    = 0.0f;
    float m_duckY    = 120.0f;
    float m_duckSpd  = 200.0f;
    float m_titlePulse = 0.0f; // drives colour oscillation
};
