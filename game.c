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
    bool forwardStreakPowerupActive;

    // This is used to track when hero is in forward streak powerup
    // and has already collided into an obstacle triggering active powerup.
    bool forwardStreakPowerupCollisionActivated;

    int forwardProgressPowerupActiveTimerStart;
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

GameState newGameState(Camera camera) {
    GameState state = {
        .gameOver = false,
        .spaceDown = false,
        .isOrbiting = false,
        .forwardProgressTravelled = 0.0f,
        .forwardProgressStreak = 0.0f,
        .forwardStreakPowerupActive = false,
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

float NewRandomYInterval(GameSettings *settings) {
    return OmlMath_RandRangeFloat(settings->minObsYInterval,
                                  settings->maxObsYInterval);
};

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

void GetObsStartingAtPosition(Obs *obsArray, float y, int numObs, WorldGen *w) {
    int startingI =
        (int)((y - w->obsStartingY - w->obsYJitter) / w->obsAverageYDiff);
    // If the location is before the obs starting y, then we know 0 is the first
    // obs.
    if (y <= w->obsStartingY) {
        startingI = 0;
    }
    for (int i = 0; i < numObs; i++) {
        obsArray[i] = ObsAt(i + startingI, w);
    }
}

void GetObsSoaStartingAtPosition(ObsSOA *obsSOA, float y, int numObs,
                                 WorldGen *w) {
    int startingI =
        (int)((y - w->obsStartingY - w->obsYJitter) / w->obsAverageYDiff);

    // If the location is before the obs starting y, then we know 0 is the first
    // obs.
    if (y <= w->obsStartingY) {
        startingI = 0;
    }
    for (int i = 0; i < numObs; i++) {
        Obs obs = ObsAt(startingI + i, w);
        obsSOA->xVals[i] = obs.x;
        obsSOA->yVals[i] = obs.y;
        obsSOA->radVals[i] = obs.rad;
    }
}

void updateGame(WorldGen *worldGen, GameSettings *settings, GameState *state,
                Hero *hero, float dt, bool spaceKeyPressed, int time) {
    // Clamp dt to our max dt. If dt comes back super high, then we can
    // experience tunneling.
    if (dt > MAX_DT) {
        dt = MAX_DT;
    }

    float screenWidthToWorld =
        Camera_ScreenToWorldMeasurement(state->camera, settings->width);
    float leftVisibleBound = (screenWidthToWorld / 2) * -1;
    float rightVisibleBound = (screenWidthToWorld / 2);
    float screenHeightToWorld =
        Camera_ScreenToWorldMeasurement(state->camera, settings->height);
    float upperVisibleBoundMeters = state->camera.y + (screenHeightToWorld / 2);
    float lowerVisibleBoundMeters =
        state->camera.y + (screenHeightToWorld / 2) * -1;

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
    ObsSOA obsSoa = NewObsSOA();
    GetObsSoaStartingAtPosition(&obsSoa, lowerVisibleBoundMeters, OBS_SOA_SIZE,
                                worldGen);

    // If space is down, and we haven't started orbiting, do the necessary
    // processing to check if we should go into an orbiting state
    if (state->spaceDown && !state->isOrbiting) {
        Obs closestObs = GetClosestObsToPoint(hero->x, hero->y, worldGen);

        // Consider an orbiting candidate found only if it is visible on screen
        int found = lowerVisibleBoundMeters <= closestObs.y &&
                    closestObs.y <= upperVisibleBoundMeters;

        // If there is something to orbit, handle
        if (found) {
            state->currOrbit.radius = sqrtf(GetDistSquaredFrom(
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
                state->currOrbit.startTime = time;
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
        hero->y += hero->vy * dt;
        hero->x += hero->vx * dt;
        // It's game over if we are not orbiting and we go out of bounds
        if (Physics_CheckCircleOutOfBoundsX(
                hero->x, hero->rad, leftVisibleBound, rightVisibleBound)) {
            state->gameOver = true;
        }
    }

    // Update obs locations
    for (int i = 0; i < OBS_SOA_SIZE; i++) {
        float currObsX = obsSoa.xVals[i];
        float currObsY = obsSoa.yVals[i];
        float currObsRad = obsSoa.radVals[i];
        if (Physics_ApproximateCirclesColliding(
                hero->x, hero->y, currObsX, currObsY, hero->rad, currObsRad)) {

            state->gameOver = true;
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
}
