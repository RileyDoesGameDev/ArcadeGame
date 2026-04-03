#pragma once
#include "../Scene.h"
#include "../GameContext.h"
#include "../LightGun.h"

// ---------------------------------------------------------------------------
// LeaderboardScene
//   Displays the top-10 scores.
//   Shoot the BACK button to return to the Title screen.
// ---------------------------------------------------------------------------
class LeaderboardScene : public Scene {
public:
    explicit LeaderboardScene(GameContext& ctx);

    void beginFrame() override;
    void handleEvent(const SDL_Event& event) override;
    void update(float dt) override;
    void render(SDL_Renderer* renderer) override;

private:
    bool backButtonHit() const;

    GameContext& m_ctx;
    LightGun     m_gun;
    float        m_rowAnim = 0.0f; // drives entry slide-in
};
