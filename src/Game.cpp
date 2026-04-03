#include "Game.h"
#include "GLBLoader.h"
#include "scenes/TitleScene.h"
#include "scenes/GameScene.h"
#include "scenes/EndGameScene.h"
#include "scenes/LeaderboardScene.h"
#include <iostream>

// ---------------------------------------------------------------------------
Game::Game()
    : m_leaderboard("scores.csv") {}

Game::~Game() {
    quit();
}

// ---------------------------------------------------------------------------
bool Game::init(const char* title, int width, int height, bool fullscreen) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        std::cerr << "[Game] SDL_Init failed: " << SDL_GetError() << '\n';
        return false;
    }

    SDL_WindowFlags flags = 0;
    if (fullscreen) flags |= SDL_WINDOW_FULLSCREEN;

    m_window = SDL_CreateWindow(title, width, height, flags);
    if (!m_window) {
        std::cerr << "[Game] SDL_CreateWindow failed: " << SDL_GetError() << '\n';
        return false;
    }

    // nullptr = let SDL pick the best available renderer (on Pi: OpenGL ES)
    m_renderer = SDL_CreateRenderer(m_window, nullptr);
    if (!m_renderer) {
        std::cerr << "[Game] SDL_CreateRenderer failed: " << SDL_GetError() << '\n';
        return false;
    }

    // Enable alpha blending globally
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);

    // Hide the system cursor – the LightGun draws its own cross-hair
    SDL_HideCursor();

    // Build shared context
    m_ctx.windowWidth  = width;
    m_ctx.windowHeight = height;
    m_ctx.leaderboard  = &m_leaderboard;

    // Load saved scores
    m_leaderboard.load();

    // Load camera from the 3-D booth model
    m_ctx.camera = GLBLoader::loadCamera("assets/booth.glb");
    if (!m_ctx.camera.valid)
        std::cerr << "[Game] Warning: could not load camera from assets/booth.glb"
                     " – GameScene will use a default camera.\n";

    // Start on the Title screen
    m_scene   = createScene(SceneID::Title);
    m_running = true;
    m_lastTick = SDL_GetTicks();

    return true;
}

// ---------------------------------------------------------------------------
void Game::run() {
    while (m_running) {
        const uint64_t now  = SDL_GetTicks();
        const float    dt   = (now - m_lastTick) / 1000.0f;
        m_lastTick          = now;

        // Cap dt so a stalled frame doesn't cause wild physics jumps
        const float safeDt = (dt > 0.1f) ? 0.1f : dt;

        if (m_scene) m_scene->beginFrame(); // reset input state before polling
        processEvents();
        update(safeDt);
        render();

        // Crude frame limiter – keeps the Pi cool
        const uint64_t elapsed = SDL_GetTicks() - now;
        const uint64_t framems = static_cast<uint64_t>(TARGET_DT * 1000.0f);
        if (elapsed < framems) {
            SDL_Delay(static_cast<uint32_t>(framems - elapsed));
        }
    }
}

// ---------------------------------------------------------------------------
void Game::processEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) {
            m_running = false;
            return;
        }
        if (m_scene) m_scene->handleEvent(e);
    }
}

// ---------------------------------------------------------------------------
void Game::update(float dt) {
    if (!m_scene) return;

    m_scene->update(dt);

    if (m_scene->shouldTransition()) {
        switchScene(m_scene->nextScene());
    }
}

// ---------------------------------------------------------------------------
void Game::render() {
    SDL_SetRenderDrawColor(m_renderer, 10, 10, 20, 255);
    SDL_RenderClear(m_renderer);

    if (m_scene) m_scene->render(m_renderer);

    SDL_RenderPresent(m_renderer);
}

// ---------------------------------------------------------------------------
void Game::switchScene(SceneID id) {
    if (id == SceneID::None) {
        m_running = false;
        return;
    }
    m_scene = createScene(id);
}

// ---------------------------------------------------------------------------
std::unique_ptr<Scene> Game::createScene(SceneID id) {
    switch (id) {
        case SceneID::Title:       return std::make_unique<TitleScene>(m_ctx);
        case SceneID::Game:        return std::make_unique<GameScene>(m_ctx);
        case SceneID::EndGame:     return std::make_unique<EndGameScene>(m_ctx);
        case SceneID::Leaderboard: return std::make_unique<LeaderboardScene>(m_ctx);
        default:
            std::cerr << "[Game] Unknown SceneID – returning nullptr\n";
            return nullptr;
    }
}

// ---------------------------------------------------------------------------
void Game::quit() {
    m_scene.reset();
    if (m_renderer) { SDL_DestroyRenderer(m_renderer); m_renderer = nullptr; }
    if (m_window)   { SDL_DestroyWindow(m_window);     m_window   = nullptr; }
    SDL_Quit();
}
