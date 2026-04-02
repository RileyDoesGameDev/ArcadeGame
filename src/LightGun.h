#pragma once
#include <SDL3/SDL.h>

// ---------------------------------------------------------------------------
// GunState  –  snapshot of the gun's position and fire status for one frame
// ---------------------------------------------------------------------------
struct GunState {
    float x      = 0.0f;  // aim position in window pixels
    float y      = 0.0f;
    bool  fired  = false;  // true only on the frame the trigger was pulled
};

// ---------------------------------------------------------------------------
// LightGun
//
//   Abstracts the input device so the rest of the game doesn't care whether
//   you're using a mouse (dev / desktop testing) or a real light gun / IR pen.
//
//   Current implementation: maps LEFT mouse button → trigger pull.
//   To wire up a real gun later, replace the internals of update() and
//   beginFrame() – the interface stays the same.
//
//   Cursor cross-hair:
//     Call renderCrosshair() from your scene's render() pass to draw a simple
//     targeting reticle at the current aim position.
// ---------------------------------------------------------------------------
class LightGun {
public:
    // Call once at the start of every frame (before processing events)
    void beginFrame();

    // Feed each SDL_Event from the poll loop here
    void handleEvent(const SDL_Event& event);

    // Read-only access to this frame's state
    const GunState& state() const { return m_state; }

    // Convenience helpers
    float x()     const { return m_state.x;     }
    float y()     const { return m_state.y;     }
    bool  fired() const { return m_state.fired; }

    // Draw a simple cross-hair at the aim point
    void renderCrosshair(SDL_Renderer* renderer) const;

private:
    GunState m_state;
};
