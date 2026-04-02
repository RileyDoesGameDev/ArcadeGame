#pragma once
#include "../Scene.h"
#include "../GameContext.h"
#include "../LightGun.h"
#include <string>

// ---------------------------------------------------------------------------
// EndGameScene
//   Shows the player's final score.  If it's a high score they can shoot
//   letter tiles to enter their initials (3-letter arcade style).
//   Then they can go to the Leaderboard or back to the Title.
// ---------------------------------------------------------------------------
class EndGameScene : public Scene {
public:
    explicit EndGameScene(GameContext& ctx);

    void handleEvent(const SDL_Event& event) override;
    void update(float dt) override;
    void render(SDL_Renderer* renderer) override;

private:
    // ---- Name-entry state -----------------------------------------------
    static constexpr int NAME_LEN  = 3;
    static constexpr int ALPHA_LEN = 26;

    void confirmLetter();
    void renderNameEntry(SDL_Renderer* r) const;
    void renderButtons(SDL_Renderer* r) const;

    bool letterHitTest(int letter, float gx, float gy) const;
    // Returns the letter index (0-25) under the gun, or -1
    int  hoveredLetter() const;

    GameContext& m_ctx;
    LightGun     m_gun;

    bool m_isHighScore  = false;
    bool m_nameEntered  = false;
    int  m_cursorPos    = 0;   // which of the 3 initials we're filling

    char m_initials[NAME_LEN + 1] = { 'A','A','A','\0' };

    // Each letter slot cycles through A-Z; shooting locks in the letter
    // and moves to the next slot
    int m_letterChoice[NAME_LEN] = { 0, 0, 0 };  // index into A-Z

    float m_blinkTimer = 0.0f;

    // ---- Buttons after name entry / if not a high score ----------------
    enum class Button { Leaderboard, PlayAgain, Title };
    bool  m_showButtons = false;
};
