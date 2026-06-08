#include "types.h"

#include "camera.c"
#include "physics.c"

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
    int includeForwardMovementPowerup;
    int forwardStreakPowerupLengthMs;
    int includeBlackholes;
} GameOptions;

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
    GameOptions gameOptions;
} GameSettings;

typedef struct {
    bool gameOver;
    bool spaceDown;
    bool isOrbiting;
    bool hasOrbitedAtLeastOnce;
    bool forwardStreakPowerupActive;

    // This is used to track when hero is in forward streak powerup
    // and has already collided into an obstacle triggering active powerup.
    bool forwardStreakPowerupCollisionActivated;

    bool isInvincible;
    int forwardProgressPowerupActiveTimerStart;
    int closestObsForOrbit;
    float forwardProgressTravelled;
    float forwardProgressStreak;
    Hero hero;
    OrbitData currOrbit;
    Camera camera; // Is it okay that we copy a camera in?
} GameState;

typedef struct {
    Hero hero;
    ObsSOA *obsSoa;
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
        .forwardProgressStreak = 0.0f,
        .hasOrbitedAtLeastOnce = false,
        .forwardStreakPowerupActive = false,
        .isInvincible = false,
        .currOrbit = newOrbitData(),
        .camera = camera,
    };
    return state;
}

GameSettings newGameSettings(int width, int height) {
    GameOptions options = {
        .includeForwardMovementPowerup = 0,
        .includeBlackholes = 0,
        .forwardStreakPowerupLengthMs = 3000,

    };
    GameSettings settings = {
        .minObsRadius = 0.2f,
        .maxObsRadius = 0.8f,
        .obsYInterval = 10.0,
        .startingObsY = 20.0f,
        .horizontalCameraMovement = 0.15f,
        .width = width,
        .height = height,
        .speed = 20.0f,
        .gameOptions = options,
    };

    return settings;
}

void AppendNewRandomObs(ObsSOA *obsSoa, float screenWidthToWorld,
                        float leftVisibleBound, float rightVisibleBound,
                        float fromY, GameSettings *settings) {
    float XToAdd[OBS_TO_ADD_AT_A_TIME];
    float YToAdd[OBS_TO_ADD_AT_A_TIME];
    float RadToAdd[OBS_TO_ADD_AT_A_TIME];

    for (int i = 0; i < OBS_TO_ADD_AT_A_TIME; i++) {
        float obsRad =
            randFloatBetween(settings->minObsRadius, settings->maxObsRadius);
        float minObsX = leftVisibleBound + obsRad + 1;
        float maxObsX = rightVisibleBound - obsRad - 1;

        XToAdd[i] = randFloatBetween(minObsX, maxObsX);
        YToAdd[i] = fromY + i * settings->obsYInterval;
        RadToAdd[i] = obsRad;
    }
    AddObsMany(obsSoa, OBS_TO_ADD_AT_A_TIME, XToAdd, YToAdd, RadToAdd);
}

