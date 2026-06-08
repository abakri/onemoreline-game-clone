#pragma once
#include "types.h"

typedef struct {
    float startAngle;
    float radius;
    int startTime; // Makes it easy to calculate a delta. Should be something
    // like ms since program start.
    int direction; // 1 is clockwise, -1 is counterclockwise
} OrbitData;

OrbitData newOrbitData() {
    OrbitData o = {
        .startAngle = 0.f,
        .radius = 0.f,
        .startTime = 0,
        .direction = 1,
    };
    return o;
}

int Physics_ApproximateCirclesColliding(float x1, float y1, float x2, float y2,
                                        float rad1, float rad2) {
    float distX = x1 - x2;
    float distY = y1 - y2;
    float distSquared = (distX * distX) + (distY * distY);
    float radiiSumSquared = (rad1 + rad2) * (rad1 + rad2);
    return distSquared <= radiiSumSquared;
};

int Physics_CheckCircleOutOfBoundsLeft(float centerX, float radius,
                                       float leftBound) {
    return centerX + radius < leftBound;
}

int Physics_CheckCircleOutOfBoundsRight(float centerX, float radius,
                                        float rightBound) {
    return centerX - radius > rightBound;
}

int Physics_CheckCircleOutOfBoundsX(float centerX, float radius,
                                    float leftBound, float rightBound) {
    return centerX + radius < leftBound || centerX - radius > rightBound;
}

int Physics_CheckCircleWithinRect(float centerX, float centerY, float radius,
                                  float leftBound, float rightBound,
                                  float lowerBound, float upperBound) {
    return centerX - radius > leftBound && centerX + radius < rightBound &&
           centerY + radius < upperBound && centerY - radius > lowerBound;
}

// --- ORBITING CALCULATIONS ---
float Orbit_CalculateAngle(float speed, float orbitRadius,
                           float initialOrbitAngle, float orbitDirection,
                           float orbitTime) {
    float angularVelocity = speed / orbitRadius;
    return initialOrbitAngle + orbitDirection * angularVelocity * orbitTime;
}

Point Orbit_CalculatePositionRelativeToTarget(float currAngle,
                                              float orbitRadius) {
    // These are the coordinates relative to the closest object
    Point p = {.x = orbitRadius * cosf(currAngle),
               .y = orbitRadius * sinf(currAngle)};

    return p;
}
