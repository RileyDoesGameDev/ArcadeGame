#pragma once
#include <SDL3/SDL.h>
#include <memory>
#include "GameContext.h"
#include "Leaderboard.h"
#include "Scene.h"

// ---------------------------------------------------------------------------
// Game
//   Owns the SDL window/renderer, drives the main loop, and swaps scenes.
//   All scenes share a GameContext so they can exchange data without
//   coupling to each other directly.
// ---------------------------------------------------------------------------
class Game {
public:
    Game();
    ~Game();

    // Non-copyable
    Game(const Game&)            = delete;
    Game& operator=(const Game&) = delete;

    bool init(const char* title, int width, int height, bool fullscreen = false);
    void run();
    void quit();

private:
    void processEvents();
    void update(float dt);
    void render();
    void switchScene(SceneID id);

    // Factory: construct the right Scene subclass for a given id
    std::unique_ptr<Scene> createScene(SceneID id);

    // SDL resources
    SDL_Window*   m_window   = nullptr;
    SDL_Renderer* m_renderer = nullptr;

    // Scene state
    std::unique_ptr<Scene> m_scene;

    // Shared state / systems
    Leaderboard m_leaderboard;
    GameContext m_ctx;

    bool     m_running  = false;
    uint64_t m_lastTick = 0;

    // Target frame time (default 60 fps)
    static constexpr float TARGET_DT = 1.0f / 60.0f;
};
