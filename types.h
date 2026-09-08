#pragma once

// This is just shared types. It's kind of a mess, but necessary to get the LSP
// happy. I think there has to be a better way to do this, but until then...

#include <stdbool.h>
#include <stdint.h>
#ifdef OML_SDL
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_image/SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#endif

#ifdef OML_WASM
#define WASM_EXPORT(name) __attribute__((export_name(name)))
#define WASM_IMPORT(mod, name) __attribute__((import_module(mod), import_name(name)))
#endif

#include "oml_math.c"

#define MAX_INT32 2147483647
#define DEBUG 0
#define OBS_TO_ADD_AT_A_TIME 100
#define MAX_DT 0.05f
#define OBS_CAP 2000

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
#include "draw.c"
#include "obstacles.c"
#include "physics.c"
