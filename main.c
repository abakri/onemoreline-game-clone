#include "raylib.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_INT32 2147483647

// References
// * One more line
// * Jetpack joyride
// * Crossy road
//
// Ideas
// * What if there are black holes, and if you get sucked into them, you start
// going backwards and the colors are inverted. Black holes have strong gravity
// and maybe some Schwarzschild radius?
//
// Planning
// angle relative to the obstacle
// [] Add barriers on left and ride
// [] Have the camera take some of the horizontal movement

typedef struct {
    float x;
    float y;
    float rad;
} Obs;

typedef struct {
    float x;
    float y;
    float vx;
    float vy;
    float rad;
} Hero;

Obs newObs(int x, int y, int rad) {
    Obs obs = {
        .x = x,
        .y = y,
        .rad = rad,
    };
    return obs;
}

int randIntBetween(int low, int high) { return (rand() % (high - low)) + low; }

int fps = 120;

int main() {
    SetTargetFPS(fps);
    InitWindow(500, 1000, "onemoreline");

    // TODO: width and height should be dynamic or locked
    int width = GetScreenWidth();
    int height = GetScreenHeight();
    float speed = 600.0f;

    // Settings
    int numObs = 20;
    Obs obs[numObs];
    int minObsRadius = 10;
    int maxObsRadius = 40;
    // For now, hardcode the y distance between each obstacle
    int obsYDiff = 400;
    int startingObsY = 0;

    // camera should make up this proportion of the left-right movement
    float horizontalCameraMovement = 0.15f;

    // State
    bool spaceDown = false;
    bool isOrbiting = false;
    float orbitRadius = 0.0f;
    int closestObsForOrbit = 0;
    float orbitTime = 0.0f;
    float orbitInitialAngle = 0.0f;
    int orbitDir = -1; // 1 is clockwise, -1 is counterclockwise
    float orbitYOffset = 0.0f;
    float orbitXOffset = 0.0f;

    bool gameOver = false;

    // Entities
    Hero hero = {
        .x = (float)width / 2,
        .y = (float)height * 0.65,
        .vx = 0,
        .vy = speed,
        .rad = 20,
    };

    // Generate obstacles
    for (int i = 0; i < 20; i++) {
        int obsRad = randIntBetween(minObsRadius, maxObsRadius);
        int minObsX = obsRad;
        int maxObsX = width - obsRad;
        int obsX = randIntBetween(minObsX, maxObsX);
        int obsY = startingObsY - i * obsYDiff;
        obs[i] = newObs(obsX, obsY, obsRad);
    }

    while (!WindowShouldClose() && !gameOver) {
        float dt = GetFrameTime();

        BeginDrawing();
        ClearBackground(BLACK);

        // The moment the spacebar is clicked
        if (!spaceDown && IsKeyDown(KEY_SPACE)) {
            spaceDown = true;
        }

        // Spacebar is let go
        if (spaceDown && !IsKeyDown(KEY_SPACE)) {
            spaceDown = 0;
            isOrbiting = false;
        }

        // If space is down, and we haven't started orbiting, do the necessary
        // processing to check if we should go into an orbiting state
        if (spaceDown && !isOrbiting) {
            // We need to record the distance from the closest obs
            // TODO: Only look at the currently visible obs for performance
            int closestDistSq = MAX_INT32;
            for (int i = 0; i < numObs; i++) {
                Obs currObs = obs[i];
                if (currObs.y > height || currObs.y < 0) {
                    continue;
                }
                int distSquared = (currObs.x - hero.x) * (currObs.x - hero.x) +
                                  (currObs.y - hero.y) * (currObs.y - hero.y);
                if (distSquared < closestDistSq) {
                    closestObsForOrbit = i;
                    closestDistSq = distSquared;
                }
            }
            // If there is something to orbit, handle
            if (closestDistSq != MAX_INT32) {
                Obs closestObs = obs[closestObsForOrbit];
                orbitRadius = sqrtf(closestDistSq);

                float dy = (float)hero.y - (float)closestObs.y;
                float dx = (float)hero.x - (float)closestObs.x;
                float cross = dx * -hero.vy - dy * hero.vx; // cross product
                float speedSq = hero.vx * hero.vx + hero.vy * hero.vy;
                float collideDistSq = (cross * cross) / speedSq;
                float combinedRad = hero.rad + closestObs.rad;

                // We should only go into orbit if the hero is not on a
                // trajectory to collide with this obj
                if (collideDistSq >= combinedRad * combinedRad) {
                    // We should now be in orbit
                    isOrbiting = true;

                    // Calculate the orbit initial angle
                    orbitInitialAngle = atan2f(dy, dx);

                    // This calculates how we can get a "swingin on a pole
                    // effect" For example, if character is going upwards and
                    // towards the bottom left of an obstacle, then on spacebar,
                    // it will orbit clockwise
                    if (cross >= 0) {
                        orbitDir = 1;
                    } else {
                        orbitDir = -1;
                    }

                    // set orbit frames
                    orbitTime = 0.0f;
                }
            }
        }

        // Handle orbiting specific logic
        if (isOrbiting) {
            Obs closestObs = obs[closestObsForOrbit];
            // update hero.x and hero.y
            // we want to increase the arc by speed, so we should
            // increase the angle by speed / orbit radius every frame
            float angularVelocity = speed / orbitRadius;
            float prevAngle = orbitInitialAngle +
                              orbitDir * angularVelocity * (orbitTime - dt);
            float currAngle =
                orbitInitialAngle + orbitDir * angularVelocity * orbitTime;

            // These are the coordinates relative to the closest object
            float relX = orbitRadius * cosf(currAngle);
            float relY = orbitRadius * sinf(currAngle);

            // Update hero.x and hero.y (coordinates relative to the screen)
            hero.x = closestObs.x + relX;
            hero.y = closestObs.y + relY;
            
            if (orbitTime - dt > 0) {
                // record how much the screen should move to account for the y
                // difference
                float prevRelY = orbitRadius * sinf(prevAngle);
                orbitYOffset = prevRelY - relY;

                float prevRelX = orbitRadius * cosf(prevAngle);
                orbitXOffset = prevRelX - relX;
                hero.x -= (1.0f - horizontalCameraMovement) * orbitXOffset;
            }

            // Update hero.vx and charSpeecY based on its current angle
            hero.vx = -orbitDir * speed * sinf(currAngle);
            hero.vy = -1 * orbitDir * speed * cosf(currAngle);

            // Now increment orbitFrames
            orbitTime += dt;

            // Draw radius of orbit
            DrawLine(hero.x, hero.y, closestObs.x, closestObs.y, RED);
        }

        // Handle not-orbiting specific logic
        if (!isOrbiting) {
            // It's game over if we are not orbiting and we go out of bounds
            if (hero.x - hero.rad >= width || hero.x + hero.rad <= 0) {
                gameOver = true;
            }

            // Subtract the proportion of the hero x to create horizontal camera movement
            hero.x += (1.0f - horizontalCameraMovement) * (hero.vx * dt);
        }

        // Draw the scene

        // Draw character
        DrawCircle(hero.x, hero.y, hero.rad, WHITE);

        // Draw the obstacles
        for (int i = 0; i < numObs; i++) {
            Obs currObs = obs[i];

            // While we are looping, check that char is colliding with obs
            float distX = (float)hero.x - (float)currObs.x;
            float distY = (float)hero.y - (float)currObs.y;

            float distSquared = (distX * distX) + (distY * distY);
            float radiiSumSquared =
                (hero.rad + currObs.rad) * (hero.rad + currObs.rad);
            if (distSquared <= radiiSumSquared) {
                gameOver = true;
            }

            DrawCircle(currObs.x, currObs.y, currObs.rad, WHITE);
            // If orbiting, move stuff downwards
            if (!isOrbiting) {
                obs[i].y += hero.vy * dt;
                obs[i].x -= horizontalCameraMovement * (hero.vx * dt); 
            } else { // otherwise, move the "camera" y based on the orbit
                     // movement
                obs[i].y += orbitYOffset;
                obs[i].x += horizontalCameraMovement * orbitXOffset;
            }
        }

        EndDrawing();
    }
    return 0;
}
