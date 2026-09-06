#pragma once
#include "types.h"

#define OBS_SOA_SIZE 20

typedef struct {
    float xVals[OBS_SOA_SIZE];   // array of x values
    float yVals[OBS_SOA_SIZE];   // array of y values
    float radVals[OBS_SOA_SIZE]; // array of radius values
} ObsSOA;

ObsSOA NewObsSOA(void) {
    ObsSOA result = {0};
    return result;
}
