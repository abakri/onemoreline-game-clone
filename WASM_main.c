#include "types.h"

// #include "camera.c"
#include "game.c"
// #include "obstacles.c"
// #include "physics.c"

#define STATE_BUFFER_SIZE 13
#define OBS_SOA_BUFFER_SIZE 20

// Internal Variables
static WorldGen worldGen;
static GameSettings gameSettings;
static Camera camera;
static GameState gameState;
static Hero hero;

// State Buffer for JS
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

float STATE_BUFFER[STATE_BUFFER_SIZE];

// Obs SOA buffers
float OBS_X_BUFFER[OBS_SOA_BUFFER_SIZE];
float OBS_Y_BUFFER[OBS_SOA_BUFFER_SIZE];
float OBS_RAD_BUFFER[OBS_SOA_BUFFER_SIZE];

// HELPER FUNCTIONS
void updateStateBuffer(void) {
    Obs closestObsToHero = GetClosestObsToPoint(hero.x, hero.y, &worldGen);
    STATE_BUFFER[HERO_X_IDX] = hero.x;
    STATE_BUFFER[HERO_Y_IDX] = hero.y;
    STATE_BUFFER[HERO_RAD_IDX] = hero.rad;
    STATE_BUFFER[CAMERA_X_IDX] = camera.x;
    STATE_BUFFER[CAMERA_Y_IDX] = camera.y;
    STATE_BUFFER[PIXELS_PER_METER_IDX] = camera.pixelsPerMeter;
    STATE_BUFFER[CLOSEST_OBS_X_IDX] = closestObsToHero.x;
    STATE_BUFFER[CLOSEST_OBS_Y_IDX] = closestObsToHero.y;
    STATE_BUFFER[IS_ORBITING_IDX] = gameState.isOrbiting;
    STATE_BUFFER[SPACE_KEY_DOWN_IDX] = gameState.spaceDown;
    STATE_BUFFER[GAME_OVER_IDX] = gameState.gameOver;
    STATE_BUFFER[ORBITING_X_IDX] = gameState.currOrbit.centerX;
    STATE_BUFFER[ORBITING_Y_IDX] = gameState.currOrbit.centerY;
}

void updateObsBuffers(void) {
    float screenHeightToWorld =
        Camera_ScreenToWorldMeasurement(&camera, gameSettings.height);
    float upperVisibleBoundMeters = camera.y + (screenHeightToWorld / 2);
    float lowerVisibleBoundMeters = camera.y + (screenHeightToWorld / 2) * -1;

    int obsILo, obsIHi;
    ObsRangeForYs(&worldGen, lowerVisibleBoundMeters, upperVisibleBoundMeters,
                  &obsILo, &obsIHi);

    for (int i = 0; i < OBS_SOA_BUFFER_SIZE; i++) {
        int seq = i + obsILo;

        // If we have exceeded the obs on screen, just set everything to 0
        if (seq > obsIHi) {
            OBS_X_BUFFER[i] = 0;
            OBS_Y_BUFFER[i] = 0;
            OBS_RAD_BUFFER[i] = 0;
            continue;
        }

        Obs obs = ObsAt(seq, &worldGen);
        OBS_X_BUFFER[i] = obs.x;
        OBS_Y_BUFFER[i] = obs.y;
        OBS_RAD_BUFFER[i] = obs.rad;
    }
}

// FUNCTIONS EXPORTED TO JS
WASM_EXPORT("init")
int init(uint32_t seed, int windowWidth, int windowHeight) {
    worldGen = newWorldGen(seed);
    gameSettings = newGameSettings(windowWidth, windowHeight);
    Camera c = {
        .x = 0,
        .y = 0,
        .pixelsPerMeter = PIXELS_PER_METER,
    };
    camera = c;
    gameState = newGameState(&camera);
    Hero h = {
        .x = 0,
        .y = 0,
        .vx = 0,
        .vy = gameSettings.speed,
        .rad = 0.3,
    };
    hero = h;

    updateStateBuffer();
    updateObsBuffers();
    return 1;
}

WASM_EXPORT("loop")
void loop(float timeSinceLastFrameSeconds, int timeSinceStartMs,
          int spaceKeyPressed) {
    updateGame(&worldGen, &gameSettings, &gameState, &hero,
               timeSinceLastFrameSeconds, spaceKeyPressed, timeSinceStartMs);

    updateStateBuffer();
    updateObsBuffers();
}

// TODO: Redesign this, whats a better way to get a point easily?
WASM_EXPORT("metersXToPixels")
float metersXToPixels(float x, float winWidth, float winHeight) {
    Point pos =
        Camera_WorldPositionToScreen(&camera, x, x, winWidth, winHeight);
    return pos.x;
}

WASM_EXPORT("metersYToPixels")
float metersYToPixels(float y, float winWidth, float winHeight) {
    Point pos =
        Camera_WorldPositionToScreen(&camera, y, y, winWidth, winHeight);
    return pos.y;
}

WASM_EXPORT("pixelsToMeters")
float pixelsToMeters(float val) {
    return Camera_ScreenToWorldMeasurement(&camera, val);
}

WASM_EXPORT("metersToPixels")
float metersToPixels(float val) {
    return Camera_WorldMeasurementToScreen(&camera, val);
}
