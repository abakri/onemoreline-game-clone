#pragma once
#include "types.h"

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

GameState newGameState(Camera camera) {
    GameState state = {
        .gameOver = false,
        .spaceDown = false,
        .isOrbiting = false,
        .closestObsForOrbit = 0,
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

void AppendNewRandomObs(ObsSOA *obsSoa, float screenWidthToWorld,
                        float leftVisibleBound, float rightVisibleBound,
                        float fromY, GameSettings *settings) {
    float XToAdd[OBS_TO_ADD_AT_A_TIME];
    float YToAdd[OBS_TO_ADD_AT_A_TIME];
    float RadToAdd[OBS_TO_ADD_AT_A_TIME];
    float currY = fromY;

    for (int i = 0; i < OBS_TO_ADD_AT_A_TIME; i++) {
        float obsRad =
            OmlMath_RandRangeFloat(settings->minObsRadius, settings->maxObsRadius);
        float minObsX = leftVisibleBound + obsRad + 1;
        float maxObsX = rightVisibleBound - obsRad - 1;
        float randYInterval = NewRandomYInterval(settings);

        XToAdd[i] = OmlMath_RandRangeFloat(minObsX, maxObsX);
        YToAdd[i] = currY + randYInterval;
        currY += randYInterval;
        RadToAdd[i] = obsRad;
    }
    AddObsMany(obsSoa, OBS_TO_ADD_AT_A_TIME, XToAdd, YToAdd, RadToAdd);
}

void updateGame(GameSettings *settings, GameState *state, Hero *hero,
                ObsSOA *obsSoa, float dt, bool spaceKeyPressed, int time) {
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
    float upperVisibleBound = state->camera.y + (screenHeightToWorld / 2);
    float lowerVisibleBound = state->camera.y + (screenHeightToWorld / 2) * -1;

    // -- HANDLE TIMER EVENTS
    // Handle forward movement powerup
    if (settings->gameOptions.includeForwardMovementPowerup &&
        state->forwardStreakPowerupActive) {
        int currentTime = time;
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
                state->currOrbit.startAngle = OmlMath_Atan2f(dy, dx);
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
            -(state->currOrbit.direction) * settings->speed * OmlMath_Sinf(currAngle);
        hero->vy =
            state->currOrbit.direction * settings->speed * OmlMath_Cosf(currAngle);

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
                state->forwardProgressPowerupActiveTimerStart = time;
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
                obsSoa->yVals[obsSoa->size - 1] + NewRandomYInterval(settings);
            AppendNewRandomObs(obsSoa, screenWidthToWorld, leftVisibleBound,
                               rightVisibleBound, newStartingY, settings);
        }
    } else { // check 5 before
        int indexToCheck = obsSoa->size - 5;
        if (hero->y >= obsSoa->yVals[indexToCheck]) {
            float newStartingY =
                obsSoa->yVals[obsSoa->size - 1] + NewRandomYInterval(settings);
            AppendNewRandomObs(obsSoa, screenWidthToWorld, leftVisibleBound,
                               rightVisibleBound, newStartingY, settings);
        }
    }
}
