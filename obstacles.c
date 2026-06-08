#pragma once
#include "types.h"

#define OBS_TO_ADD_AT_A_TIME 50

typedef struct {
    int size;
    int capacity;
    float *xVals;   // array of x values
    float *yVals;   // array of y values
    float *radVals; // array of radius values
} ObsSOA;

void AddObsMany(ObsSOA *obsSoa, int numNewItems, float x[], float y[],
                float rad[]) {
    // Note that we are assuming that the x, y, and rad arrays have a length of
    // numNewItems

    // Increase capacity if necessary
    if (obsSoa->size + numNewItems >= obsSoa->capacity) {
        if (obsSoa->capacity == 0) {
            obsSoa->capacity = numNewItems;
        } else {
            obsSoa->capacity = (obsSoa->capacity + numNewItems) * 2;
        }

        obsSoa->xVals =
            realloc(obsSoa->xVals, sizeof(float) * obsSoa->capacity);
        obsSoa->yVals =
            realloc(obsSoa->yVals, sizeof(float) * obsSoa->capacity);
        obsSoa->radVals =
            realloc(obsSoa->radVals, sizeof(float) * obsSoa->capacity);

        if (!obsSoa->xVals || !obsSoa->yVals || !obsSoa->radVals) {
            SDL_Log("Failed to reallocate obs soa");
            exit(1);
        }
    }

    // Add the new values
    for (int i = 0; i < numNewItems; i++) {
        obsSoa->xVals[obsSoa->size] = x[i];
        obsSoa->yVals[obsSoa->size] = y[i];
        obsSoa->radVals[obsSoa->size] = rad[i];
        obsSoa->size += 1;
    }
}

// TODO: Remember we need to actually call this once we have multiple
// menus/games
void FreeObs(ObsSOA *obsSoa) {
    free(obsSoa->xVals);
    free(obsSoa->yVals);
    free(obsSoa->radVals);
    obsSoa->capacity = 0;
    obsSoa->size = 0;
}

ObsSOA NewObsSOA() {
    ObsSOA result = {.size = 0, .capacity = 0};

    return result;
}
