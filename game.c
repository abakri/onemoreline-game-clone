#pragma once
#include "types.h"

#define OBS_X_SALT 1
#define OBS_Y_SALT 2
#define OBS_RAD_SALT 3
#define MAX_FLOAT 3.40282e+38

typedef struct {
    uint32_t seed;
    float obsStartingY;
    float obsAverageYDiff;
    float obsYJitter;
    float obsMinRadius;
    float obsMaxRadius;
    float obsXMargin; // The minimum spacing to the left or right of an Obs
    float width;
} WorldGen;

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
    int includeBlackholes;
} GameOptions;

typedef struct {
    int numObs;
    float minObsRadius;
    float maxObsRadius;
    float minObsYInterval;
    float maxObsYInterval;
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
    Hero hero;
    OrbitData currOrbit;
    Camera *camera;
} GameState;

typedef struct {
    Hero hero;
    ObsSOA *obsSoa;
} GameEntities;

float PIXELS_PER_METER = 50;

WorldGen newWorldGen(int32_t seed) {
    WorldGen world = {
        .seed = seed,
        .obsStartingY = 20.0f,
        .obsAverageYDiff = 8.5f,
        .obsYJitter = 1.75f,
        .obsMinRadius = 0.2f,
        .obsMaxRadius = 0.8f,
        .obsXMargin = 1.0f,
        .width = 10.0f,
    };
    return world;
}

Obs newObs(float x, float y, float rad) {
    Obs obs = {
        .x = x,
        .y = y,
        .rad = rad,
    };
    return obs;
}

GameState newGameState(Camera *camera) {
    GameState state = {
        .gameOver = false,
        .spaceDown = false,
        .isOrbiting = false,
        .currOrbit = newOrbitData(),
        .camera = camera,
    };
    return state;
}

GameSettings newGameSettings(int width, int height) {
    GameOptions options = {
        .includeBlackholes = 0,
    };
    GameSettings settings = {
        .minObsRadius = 0.2f,
        .maxObsRadius = 0.8f,
        .minObsYInterval = 5.0f,
        .maxObsYInterval = 12.0f,
        .obsYInterval = 10.0f,
        .startingObsY = 20.0f,
        .horizontalCameraMovement = 0.15f,
        .width = width,
        .height = height,
        .speed = 20.0f,
        .gameOptions = options,
    };

    return settings;
}

// Deterministic Obs by sequence based on seed
Obs ObsAt(int i, WorldGen *w) {
    float randomUnitForRad =
        OmlMath_HashToUnit(OmlMath_Hash(w->seed, i, OBS_RAD_SALT));
    float rad =
        OmlMath_Lerp(w->obsMinRadius, w->obsMaxRadius, randomUnitForRad);

    float randomUnitForJitter =
        OmlMath_HashToUnit(OmlMath_Hash(w->seed, i, OBS_Y_SALT));
    float jitterLo = w->obsYJitter * -1.f;
    float jitterHi = w->obsYJitter * 1.f;
    float y = w->obsStartingY + i * w->obsAverageYDiff +
              OmlMath_Lerp(jitterLo, jitterHi, randomUnitForJitter);

    float randomUnitForX =
        OmlMath_HashToUnit(OmlMath_Hash(w->seed, i, OBS_X_SALT));
    float minX = (w->width * -0.5f) + 1.f + rad;
    float maxX = (w->width * 0.5f) - 1.f - rad;
    float x = OmlMath_Lerp(minX, maxX, randomUnitForX);

    Obs obs = {
        .y = y,
        .x = x,
        .rad = rad,
    };

    return obs;
}

float GetDistSquaredFrom(float x, float y, float targetX, float targetY) {
    return (targetX - x) * (targetX - x) + (targetY - y) * (targetY - y);
}

Obs GetClosestObsToPoint(float x, float y, WorldGen *worldGen) {
    int loIndex = (int)((y - worldGen->obsStartingY - worldGen->obsYJitter) /
                        worldGen->obsAverageYDiff) -
                  1;
    // If the location is before the obs starting y, then we know 0 is the first
    // obs.
    if (y <= worldGen->obsStartingY) {
        loIndex = 0;
    }

    float shortestDistSquared = MAX_FLOAT;
    int shortestDistanceObsIndex = 0;

    Obs candidates[3] = {
        ObsAt(loIndex, worldGen),
        ObsAt(loIndex + 1, worldGen),
        ObsAt(loIndex + 2, worldGen),
    };

    for (int i = 0; i < 3; i++) {
        Obs obs = candidates[i];
        float distSquared = GetDistSquaredFrom(x, y, obs.x, obs.y);
        if (distSquared < shortestDistSquared) {
            shortestDistSquared = distSquared;
            shortestDistanceObsIndex = i;
        }
    }

    return candidates[shortestDistanceObsIndex];
}

