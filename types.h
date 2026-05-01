#pragma once

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_INT32 2147483647
#define DEBUG 0

typedef struct {
    float x;
    float y;
} Point;

// All of this is necessary to make clangd happy with unity builds
// https://www.frogtoss.com/labs/clangd-with-unity-builds.html
//
// Basically any non-root file that uses any of the code above should be
// included here. And each of them should also include this file at the top.
#include "physics.c"
