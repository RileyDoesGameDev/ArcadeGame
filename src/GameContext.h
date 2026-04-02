#pragma once
#include <string>

// Forward declare so scenes don't need the full Leaderboard header
class Leaderboard;

// ---------------------------------------------------------------------------
// GameContext
//   Shared state passed to every scene.  Scenes read/write this to
//   communicate across scene boundaries (e.g. GameScene writes finalScore,
//   EndGameScene reads it and submits the player's name to the leaderboard).
// ---------------------------------------------------------------------------
struct GameContext {
    // ---- Display --------------------------------------------------------
    int windowWidth  = 1280;
    int windowHeight = 720;

    // ---- Session data (written by GameScene, read by EndGameScene) ------
    int         finalScore  = 0;
    int         finalRound  = 0;   // which round the player reached
    std::string playerName  = "AAA";

    // ---- Shared systems -------------------------------------------------
    Leaderboard* leaderboard = nullptr;   // owned by Game, non-owning ptr here
};
