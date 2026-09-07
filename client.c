/* The purpose of this file is to prepare the
 * data required by clients to properly render
 * the game
 */

#include "game.c"

#define STATE_BUFFER_SIZE 18
#define OBS_SOA_BUFFER_SIZE 20

const int HERO_X_IDX = 0;
const int HERO_Y_IDX = 1;
const int HERO_RAD_IDX = 2;
const int CAMERA_X_IDX = 3;
const int CAMERA_Y_IDX = 4;
const int PIXELS_PER_METER_IDX = 5;
const int CLOSEST_OBS_X_IDX = 6;
const int CLOSEST_OBS_Y_IDX = 7;
const int IS_ORBITING_IDX = 8;
const int SPACE_KEY_DOWN_IDX = 9;
const int GAME_OVER_IDX = 10;
const int ORBITING_X_IDX = 11;
const int ORBITING_Y_IDX = 12;
const int LEFT_BOUNDARY_X_IDX = 13;
const int RIGHT_BOUNDARY_X_IDX = 14;
const int SCORE_IDX = 15;
const int SCREEN_WIDTH_IDX = 16;
const int SCREEN_HEIGHT_IDX = 17;

typedef struct {
    float STATE_BUFFER[STATE_BUFFER_SIZE];
    float OBS_X_BUFFER[OBS_SOA_BUFFER_SIZE];
    float OBS_Y_BUFFER[OBS_SOA_BUFFER_SIZE];
    float OBS_RAD_BUFFER[OBS_SOA_BUFFER_SIZE];
} ClientData;

void updateClientData(Game *game, ClientData *clientData) {
    // Populate screen width and height
    clientData->STATE_BUFFER[SCREEN_WIDTH_IDX] = game->width;
    clientData->STATE_BUFFER[SCREEN_HEIGHT_IDX] = game->height;

    // Convert hero data to pixels
    Point heroPositionPx = Camera_WorldPositionToScreen(
        &game->camera, game->hero.x, game->hero.y, game->width, game->height);
    clientData->STATE_BUFFER[HERO_X_IDX] = heroPositionPx.x;
    clientData->STATE_BUFFER[HERO_Y_IDX] = heroPositionPx.y;
    clientData->STATE_BUFFER[HERO_RAD_IDX] =
        Camera_WorldMeasurementToScreen(&game->camera, game->hero.rad);

    clientData->STATE_BUFFER[CAMERA_X_IDX] = game->camera.x;
    clientData->STATE_BUFFER[CAMERA_Y_IDX] = game->camera.y;
    clientData->STATE_BUFFER[PIXELS_PER_METER_IDX] =
        game->camera.pixelsPerMeter;

    // Add position of closest obs to hero in pixels
    Obs closestObsToHero =
        GetClosestObsToPoint(game->hero.x, game->hero.y, &game->worldGen);
    Point closestObsPositionPx = Camera_WorldPositionToScreen(
        &game->camera, closestObsToHero.x, closestObsToHero.y, game->width,
        game->height);
    clientData->STATE_BUFFER[CLOSEST_OBS_X_IDX] = closestObsPositionPx.x;
    clientData->STATE_BUFFER[CLOSEST_OBS_Y_IDX] = closestObsPositionPx.y;

    // TODO: Likely we should just have a flag for "ORBIT_INDICATOR_ON"
    // and "ORBIT_INDICATOR_START_X", "ORBIT_INDICATOR_START_Y",
    // "ORBIT_INDICATOR_END_X", "ORBIT_INDICATOR_END_Y"
    clientData->STATE_BUFFER[IS_ORBITING_IDX] = game->isOrbiting;

    // TODO: We won't need this if we implement the above
    Point orbitPositionPx = Camera_WorldPositionToScreen(
        &game->camera, game->currOrbit.centerX, game->currOrbit.centerY,
        game->width, game->height);
    clientData->STATE_BUFFER[ORBITING_X_IDX] = orbitPositionPx.x;
    clientData->STATE_BUFFER[ORBITING_Y_IDX] = orbitPositionPx.y;

    clientData->STATE_BUFFER[SPACE_KEY_DOWN_IDX] = game->spaceDown;
    clientData->STATE_BUFFER[GAME_OVER_IDX] = game->gameOver;

    // Add pixel boundary positions for the left and right
    float screenWidthMeters =
        Camera_ScreenToWorldMeasurement(&game->camera, game->width);

    float leftBoundMeters = (screenWidthMeters / 2) * -1;
    float rightBoundMeters = (screenWidthMeters / 2);
    float leftScreenPixelsX =
        Camera_WorldPositionToScreen(&game->camera, leftBoundMeters,
                                     leftBoundMeters, game->width, game->height)
            .x -
        1;
    float rightScreenPixelsX = Camera_WorldPositionToScreen(
                                   &game->camera, rightBoundMeters,
                                   rightBoundMeters, game->width, game->height)
                                   .x;
    clientData->STATE_BUFFER[LEFT_BOUNDARY_X_IDX] = leftScreenPixelsX;
    clientData->STATE_BUFFER[RIGHT_BOUNDARY_X_IDX] = rightScreenPixelsX;

    // Add score which is just the hero's y in meters
    clientData->STATE_BUFFER[SCORE_IDX] = game->hero.y;

    // ----- UPDATE THE OBS BUFFERS
    float screenHeightToWorld =
        Camera_ScreenToWorldMeasurement(&game->camera, game->height);
    float upperVisibleBoundMeters = game->camera.y + (screenHeightToWorld / 2);
    float lowerVisibleBoundMeters =
        game->camera.y + (screenHeightToWorld / 2) * -1;

    int obsILo, obsIHi;
    ObsRangeForYs(&game->worldGen, lowerVisibleBoundMeters,
                  upperVisibleBoundMeters, &obsILo, &obsIHi);

    for (int i = 0; i < OBS_SOA_BUFFER_SIZE; i++) {
        int seq = i + obsILo;

        // If we have exceeded the obs on screen, just set everything to 0
        if (seq > obsIHi) {
            clientData->OBS_X_BUFFER[i] = 0;
            clientData->OBS_Y_BUFFER[i] = 0;
            clientData->OBS_RAD_BUFFER[i] = 0;
            continue;
        }

        Obs obs = ObsAt(seq, &game->worldGen);
        Point obsPositionPx = Camera_WorldPositionToScreen(
            &game->camera, obs.x, obs.y, game->width, game->height);
        clientData->OBS_X_BUFFER[i] = obsPositionPx.x;
        clientData->OBS_Y_BUFFER[i] = obsPositionPx.y;
        clientData->OBS_RAD_BUFFER[i] =
            Camera_WorldMeasurementToScreen(&game->camera, obs.rad);
    }
}
