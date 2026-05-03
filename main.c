#include "types.h"

#include "camera.c"
#include "physics.c"
#include <SDL3/SDL.h>

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
    float minObsRadius;
    float maxObsRadius;
    float obsYInterval;
    float startingObsY;
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
    float forwardProgressTravelled;
    Hero hero;
    OrbitData currOrbit;
    Camera camera; // Is it okay that we copy a camera in?
} GameState;

typedef struct {
    Hero hero;
    Obs obs[NUM_OBS];
} GameEntities;

float PIXELS_PER_METER = 50;

Obs newObs(float x, float y, float rad) {
    Obs obs = {
        .x = x,
        .y = y,
        .rad = rad,
    };
    return obs;
}

float randFloatBetween(float min, float max) {
    return ((float)rand() / (float)RAND_MAX) * (max - min) + min;
}

GameState newGameState(Camera camera) {
    GameState state = {
        .gameOver = false,
        .spaceDown = false,
        .isOrbiting = false,
        .closestObsForOrbit = 0.0f,
        .forwardProgressTravelled = 0.0f,
        .currOrbit = newOrbitData(),
        .camera = camera,
    };
    return state;
}

GameSettings newGameSettings(int width, int height) {
    GameSettings settings = {
        .minObsRadius = 0.2f,
        .maxObsRadius = 0.8f,
        .obsYInterval = 10.0,
        .startingObsY = 20.0f,
        .horizontalCameraMovement = 0.15f,
        .width = width,
        .height = height,
        .speed = 20.0f,
    };

    return settings;
}

void processGame(GameSettings *settings, GameState *state, Hero *hero,
                 Obs obs[], float dt, bool spaceKeyPressed, int time) {
    float screenWidthToWorld =
        Camera_ScreenToWorldMeasurement(state->camera, settings->width);
    float leftVisibleBound = (screenWidthToWorld / 2) * -1;
    float rightVisibleBound = (screenWidthToWorld / 2);
    float screenHeightToWorld =
        Camera_ScreenToWorldMeasurement(state->camera, settings->height);
    float upperVisibleBound = state->camera.y + (screenHeightToWorld / 2);
    float lowerVisibleBound = state->camera.y + (screenHeightToWorld / 2) * -1;

    // --- HANDLE INPUT ---
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
        int found = false;
        float closestDistSq =
            (float)10000.0f; // TODO (aslan): This is obviously not right. What is the largest float?
        for (int i = 0; i < NUM_OBS; i++) {
            Obs currObs = obs[i];
            if (currObs.y > upperVisibleBound ||
                currObs.y < lowerVisibleBound) {
                continue;
            }
            float distSquared = (currObs.x - hero->x) * (currObs.x - hero->x) +
                                (currObs.y - hero->y) * (currObs.y - hero->y);
            if (distSquared < closestDistSq) {
                found = 1;
                state->closestObsForOrbit = i;
                closestDistSq = distSquared;
            }
        }
        // If there is something to orbit, handle
        if (found) {
            Obs closestObs = obs[state->closestObsForOrbit];
            state->currOrbit.radius = sqrtf(closestDistSq);

            float dy = (float)hero->y - (float)closestObs.y;
            float dx = (float)hero->x - (float)closestObs.x;
            float cross = dx * hero->vy - dy * hero->vx; // cross product
            float dot = dx * hero->vx + dy * hero->vy;

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
        // Update hero.x and hero.y
        // These are the coordinates relative to the closest object
        float currAngle = Orbit_CalculateAngle(
            settings->speed, state->currOrbit.radius,
            state->currOrbit.startAngle, state->currOrbit.direction,
            (float)(time - state->currOrbit.startTime) / 1000.0f);
        Point currPosRelativeToObs = Orbit_CalculatePositionRelativeToTarget(
            currAngle, state->currOrbit.radius);
        float currXRelativeToObs = currPosRelativeToObs.x;
        float currYRelativeToObs = currPosRelativeToObs.y;

        // Update hero.vx and charSpeedY based on its current angle
        hero->vx =
            -(state->currOrbit.direction) * settings->speed * sinf(currAngle);
        hero->vy =
            state->currOrbit.direction * settings->speed * cosf(currAngle);

        // Update hero.x and hero.y (coordinates relative to the screen)
        Obs closestObs = obs[state->closestObsForOrbit];
        hero->x = closestObs.x + currXRelativeToObs;
        hero->y = closestObs.y + currYRelativeToObs;
    }

    // Handle not-orbiting specific logic
    if (!state->isOrbiting) {
        hero->y += hero->vy * dt;
        hero->x += hero->vx * dt;
        // It's game over if we are not orbiting and we go out of bounds
        if (Physics_CheckCircleOutOfBoundsX(
                hero->x, hero->rad, leftVisibleBound, rightVisibleBound)) {
            state->gameOver = true;
        }
    }

    // Update obs locations
    for (int i = 0; i < NUM_OBS; i++) {
        Obs currObs = obs[i];
        if (Physics_ApproximateCirclesColliding(hero->x, hero->y, currObs.x,
                                                currObs.y, hero->rad,
                                                currObs.rad)) {
            state->gameOver = true;
        }
    }

    // Update camera
    float worldHeight = Camera_ScreenToWorldMeasurement(state->camera, settings->height);
    state->camera.x = 0.25 * hero->x; // Camera x should be 25% of hero x diff relative to 0
    state->camera.y = hero->y + (0.15 * worldHeight); // hero should be towards bottom of screen a bit

    // SDL_Log("%.2f, %.2f", hero->x, hero->y);
}

