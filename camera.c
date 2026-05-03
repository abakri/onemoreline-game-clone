#pragma once
#include "types.h"

Point Camera_WorldPositionToScreen(Camera camera, float worldX, float worldY,
                                   float screenWidth, float screenHeight) {
    // Camera center is at the middle of the screen, meaning left is negative X
    // and right is positive X, and same with Y values.
    Point p = {
        .x = (worldX - camera.x) * camera.pixelsPerMeter + screenWidth / 2,
        // Flip the y axis so positive is now going upwards.
        .y = -(worldY - camera.y) * camera.pixelsPerMeter + screenHeight / 2,
    };
    return p;
}

float Camera_WorldMeasurementToScreen(Camera camera, float worldValue) {
    return worldValue * camera.pixelsPerMeter;
}

float Camera_ScreenToWorldMeasurement(Camera camera, float worldValue) {
    return worldValue / camera.pixelsPerMeter;
}
