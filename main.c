#include "math.h"
#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_INT32 2147483647
#define DEBUG 0
#define NUM_OBS 20

// Function to draw a circle in SDL
void DrawFilledCircle(SDL_Renderer *renderer, int xC, int yC, int radius) {
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (x * x + y * y <= radius * radius) {
                SDL_RenderPoint(renderer, xC + x, yC + y);
            }
        }
    }
}

typedef struct {
    float x;
    float y;
    float rad;
} Obs;

typedef struct {
    float x;
    float y;
    float rad;
} Blackhole;

typedef struct {
    float x;
    float y;
    float vx;
    float vy;
    float rad;
} Hero;

typedef struct {
    int numObs;
    int minObsRadius;
    int maxObsRadius;
    int obsYDiff;
    int startingObsY;
    int width;
    int height;
    float speed;
    float horizontalCameraMovement;
} GameSettings;

typedef struct {
    bool gameOver;
    bool spaceDown;
    bool isOrbiting;
    int closestObsForOrbit;
    int orbitDir; // 1 is clockwise, -1 is counterclockwise
    float orbitRadius;
    float orbitTime;
    float orbitInitialAngle;
    float orbitYOffset;
    float orbitXOffset;
    float forwardProgressTravelled;
    Hero hero;
} GameState;

typedef struct {
    Hero hero;
    Obs obs[NUM_OBS];
} GameEntities;

Obs newObs(int x, int y, int rad) {
    Obs obs = {
        .x = x,
        .y = y,
        .rad = rad,
    };
    return obs;
}

int randIntBetween(int low, int high) { return (rand() % (high - low)) + low; }

GameState newGameState() {
    GameState state = {
        .gameOver = false,
        .spaceDown = false,
        .isOrbiting = false,
        .closestObsForOrbit = 0.0f,
        .orbitDir = -1,
        .orbitRadius = 0.0f,
        .orbitTime = 0.0f,
        .orbitInitialAngle = 0.0f,
        .orbitYOffset = 0.0f,
        .orbitXOffset = 0.0f,
        .forwardProgressTravelled = 0.0f,
    };
    return state;
}

GameSettings newGameSettings(int width, int height) {
    GameSettings settings = {
        .minObsRadius = 10,
        .maxObsRadius = 40,
        .obsYDiff = 400,
        .startingObsY = 0,
        .horizontalCameraMovement = 0.15f,
        .width = width,
        .height = height,
        .speed = 1000.0f,
    };

    return settings;
}

void processGame(GameSettings *settings, GameState *state, Hero *hero,
                 Obs obs[], float dt, bool spaceKeyPressed) {
    // The moment the spacebar is clicked
    if (!state->spaceDown && spaceKeyPressed) {
        state->spaceDown = true;
    }

    // Spacebar is let go
    if (state->spaceDown && !spaceKeyPressed) {
        state->spaceDown = false;
        state->isOrbiting = false;
    }

    // If space is down, and we haven't started orbiting, do the necessary
    // processing to check if we should go into an orbiting state
    if (state->spaceDown && !state->isOrbiting) {
        // We need to record the distance from the closest obs
        // TODO: Only look at the currently visible obs for performance
        int closestDistSq = MAX_INT32;
        for (int i = 0; i < NUM_OBS; i++) {
            Obs currObs = obs[i];
            if (currObs.y > settings->height || currObs.y < 0) {
                continue;
            }
            int distSquared = (currObs.x - hero->x) * (currObs.x - hero->x) +
                              (currObs.y - hero->y) * (currObs.y - hero->y);
            if (distSquared < closestDistSq) {
                state->closestObsForOrbit = i;
                closestDistSq = distSquared;
            }
        }
        // If there is something to orbit, handle
        if (closestDistSq != MAX_INT32) {
            Obs closestObs = obs[state->closestObsForOrbit];
            state->orbitRadius = sqrtf(closestDistSq);

            float dy = (float)hero->y - (float)closestObs.y;
            float dx = (float)hero->x - (float)closestObs.x;
            float cross = dx * -(hero->vy) - dy * hero->vx; // cross product
            float dot = dx * hero->vx + dy * -(hero->vy);

            // We should only go into orbit if the hero is not on a
            // trajectory to collide with this obj
            if (dot >= 0) {
                // We should now be in orbit
                state->isOrbiting = true;

                // Calculate the orbit initial angle
                state->orbitInitialAngle = atan2f(dy, dx);

                // This calculates how we can get a "swingin on a pole
                // effect" For example, if character is going upwards and
                // towards the bottom left of an obstacle, then on spacebar,
                // it will orbit clockwise
                if (cross >= 0) {
                    state->orbitDir = 1;
                } else {
                    state->orbitDir = -1;
                }

                // set orbit frames
                state->orbitTime = 0.0f;
            }
        }
    }

    // Handle orbiting specific logic
    if (state->isOrbiting) {
        Obs closestObs = obs[state->closestObsForOrbit];
        // update hero.x and hero.y
        // we want to increase the arc by speed, so we should
        // increase the angle by speed / orbit radius every frame
        float angularVelocity = settings->speed / state->orbitRadius;
        float prevAngle =
            state->orbitInitialAngle +
            state->orbitDir * angularVelocity * (state->orbitTime - dt);
        float currAngle = state->orbitInitialAngle +
                          state->orbitDir * angularVelocity * state->orbitTime;

        // These are the coordinates relative to the closest object
        float relX = state->orbitRadius * cosf(currAngle);
        float relY = state->orbitRadius * sinf(currAngle);

        // Update hero.x and hero.y (coordinates relative to the screen)
        hero->x = closestObs.x + relX;
        hero->y = closestObs.y + relY;

        // record how much the screen should move to account for the y
        // difference
        float prevRelY = state->orbitRadius * sinf(prevAngle);
        state->orbitYOffset = prevRelY - relY;

        float prevRelX = state->orbitRadius * cosf(prevAngle);
        state->orbitXOffset = prevRelX - relX;
        hero->x -=
            (1.0f - settings->horizontalCameraMovement) * state->orbitXOffset;

        // Update hero.vx and charSpeecY based on its current angle
        hero->vx = -state->orbitDir * settings->speed * sinf(currAngle);
        hero->vy = -1 * state->orbitDir * settings->speed * cosf(currAngle);

        // Now increment orbitFrames
        state->orbitTime += dt;
    }

    // Handle not-orbiting specific logic
    if (!state->isOrbiting) {
        // It's game over if we are not orbiting and we go out of bounds
        if (hero->x - hero->rad >= settings->width ||
            hero->x + hero->rad <= 0) {
            state->gameOver = true;
        }

        // Subtract the proportion of the hero x to create horizontal camera
        // movement
        hero->x +=
            (1.0f - settings->horizontalCameraMovement) * (hero->vx * dt);
    }

    // Update obs locations
    for (int i = 0; i < NUM_OBS; i++) {
        // If orbiting, move stuff downwards
        if (!state->isOrbiting) {
            state->forwardProgressTravelled +=
                hero->vy * dt; // update progress traveled
            obs[i].y += hero->vy * dt;
            obs[i].x -= settings->horizontalCameraMovement * (hero->vx * dt);
        } else { // otherwise, move the "camera" y based on the orbit
                 // movement
            state->forwardProgressTravelled +=
                state->orbitYOffset; // update progress traveled
            obs[i].y += state->orbitYOffset;
            obs[i].x +=
                settings->horizontalCameraMovement * state->orbitXOffset;
        }
    }
}