// Get the lo and hi sequence for Obs given a lo and hi Y world position.
// For example, you can get the sequences for obs that are on the currently
// visible bounds of the screen. This helps to only fetch visible Obs.
//
// Pass in *iLo and *iHi, and this function will fill mutate those values.
void ObsRangeForYs(WorldGen *w, float yLo, float yHi, int *iLo, int *iHi) {
    float avgYDiff = w->obsAverageYDiff;
    float yJitter = w->obsYJitter;
    int lo = (int)((yLo - w->obsStartingY - yJitter) / avgYDiff) - 1;
    int hi = (int)((yHi - w->obsStartingY + yJitter) / avgYDiff) + 1;

    // Bound lo to 0
    if (lo < 0) {
        lo = 0;
    }

    // So looping from lo to hi is a no-op
    if (hi < lo) {
        hi = lo - 1;
    }
    *iLo = lo;
    *iHi = hi;
}

void updateGame(WorldGen *worldGen, GameSettings *settings, GameState *state,
                Hero *hero, float timeSinceLastFrameSeconds,
                int spaceKeyPressed, int timeSinceStartMs) {
    // Clamp dt to our max dt. If dt comes back super high, then we can
    // experience tunneling.
    if (timeSinceLastFrameSeconds > MAX_DT) {
        timeSinceLastFrameSeconds = MAX_DT;
    }

    float screenWidthToWorld =
        Camera_ScreenToWorldMeasurement(state->camera, settings->width);
    float leftVisibleBound = (screenWidthToWorld / 2) * -1;
    float rightVisibleBound = (screenWidthToWorld / 2);
    float screenHeightToWorld =
        Camera_ScreenToWorldMeasurement(state->camera, settings->height);
    float upperVisibleBoundMeters = state->camera->y + (screenHeightToWorld / 2);
    float lowerVisibleBoundMeters =
        state->camera->y + (screenHeightToWorld / 2) * -1;

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

    // Fetch Obs

    // If space is down, and we haven't started orbiting, do the necessary
    // processing to check if we should go into an orbiting state
    if (state->spaceDown && !state->isOrbiting) {
        Obs closestObs = GetClosestObsToPoint(hero->x, hero->y, worldGen);

        // Consider an orbiting candidate found only if it is visible on screen
        int found = lowerVisibleBoundMeters <= closestObs.y &&
                    closestObs.y <= upperVisibleBoundMeters;

        // If there is something to orbit, handle
        if (found) {
            state->currOrbit.radius = OmlMath_Sqrtf(GetDistSquaredFrom(
                hero->x, hero->y, closestObs.x, closestObs.y));

            float dy = hero->y - closestObs.y;
            float dx = hero->x - closestObs.x;
            float cross = dx * hero->vy - dy * hero->vx; // cross product
            float dot = dx * hero->vx + dy * hero->vy;

            // We should only go into orbit if the hero is not on a
            // trajectory to collide with this obj
            if (dot >= 0) {
                // We should now be in orbit
                state->isOrbiting = true;

                // Update current orbit
                // Ensure that we latch on to the specific orbit x and y
                state->currOrbit.centerX = closestObs.x;
                state->currOrbit.centerY = closestObs.y;
                state->currOrbit.startAngle = OmlMath_Atan2f(dy, dx);
                state->currOrbit.startTime = timeSinceStartMs;
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
            (float)(timeSinceStartMs - state->currOrbit.startTime) / 1000.0f);
        Point currPosRelativeToObs = Orbit_CalculatePositionRelativeToTarget(
            currAngle, state->currOrbit.radius);
        float currXRelativeToObs = currPosRelativeToObs.x;
        float currYRelativeToObs = currPosRelativeToObs.y;

        // Update hero.vx and charSpeedY based on its current angle
        hero->vx = -(state->currOrbit.direction) * settings->speed *
                   OmlMath_Sinf(currAngle);
        hero->vy = state->currOrbit.direction * settings->speed *
                   OmlMath_Cosf(currAngle);

        // Update hero.x and hero.y
        hero->x = state->currOrbit.centerX + currXRelativeToObs;
        hero->y = state->currOrbit.centerY + currYRelativeToObs;
    }

    // Handle not-orbiting specific logic
    if (!state->isOrbiting) {
        // Update x and y
        hero->y += hero->vy * timeSinceLastFrameSeconds;
        hero->x += hero->vx * timeSinceLastFrameSeconds;
        // It's game over if we are not orbiting and we go out of bounds
        if (Physics_CheckCircleOutOfBoundsX(
                hero->x, hero->rad, leftVisibleBound, rightVisibleBound)) {
            state->gameOver = true;
        }
    }

    // Check for collisions with obs
    int obsILo, obsIHi;
    ObsRangeForYs(worldGen, lowerVisibleBoundMeters, upperVisibleBoundMeters,
                  &obsILo, &obsIHi);
    for (int i = obsILo; i <= obsIHi; i++) {
        Obs currObs = ObsAt(i, worldGen);
        if (Physics_ApproximateCirclesColliding(hero->x, hero->y, currObs.x,
                                                currObs.y, hero->rad,
                                                currObs.rad)) {

            state->gameOver = true;
        }
    }

    // Update camera
    float worldHeight =
        Camera_ScreenToWorldMeasurement(state->camera, settings->height);
    state->camera->x =
        0.25 * hero->x; // Camera x should be 25% of hero x diff relative to 0
    state->camera->y =
        hero->y +
        (0.15 * worldHeight); // hero should be towards bottom of screen a bit
}