void renderGame(SDL_Renderer *renderer, GameState *state,
                GameSettings *settings, Hero *hero, Obs obs[]) {
    // Draw boundaries on the left and right
    float screenWidthToWorld =
        Camera_ScreenToWorldMeasurement(state->camera, settings->width);
    float leftBound = (screenWidthToWorld / 2) * -1;
    float rightBound = (screenWidthToWorld / 2);
    float leftScreenX =
        Camera_WorldPositionToScreen(state->camera, leftBound, 0,
                                     settings->width, settings->height)
            .x - 1;
    float rightScreenX =
        Camera_WorldPositionToScreen(state->camera, rightBound, 0,
                                     settings->width, settings->height)
            .x;
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // WHITE
    // Left boundary
    SDL_RenderLine(renderer, leftScreenX,
                   0, // top of screen
                   leftScreenX,
                   settings->height // bottom of screen
    );

    // Right boundary
    SDL_RenderLine(renderer, rightScreenX,
                   0, // top of screen
                   rightScreenX,
                   settings->height // bottom of screen
    );

    // TODO: Extract this out when we support black holes
    // bool blackhole = false;

    // Draw character
    Point heroScreenPos = Camera_WorldPositionToScreen(
        state->camera, hero->x, hero->y, settings->width, settings->height);
    float heroScreenRad =
        Camera_WorldMeasurementToScreen(state->camera, hero->rad);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // WHITE
    DrawFilledCircle(renderer, heroScreenPos.x, heroScreenPos.y, heroScreenRad);

    // Draw line to signal orbit
    Obs closestObs = obs[state->closestObsForOrbit];
    Point closestObsScreenPos =
        Camera_WorldPositionToScreen(state->camera, closestObs.x, closestObs.y,
                                     settings->width, settings->height);
    if (state->spaceDown) {
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // RED
        SDL_RenderLine(renderer, heroScreenPos.x, heroScreenPos.y,
                       closestObsScreenPos.x, closestObsScreenPos.y);
    }

    // Draw the obstacles
    for (int i = 0; i < NUM_OBS; i++) {
        Obs currObs = obs[i];
        Point currObsScreenPos =
            Camera_WorldPositionToScreen(state->camera, currObs.x, currObs.y,
                                         settings->width, settings->height);
        float currObsScreenRad =
            Camera_WorldMeasurementToScreen(state->camera, currObs.rad);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // WHITE
        DrawFilledCircle(renderer, currObsScreenPos.x, currObsScreenPos.y,
                         currObsScreenRad);
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
    Camera camera = {
        .x = 0,
        .y = 0,
        .pixelsPerMeter = PIXELS_PER_METER,
    };
    GameState state = newGameState(camera);

    // Generate Hero
    Hero hero = {
        .x = 0,
        .y = 0,
        .vx = 0,
        .vy = settings.speed,
        .rad = 0.3,
    };

    // Generate obstacles
    GameEntities entities = {
        .hero = hero,
        .obs = {},
    };
    float screenWidthToWorld =
        Camera_ScreenToWorldMeasurement(state.camera, settings.width);
    float leftVisibleBound = (screenWidthToWorld / 2) * -1;
    float rightVisibleBound = (screenWidthToWorld / 2);
    for (int i = 0; i < NUM_OBS; i++) {
        float obsRad =
            randFloatBetween(settings.minObsRadius, settings.maxObsRadius);
        float minObsX = leftVisibleBound + obsRad + 1;
        float maxObsX = rightVisibleBound - obsRad - 1;
        float obsX = randFloatBetween(minObsX, maxObsX);
        float obsY = settings.startingObsY + i * settings.obsYInterval;
        entities.obs[i] = newObs(obsX, obsY, obsRad);
    }

    float targetFps = 120;
    int quit = 0;
    SDL_Event event;
    bool spacePressed = false;

    uint64_t lastFrameTime = SDL_GetTicks();
    while (!quit && !state.gameOver) {
        // INITIALIZE FRAME
        uint64_t frameStart = SDL_GetTicks();

        // dt is the amount of seconds since the last frame
        float dt = (float)(frameStart - lastFrameTime) / 1000.0f;
        lastFrameTime = frameStart;

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
        uint64_t frameEnd = SDL_GetTicks();

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
