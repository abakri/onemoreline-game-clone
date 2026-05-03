#pragma once

// This is just shared types. It's kind of a mess, but necessary to get the LSP
// happy. I think there has to be a better way to do this, but until then...

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

typedef struct {
    float x; // world x position of camera
    float y; // world y position of camera
    float pixelsPerMeter;
} Camera;

typedef struct {
    float left;
    float right;
    float top;
    float bottom;
} RectBounds;

// All of this is necessary to make clangd happy with unity builds
// https://www.frogtoss.com/labs/clangd-with-unity-builds.html
//
// Basically any non-root file that uses any of the code above should be
// included here. And each of them should also include this file at the top.
#include "camera.c"
#include "physics.c"
