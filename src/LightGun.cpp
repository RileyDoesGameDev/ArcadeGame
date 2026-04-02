#include "LightGun.h"

// ---------------------------------------------------------------------------
// beginFrame  –  reset per-frame state
// ---------------------------------------------------------------------------
void LightGun::beginFrame() {
    m_state.fired = false;

    // SDL3: SDL_GetMouseState returns float coordinates
    SDL_GetMouseState(&m_state.x, &m_state.y);
}

// ---------------------------------------------------------------------------
// handleEvent  –  fire on left-mouse-button down
// ---------------------------------------------------------------------------
void LightGun::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        if (event.button.button == SDL_BUTTON_LEFT) {
            m_state.fired = true;
            // Update position at fire moment for accuracy
            m_state.x = static_cast<float>(event.button.x);
            m_state.y = static_cast<float>(event.button.y);
        }
    }

    // Track continuous aim position from mouse motion
    if (event.type == SDL_EVENT_MOUSE_MOTION) {
        m_state.x = event.motion.x;
        m_state.y = event.motion.y;
    }

    // -----------------------------------------------------------------------
    // TODO: Replace the above with your real light-gun / IR-camera input.
    //
    //   Common approaches on Raspberry Pi:
    //   1. Wiimote IR + WiiBrew library  (Bluetooth, wiimote IR camera)
    //   2. USB light gun that reports as HID mouse (plug & play)
    //   3. Custom IR LED gun + Pi camera + OpenCV blob detection
    //
    //   All three ultimately give you an (x, y) position + a fire signal,
    //   so just fill m_state.x/y and set m_state.fired = true here.
    // -----------------------------------------------------------------------
}

// ---------------------------------------------------------------------------
// renderCrosshair  –  simple pixel cross-hair for debugging / dev mode
// ---------------------------------------------------------------------------
void LightGun::renderCrosshair(SDL_Renderer* renderer) const {
    constexpr float HALF = 12.0f;
    constexpr float THICK = 2.0f;

    SDL_SetRenderDrawColor(renderer, 255, 50, 50, 200);

    // Horizontal bar
    SDL_FRect hbar = { m_state.x - HALF, m_state.y - THICK * 0.5f,
                       HALF * 2.0f, THICK };
    SDL_RenderFillRect(renderer, &hbar);

    // Vertical bar
    SDL_FRect vbar = { m_state.x - THICK * 0.5f, m_state.y - HALF,
                       THICK, HALF * 2.0f };
    SDL_RenderFillRect(renderer, &vbar);

    // Centre dot
    SDL_FRect dot = { m_state.x - 2.0f, m_state.y - 2.0f, 4.0f, 4.0f };
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderFillRect(renderer, &dot);
}
