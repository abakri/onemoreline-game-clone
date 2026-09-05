#pragma once
#include "types.h"

// -------------------------------- SDL HELPERS --------------------------------
#ifdef OML_SDL
// Helper functions not provided by SDL3
//
// Function to draw a circle in SDL
void Draw_DrawFilledCircle(SDL_Renderer *renderer, int xC, int yC, int radius) {
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (x * x + y * y <= radius * radius) {
                SDL_RenderPoint(renderer, xC + x, yC + y);
            }
        }
    }
}
#endif
