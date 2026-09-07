#include "client.c"
#include "types.h"

// Internal global for game
static Game game;

// Struct that holds the memory buffers to be used
static ClientData clientData;

// FUNCTIONS EXPORTED TO JS
WASM_EXPORT("init")
int init(uint32_t seed, int windowWidth, int windowHeight) {
    createGame(&game, windowWidth, windowHeight, seed);
    updateClientData(&game, &clientData);
    return 1;
}

WASM_EXPORT("loop")
void loop(float timeSinceLastFrameSeconds, int timeSinceStartMs,
          int spaceKeyPressed) {
    updateGame(&game, timeSinceLastFrameSeconds, spaceKeyPressed,
               timeSinceStartMs);

    updateClientData(&game, &clientData);
}

// State buffer exports
WASM_EXPORT("stateBufferPointer")
float (*stateBufferPointer(void))[STATE_BUFFER_SIZE] {
    return &clientData.STATE_BUFFER;
}

WASM_EXPORT("stateBufferSize")
int stateBufferSize(void) {
    return STATE_BUFFER_SIZE;
}

// Obs buffer exports
WASM_EXPORT("obsXBufferPointer")
float (*obsXBufferPointer(void))[OBS_SOA_BUFFER_SIZE] {
    return &clientData.OBS_X_BUFFER;
}

WASM_EXPORT("obsYBufferPointer")
float (*obsYBufferPointer(void))[OBS_SOA_BUFFER_SIZE] {
    return &clientData.OBS_Y_BUFFER;
}

WASM_EXPORT("obsRadBufferPointer")
float (*obsRadBufferPointer(void))[OBS_SOA_BUFFER_SIZE] {
    return &clientData.OBS_RAD_BUFFER;
}

WASM_EXPORT("obsBufferSize")
int obsBufferSize(void) {
    return OBS_SOA_BUFFER_SIZE;
}

