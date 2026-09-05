#pragma once
#include "types.h"

// -------------------------------- SDL HELPERS --------------------------------
#ifdef OML_SDL
// Helper functions not provided by SDL3
//
// Function to draw a circle in SDL
void Draw_DrawFilledCircle(SDL_Renderer *renderer, float xC, float yC, float radius) {
    int radInt = (int) radius;
    for (int y = -radInt; y <= radInt; y++) {
        for (int x = -radInt; x <= radInt; x++) {
            if (x * x + y * y <= radius * radius) {
                SDL_RenderPoint(renderer, xC + x, yC + y);
            }
        }
    }
}
#endif
