#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_INT32 2147483647

// Planning
// TODO: You can only latch onto an obstacle if you have a clear path for orbit
// based on your trajectory.

typedef struct {
    int x;
    int y;
    int rad;
} Obs;

Obs newObs(int x, int y, int rad) {
    Obs obs = {x, y, rad};
    return obs;
}

int randIntBetween(int low, int high) { return (rand() % (high - low)) + low; }

int main() {
    SetTargetFPS(120);
    InitWindow(500, 1000, "onemoreline");

    int width = GetScreenWidth();
    int height = GetScreenHeight();
    int speed = 5;

    int charRadius = 20;
    int charX = width / 2;
    int charY = height - 2 * 100;
    int charSpeedX = 0;
    int charSpeedY = speed;

    // Generate obstacles
    int numObs = 20;
    Obs obs[numObs];
    int minObsRadius = 10;
    int maxObsRadius = 40;
    // For now, hardcode the y distance between each obstacle
    int obsYDiff = 400;
    int startingObsY = height / 2;

    int spaceDown = 0;
    int shouldOrbit = 0;
    float orbitRadius = 0;
    int closestObsForOrbit = 0;
    int orbitFrames = 0;
    float orbitInitialAngle = 0;
    int orbitDir = -1; // 1 is clockwise, -1 is counterclockwise

    int gameOver = 0;

    for (int i = 0; i < 20; i++) {
        int obsRad = randIntBetween(minObsRadius, maxObsRadius);
        int minObsX = obsRad;
        int maxObsX = width - obsRad;
        int obsX = randIntBetween(minObsX, maxObsX);
        int obsY = startingObsY - i * obsYDiff;
        obs[i] = newObs(obsX, obsY, obsRad);
    }

    while (!WindowShouldClose() && !gameOver) {
        BeginDrawing();
        ClearBackground(BLACK);

        // This happens if spacebar is first clicked
        if (!spaceDown && IsKeyDown(KEY_SPACE)) {
            spaceDown = 1;

            // We need to record the distance from the closest obs
            // TODO: Only look at the currently visible obs
            int closestDistSq = MAX_INT32;
            for (int i = 0; i < numObs; i++) {
                Obs currObs = obs[i];
                if (currObs.y > height || currObs.y < 0) {
                    continue;
                }
                int distSquared = (currObs.x - charX) * (currObs.x - charX) +
                                  (currObs.y - charY) * (currObs.y - charY);
                if (distSquared < closestDistSq) {
                    closestObsForOrbit = i;
                    closestDistSq = distSquared;
                }
            }
            // If there is something to orbit, then set shouldOrbit to true
            if (closestDistSq != MAX_INT32) {
                Obs closestObs = obs[closestObsForOrbit];
                shouldOrbit = 1;
                orbitRadius = sqrtf(closestDistSq);

                // set initial angle
                float dy = (float)charY - (float)closestObs.y;
                float dx = (float)charX - (float)closestObs.x;
                orbitInitialAngle = atan2f(dy, dx);

                // This calculates how we can get a "swingin on a pole effect"
                // For example, if character is going upwards and towards the
                // bottom left of an obstacle, then on spacebar, it will orbit
                // clockwise
                float cross = dx * -(float)charSpeedY - dy * (float)charSpeedX;
                if (cross >= 0) {
                    orbitDir = 1;
                } else {
                    orbitDir = -1;
                }

                // set orbit frames
                orbitFrames = 0;
            }
        }

        // Spacebar is let go
        if (spaceDown && !IsKeyDown(KEY_SPACE)) {
            spaceDown = 0;
            shouldOrbit = false;
            // we should adjust the char x and y speed based on the current
            // angle
        }

        // Update charX and char Y if should orbit
        if (shouldOrbit) {
            Obs closestObs = obs[closestObsForOrbit];
            // update charX and charY
            // we want to increase the arc by speed, so we should
            // increase the angle by speed / orbit radius every frame
            float currAngle =
                orbitInitialAngle +
                orbitDir * (float)orbitFrames * (float)speed / orbitRadius;

            // These are the coordinates relative to the closest object
            float relX = orbitRadius * cosf(currAngle);
            float relY = orbitRadius * sinf(currAngle);

            // Update charX and charY (coordinates relative to the screen)
            charX = closestObs.x + relX;
            charY = closestObs.y + relY;

            // Update charSpeedX and charSpeecY based on its current angle
            charSpeedX = -orbitDir * speed * sinf(currAngle);
            charSpeedY = -1 * orbitDir * speed * cosf(currAngle);

            // Now increment orbitFrames
            orbitFrames++;

            // Draw radius of orbit
            DrawLine(charX, charY, closestObs.x, closestObs.y, RED);
        }

        // Draw character
        DrawCircle(charX, charY, charRadius, WHITE);

        // Draw the obstacles
        for (int i = 0; i < numObs; i++) {
            Obs currObs = obs[i];

            // While we are looping, check that char is colliding with obs
            float distX = (float)charX - (float)currObs.x;
            float distY = (float)charY - (float)currObs.y;
            float dist = sqrt((distX * distX) + (distY * distY));
            float radiiSum = charRadius + currObs.rad;
            if (dist <= radiiSum) {
                gameOver = 1;
            }

            DrawCircle(currObs.x, currObs.y, currObs.rad, WHITE);
            // Only move stuff downwards IF we are not orbiting
            if (!shouldOrbit) {
                obs[i].y += charSpeedY;
            }
        }

        // If we are not orbiting, then charX updates linearly
        if (!shouldOrbit) {

            // It's game over if we are not orbiting and we go out of bounds
            if (charX - charRadius >= width || charX + charRadius <= 0) {
                gameOver = 1;
            }

            charX += charSpeedX;
        }

        // Draw obstacles
        EndDrawing();
    }
    return 0;
}
