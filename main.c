#include "types.h"

#include "physics.c"
#include <SDL3/SDL.h>

#define NUM_OBS 20

// TODO
// * Figure out why the orbiting is so fast now. Notice that the player drifts
// upwards, meaning that the camera is not adjusting enough (player is moving
// too fast). This drift will be bad in the long run.

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
    float orbitYChangeSinceLastFrame;
    float orbitXChangeSinceLastFrame;
    float forwardProgressTravelled;
    Hero hero;
    OrbitData currOrbit;
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
        .orbitYChangeSinceLastFrame = 0.0f,
        .orbitXChangeSinceLastFrame = 0.0f,
        .forwardProgressTravelled = 0.0f,
        .currOrbit = newOrbitData(),
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
                 Obs obs[], float dt, bool spaceKeyPressed, int time) {
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
            state->currOrbit.radius = sqrtf(closestDistSq);

            float dy = (float)hero->y - (float)closestObs.y;
            float dx = (float)hero->x - (float)closestObs.x;
            float cross = dx * -(hero->vy) - dy * hero->vx; // cross product
            float dot = dx * hero->vx + dy * -(hero->vy);

            // We should only go into orbit if the hero is not on a
            // trajectory to collide with this obj
            if (dot >= 0) {
                // We should now be in orbit
                state->isOrbiting = true;

                // Update current orbit
                state->currOrbit.startAngle = atan2f(dy, dx);
                state->currOrbit.startTime = time;
                state->currOrbit.radius = sqrt(closestDistSq);
                state->currOrbit.direction = cross >= 0 ? 1 : -1;
            }
        }
    }

    // Handle orbiting specific logic
    if (state->isOrbiting) {
        Obs closestObs = obs[state->closestObsForOrbit];

        // Update hero.x and hero.y
        // These are the coordinates relative to the closest object
        float currAngle = Orbit_CalculateAngle(
            settings->speed, state->currOrbit.radius,
            state->currOrbit.startAngle, state->currOrbit.direction,
            (float)(time - state->currOrbit.startTime) / 1100.0f);
        Point currRelativePos = Orbit_CalculatePositionRelativeToTarget(
            currAngle, state->currOrbit.radius);
        float relX = currRelativePos.x; // cos is x because y is inverted
        float relY = currRelativePos.y; // sin is y because y is inverted

        // Update hero.x and hero.y (coordinates relative to the screen)
        hero->x = closestObs.x + relX;
        hero->y = closestObs.y + relY;

        // record how much the screen should move to account for the y
        // difference
        float prevAngle = Orbit_CalculateAngle(
            settings->speed, state->currOrbit.radius,
            state->currOrbit.startAngle, state->currOrbit.direction,
            ((float)(time - state->currOrbit.startTime) / 1100.0f) - dt);
        Point prevRelativePos = Orbit_CalculatePositionRelativeToTarget(
            prevAngle, state->currOrbit.radius);
        float prevRelY = prevRelativePos.y;
        float prevRelX = prevRelativePos.x;
        state->orbitYChangeSinceLastFrame = prevRelY - relY;
        state->orbitXChangeSinceLastFrame = prevRelX - relX;

        hero->x -=
            (1.0f - settings->horizontalCameraMovement) * state->orbitXChangeSinceLastFrame;

        // Update hero.vx and charSpeedY based on its current angle
        hero->vx =
            -state->currOrbit.direction * settings->speed * sinf(currAngle);
        hero->vy =
            -1 * state->currOrbit.direction * settings->speed * cosf(currAngle);
    }

    // Handle not-orbiting specific logic
    if (!state->isOrbiting) {
        // It's game over if we are not orbiting and we go out of bounds
        if (Physics_CheckCircleOutOfBoundsX(hero->x, hero->rad, 0,
                                            settings->width)) {
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
                state->orbitYChangeSinceLastFrame; // update progress traveled
            obs[i].y += state->orbitYChangeSinceLastFrame;
            obs[i].x +=
                settings->horizontalCameraMovement * state->orbitXChangeSinceLastFrame;
        }

        Obs currObs = obs[i];
        if (Physics_ApproximateCirclesColliding(hero->x, hero->y, currObs.x,
                                                currObs.y, hero->rad,
                                                currObs.rad)) {
            state->gameOver = true;
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
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // WHITE
        DrawFilledCircle(renderer, currObs.x, currObs.y, currObs.rad);
    }

    SDL_RenderPresent(renderer);

    // Draw the fixed forward progress on the top right as an int
    // TODO: this doesn't seem to work? I guess we will have to use ttf at
    // some point.
    int forwardProgressAsInt = state->forwardProgressTravelled;
    char str[20];
    snprintf(str, sizeof(str), "%d", forwardProgressAsInt);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // WHITE
    SDL_RenderDebugText(renderer, 0, 0, "hello");
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
        processGame(&settings, &state, &hero, entities.obs, dt, spacePressed,
                    SDL_GetTicks());
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
