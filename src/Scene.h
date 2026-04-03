#pragma once
#include <SDL3/SDL.h>

// ---------------------------------------------------------------------------
// SceneID
//   One entry per screen in the game.  Add more here as the project grows.
// ---------------------------------------------------------------------------
enum class SceneID {
    None,
    Title,
    Game,
    EndGame,
    Leaderboard
};

// ---------------------------------------------------------------------------
// Scene  (abstract base)
//   Every screen inherits from this.  The Game manager calls these three
//   methods each frame and checks shouldTransition() to know when to swap.
// ---------------------------------------------------------------------------
class Scene {
public:
    virtual ~Scene() = default;

    // Called once per frame BEFORE events are polled (resets per-frame input state)
    virtual void beginFrame() {}

    // Called once per SDL event (called inside the event poll loop)
    virtual void handleEvent(const SDL_Event& event) = 0;

    // Called once per frame with delta-time in seconds
    virtual void update(float dt) = 0;

    // Called once per frame after update – renderer is already cleared
    virtual void render(SDL_Renderer* renderer) = 0;

    // Game manager queries these after each frame
    bool     shouldTransition() const { return m_transition; }
    SceneID  nextScene()        const { return m_nextScene;  }

protected:
    // Derived scenes call this to request a scene change
    void transitionTo(SceneID id) {
        m_nextScene  = id;
        m_transition = true;
    }

private:
    bool    m_transition = false;
    SceneID m_nextScene  = SceneID::None;
};