void renderGame(SDL_Renderer *renderer, GameState *state,
                GameSettings *settings, Hero *hero, Obs obs[]) {
    // TODO: Extract this out when we support black holes
    // bool blackhole = false;

    // Draw character
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // WHITE
    DrawFilledCircle(renderer, hero->x, hero->y, hero->rad);

    // Draw line to signal orbit
    Obs closestObs = obs[state->closestObsForOrbit];
    if (state->spaceDown) {
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // RED
        SDL_RenderLine(renderer, hero->x, hero->y, closestObs.x, closestObs.y);
    }

    // Draw the obstacles
    for (int i = 0; i < NUM_OBS; i++) {
        Obs currObs = obs[i];

        // While we are looping, check that char is colliding with obs
        float distX = (float)hero->x - (float)currObs.x;
        float distY = (float)hero->y - (float)currObs.y;

        float distSquared = (distX * distX) + (distY * distY);
        float radiiSumSquared =
            (hero->rad + currObs.rad) * (hero->rad + currObs.rad);
        if (distSquared <= radiiSumSquared) {
            state->gameOver = true;
        }
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // WHITE
        DrawFilledCircle(renderer, currObs.x, currObs.y, currObs.rad);
    }

    SDL_RenderPresent(renderer);

    // Draw the fixed forward progress on the top right as an int
    // int forwardProgressAsInt = state->forwardProgressTravelled;
    // char str[12];
    // SDL_RenderDebugText(renderer, settings->width - 70, 20, sprintf(str,
    // "%d", forwardProgressAsInt));
}

int main() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Renderer *renderer = NULL;
    SDL_Window *window = NULL;

    int width = 500;
    int height = 1000;

    if (!SDL_CreateWindowAndRenderer("OML Clone", width, height, 0, &window,
                                     &renderer)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    };

    SDL_RaiseWindow(window);

    // Init game
    GameSettings settings = newGameSettings(width, height);
    GameState state = newGameState();

    // Generate Hero
    Hero hero = {
        .x = (float)width / 2,
        .y = (float)height * 0.65,
        .vx = 0,
        .vy = settings.speed,
        .rad = 15,
    };

    // Generate obstacles
    GameEntities entities = {
        .hero = hero,
        .obs = {},
    };
    for (int i = 0; i < NUM_OBS; i++) {
        int obsRad =
            randIntBetween(settings.minObsRadius, settings.maxObsRadius);
        int minObsX = obsRad;
        int maxObsX = width - obsRad;
        int obsX = randIntBetween(minObsX, maxObsX);
        int obsY = settings.startingObsY - i * settings.obsYDiff;
        entities.obs[i] = newObs(obsX, obsY, obsRad);
    }

    float targetFps = 120;
    int quit = 0;
    SDL_Event event;
    uint64_t frameEnd = SDL_GetTicks();
    bool spacePressed = false;
    while (!quit && !state.gameOver) {
        // INITIALIZE FRAME
        uint64_t frameStart = SDL_GetTicks();

        // dt is the amount of seconds since the last frame
        float dt = (float)(frameStart - frameEnd) / 1000.0f;

        // Poll events
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_QUIT:
                quit = 1;
                break;
            case SDL_EVENT_KEY_DOWN:
                spacePressed = true;
                break;
            case SDL_EVENT_KEY_UP:
                spacePressed = false;
                break;
            }
        }

        // ------- HANDLE GAME AND RENDER GRAPHICS -------
        // Black background
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);
        processGame(&settings, &state, &hero, entities.obs, dt, spacePressed);
        renderGame(renderer, &state, &settings, &hero, entities.obs);

        // ------ END GAME AND RENDERING -------
        // End of frame processing
        // ms since start of frame is frameEnd - frameStart
        frameEnd = SDL_GetTicks();

        // now to hit our target fps we should sleep for (1000/targetFps) -
        // (frameEnd - frameStart)
        int toWait = (1000.0f / targetFps) - (frameEnd - frameStart);
        if (toWait > 0) {
            SDL_Delay(toWait);
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