void processGame(GameSettings *settings, GameState *state, Hero *hero,
                 ObsSOA *obsSoa, float dt, bool spaceKeyPressed, int time) {
    float screenWidthToWorld =
        Camera_ScreenToWorldMeasurement(state->camera, settings->width);
    float leftVisibleBound = (screenWidthToWorld / 2) * -1;
    float rightVisibleBound = (screenWidthToWorld / 2);
    float screenHeightToWorld =
        Camera_ScreenToWorldMeasurement(state->camera, settings->height);
    float upperVisibleBound = state->camera.y + (screenHeightToWorld / 2);
    float lowerVisibleBound = state->camera.y + (screenHeightToWorld / 2) * -1;

    // -- HANDLE TIMER EVENTS
    // Handle forward movement powerup
    if (settings->gameOptions.includeForwardMovementPowerup &&
        state->forwardStreakPowerupActive) {
        int currentTime = SDL_GetTicks();
        int elapsedMs =
            currentTime - state->forwardProgressPowerupActiveTimerStart;
        if (elapsedMs >= settings->gameOptions.forwardStreakPowerupLengthMs) {
            // TODO: Extract to function
            state->isInvincible = false;
            state->forwardStreakPowerupActive = false;
            state->forwardStreakPowerupCollisionActivated = false;
            // TODO: Don't do this. I guess we should have some build and
            // teardown for each powerup.
            hero->vy /= 1.5;
            hero->vx /= 1.5;
        }
    }

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
            (float)10000.0f; // TODO (aslan): This is obviously not right. What
                             // is the largest float?
        for (int i = 0; i < obsSoa->size; i++) {
            float currObsX = obsSoa->xVals[i];
            float currObsY = obsSoa->yVals[i];
            if (currObsY > upperVisibleBound || currObsY < lowerVisibleBound) {
                continue;
            }
            float distSquared = (currObsX - hero->x) * (currObsX - hero->x) +
                                (currObsY - hero->y) * (currObsY - hero->y);
            if (distSquared < closestDistSq) {
                found = 1;
                state->closestObsForOrbit = i;
                closestDistSq = distSquared;
            }
        }
        // If there is something to orbit, handle
        if (found) {
            float closestObsX = obsSoa->xVals[state->closestObsForOrbit];
            float closestObsY = obsSoa->yVals[state->closestObsForOrbit];
            state->currOrbit.radius = sqrtf(closestDistSq);

            float dy = (float)hero->y - (float)closestObsY;
            float dx = (float)hero->x - (float)closestObsX;
            float cross = dx * hero->vy - dy * hero->vx; // cross product
            float dot = dx * hero->vx + dy * hero->vy;

            // We should only go into orbit if the hero is not on a
            // trajectory to collide with this obj
            if (dot >= 0) {
                // We should now be in orbit
                state->isOrbiting = true;
                state->hasOrbitedAtLeastOnce = true;

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
        // Reset counter for forward streak
        if (settings->gameOptions.includeForwardMovementPowerup) {
            state->forwardProgressStreak = 0.0f;
        }

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
        float closestObsX = obsSoa->xVals[state->closestObsForOrbit];
        float closestObsY = obsSoa->yVals[state->closestObsForOrbit];
        hero->x = closestObsX + currXRelativeToObs;
        hero->y = closestObsY + currYRelativeToObs;
    }

    // Handle not-orbiting specific logic
    if (!state->isOrbiting) {
        // Increase forward progress streak if necessary
        if (settings->gameOptions.includeForwardMovementPowerup &&
            hero->vy > 0 && state->hasOrbitedAtLeastOnce &&
            !state->forwardStreakPowerupActive) {
            state->forwardProgressStreak +=
                1 *
                dt; // If streak has reached threshold time, then we activate
            if (state->forwardProgressStreak >= 0.5) {
                state->forwardStreakPowerupActive = true;
                state->isInvincible = true;
                state->forwardProgressPowerupActiveTimerStart = SDL_GetTicks();
            }
        }

        // Update x and y
        hero->y += hero->vy * dt;
        hero->x += hero->vx * dt;
        // It's game over if we are not orbiting and we go out of bounds
        if (!state->isInvincible &&
            Physics_CheckCircleOutOfBoundsX(
                hero->x, hero->rad, leftVisibleBound, rightVisibleBound)) {
            state->gameOver = true;
        }
        if (state->isInvincible) {
            // If invincible and out of bounds left, bounce right
            if (Physics_CheckCircleOutOfBoundsLeft(hero->x, hero->rad,
                                                   leftVisibleBound)) {
                hero->x = leftVisibleBound - hero->rad;
                hero->vx *= -1;
            }
            // If invincible and out of bounds right, bouce left
            if (Physics_CheckCircleOutOfBoundsRight(hero->x, hero->rad,
                                                    rightVisibleBound)) {
                hero->x = rightVisibleBound + hero->rad;
                hero->vx *= -1;
            };
        }
    }

    // Update obs locations
    for (int i = 0; i < obsSoa->size; i++) {
        float currObsX = obsSoa->xVals[i];
        float currObsY = obsSoa->yVals[i];
        float currObsRad = obsSoa->radVals[i];
        if (Physics_ApproximateCirclesColliding(
                hero->x, hero->y, currObsX, currObsY, hero->rad, currObsRad)) {

            if (!state->isInvincible) {
                state->gameOver = true;
            }

            // If the forwardStreakPowerupActive is active
            if (settings->gameOptions.includeForwardMovementPowerup &&
                state->isInvincible && state->forwardStreakPowerupActive &&
                !state->forwardStreakPowerupCollisionActivated) {
                state->forwardStreakPowerupCollisionActivated = true;
                hero->vy *= 1.5;
                hero->vx *= 1.5;
            }
        }
    }

    // Update camera
    float worldHeight =
        Camera_ScreenToWorldMeasurement(state->camera, settings->height);
    state->camera.x =
        0.25 * hero->x; // Camera x should be 25% of hero x diff relative to 0
    state->camera.y =
        hero->y +
        (0.15 * worldHeight); // hero should be towards bottom of screen a bit

    // Update our forward progress
    state->forwardProgressTravelled = hero->y;

    // If we need to create new obstacles, do that now
    if (obsSoa->size == 0) {
        return;
    }
    if (obsSoa->size < 10) { // if there are only 10, check index size // 2
        int indexToCheck = obsSoa->size / 2;
        if (hero->y >= obsSoa->yVals[indexToCheck]) {
            float newStartingY =
                obsSoa->yVals[obsSoa->size - 1] + settings->obsYInterval;
            AppendNewRandomObs(obsSoa, screenWidthToWorld, leftVisibleBound,
                               rightVisibleBound, newStartingY, settings);
        }
    } else { // check 5 before
        int indexToCheck = obsSoa->size - 5;
        if (hero->y >= obsSoa->yVals[indexToCheck]) {
            float newStartingY =
                obsSoa->yVals[obsSoa->size - 1] + settings->obsYInterval;
            AppendNewRandomObs(obsSoa, screenWidthToWorld, leftVisibleBound,
                               rightVisibleBound, newStartingY, settings);
        }
    }
}

// TODO: A lot of the drawing here will likely be replaced with game assets
void renderGame(SDL_Renderer *renderer, GameState *state,
                GameSettings *settings, Hero *hero, ObsSOA *obsSoa,
                TTF_Text *scoreTextObj) {
    // Draw boundaries on the left and right
    float screenWidthToWorld =
        Camera_ScreenToWorldMeasurement(state->camera, settings->width);
    float leftBound = (screenWidthToWorld / 2) * -1;
    float rightBound = (screenWidthToWorld / 2);
    float leftScreenX =
        Camera_WorldPositionToScreen(state->camera, leftBound, 0,
                                     settings->width, settings->height)
            .x -
        1;
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

    if (settings->gameOptions.includeForwardMovementPowerup && state->forwardStreakPowerupActive) {
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // RED
    } else {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // WHITE
    }
    Draw_DrawFilledCircle(renderer, heroScreenPos.x, heroScreenPos.y,
                          heroScreenRad);

    // Draw line to signal orbit
    float closestObsX = obsSoa->xVals[state->closestObsForOrbit];
    float closestObsY = obsSoa->yVals[state->closestObsForOrbit];
    Point closestObsScreenPos =
        Camera_WorldPositionToScreen(state->camera, closestObsX, closestObsY,
                                     settings->width, settings->height);
    if (state->spaceDown) {
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // RED
        SDL_RenderLine(renderer, heroScreenPos.x, heroScreenPos.y,
                       closestObsScreenPos.x, closestObsScreenPos.y);
    }

    // Draw the obstacles
    for (int i = 0; i < obsSoa->size; i++) {
        float currObsX = obsSoa->xVals[i];
        float currObsY = obsSoa->yVals[i];
        float currObsRad = obsSoa->radVals[i];
        Point currObsScreenPos =
            Camera_WorldPositionToScreen(state->camera, currObsX, currObsY,
                                         settings->width, settings->height);
        float currObsScreenRad =
            Camera_WorldMeasurementToScreen(state->camera, currObsRad);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // WHITE
        Draw_DrawFilledCircle(renderer, currObsScreenPos.x, currObsScreenPos.y,
                              currObsScreenRad);
    }

    // Draw the fixed forward progress on the top right as an int
    // TODO: this doesn't seem to work? I guess we will have to use ttf at
    // some point.
    int forwardProgressAsInt = state->forwardProgressTravelled;
    char str[20];
    snprintf(str, sizeof(str), "%d", forwardProgressAsInt);

    TTF_SetTextString(scoreTextObj, str, 0);
    TTF_DrawRendererText(scoreTextObj, 20, 20);

    SDL_RenderPresent(renderer);
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

    if (!SDL_CreateWindowAndRenderer("OML", width, height, 0, &window,
                                     &renderer)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    };
    SDL_RaiseWindow(window);

    // Init font rendering
    TTF_Init();
    TTF_Font *font = TTF_OpenFont("./assets/Jersey10-Regular.ttf", 36.0f);
    if (!font) {
        SDL_Log("Failed to load font: %s", SDL_GetError());
        return 1;
    }

    // Create font engine
    TTF_TextEngine *textEngine = TTF_CreateRendererTextEngine(renderer);
    TTF_Text *scoreTextObj =
        TTF_CreateText(textEngine, font, "Hello, world", 0);

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
    ObsSOA obsSoa = NewObsSOA();
    GameEntities entities = {
        .hero = hero,
        .obsSoa = &obsSoa,
    };
    float screenWidthToWorld =
        Camera_ScreenToWorldMeasurement(state.camera, settings.width);
    float leftVisibleBound = (screenWidthToWorld / 2) * -1;
    float rightVisibleBound = (screenWidthToWorld / 2);
    AppendNewRandomObs(entities.obsSoa, screenWidthToWorld, leftVisibleBound,
                       rightVisibleBound, settings.startingObsY, &settings);

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

        // TODO: Move this out to some event tracking struct + function to
        // update it Poll events
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
        processGame(&settings, &state, &hero, entities.obsSoa, dt, spacePressed,
                    SDL_GetTicks());
        renderGame(renderer, &state, &settings, &hero, entities.obsSoa,
                   scoreTextObj);

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

    TTF_CloseFont(font);
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
