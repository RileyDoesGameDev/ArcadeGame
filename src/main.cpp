#include "Game.h"
#include <iostream>

// ---------------------------------------------------------------------------
// Entry point
//
//   Build (Pi / Linux):
//     mkdir build && cd build
//     cmake .. -DCMAKE_BUILD_TYPE=Release
//     make -j4
//     ./retro_arcade
//
//   Fullscreen on the arcade cabinet:
//     Just flip the last argument of game.init() to `true`.
// ---------------------------------------------------------------------------
int main(int /*argc*/, char* /*argv*/[]) {
    Game game;

    constexpr int  W          = 1280;
    constexpr int  H          = 720;
    constexpr bool FULLSCREEN = false;  // Set true for the physical cabinet

    if (!game.init("Retro Hunt", W, H, FULLSCREEN)) {
        std::cerr << "Failed to initialise the game.\n";
        return 1;
    }

    game.run();
    return 0;
}
