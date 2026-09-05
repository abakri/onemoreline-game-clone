#include "types.h"

#include "camera.c"
#include "game.c"
#include "obstacles.c"
#include "physics.c"

// TODO: A lot of the drawing here will likely be replaced with game assets
void renderGame(SDL_Renderer *renderer, WorldGen *worldGen, GameState *state,
                GameSettings *settings, Hero *hero, TTF_Text *scoreTextObj) {
    // Draw boundaries on the left and right
    float screenWidthToWorld =
        Camera_ScreenToWorldMeasurement(state->camera, settings->width);
    float screenHeightToWorld =
        Camera_ScreenToWorldMeasurement(state->camera, settings->height);

    float leftBound = (screenWidthToWorld / 2) * -1;
    float rightBound = (screenWidthToWorld / 2);
    float upperBound = state->camera.y + (screenHeightToWorld / 2);
    float lowerBound = state->camera.y + (screenHeightToWorld / 2) * -1;

    float leftScreenX =
        Camera_WorldPositionToScreen(state->camera, leftBound, 0,
                                     settings->width, settings->height)
            .x -
        1;
    float rightScreenX =
        Camera_WorldPositionToScreen(state->camera, rightBound, 0,
                                     settings->width, settings->height)
            .x;
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // WHITE
    // Left boundary
    SDL_RenderLine(renderer, leftScreenX,
                   0, // top of screen
                   leftScreenX,
                   settings->height // bottom of screen
    );

    // Right boundary
    SDL_RenderLine(renderer, rightScreenX,
                   0, // top of screen
                   rightScreenX,
                   settings->height // bottom of screen
    );

    // TODO: Extract this out when we support black holes
    // bool blackhole = false;

    // Draw character
    Point heroScreenPos = Camera_WorldPositionToScreen(
        state->camera, hero->x, hero->y, settings->width, settings->height);
    float heroScreenRad =
        Camera_WorldMeasurementToScreen(state->camera, hero->rad);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // WHITE
    Draw_DrawFilledCircle(renderer, heroScreenPos.x, heroScreenPos.y,
                          heroScreenRad);

    ObsSOA obsSoa = NewObsSOA();
    float lowerVisibleBound = state->camera.y + (screenHeightToWorld / 2) * -1;
    GetObsSoaStartingAtPosition(&obsSoa, lowerVisibleBound, OBS_SOA_SIZE,
                                worldGen);

    // Draw line to signal orbit
    Obs closestObs = GetClosestObsToPoint(hero->x, hero->y, worldGen);
    Point closestObsScreenPos =
        Camera_WorldPositionToScreen(state->camera, closestObs.x, closestObs.y,
                                     settings->width, settings->height);
    // Use this to always see the closest obs
    // SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255); // BLUE
    // SDL_RenderLine(renderer, heroScreenPos.x, heroScreenPos.y,
    //                closestObsScreenPos.x, closestObsScreenPos.y);

    if (state->spaceDown && !state->isOrbiting) {
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // RED
        SDL_RenderLine(renderer, heroScreenPos.x, heroScreenPos.y,
                       closestObsScreenPos.x, closestObsScreenPos.y);
    } else if (state->isOrbiting) {
        Point orbitCenterScreenPos = Camera_WorldPositionToScreen(
            state->camera, state->currOrbit.centerX, state->currOrbit.centerY,
            settings->width, settings->height);
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // RED
        SDL_RenderLine(renderer, heroScreenPos.x, heroScreenPos.y,
                       orbitCenterScreenPos.x, orbitCenterScreenPos.y);
    }

    // Draw the obstacles
    // TODO: Only draw obstacles that are on the screen
    for (int i = 0; i < OBS_SOA_SIZE; i++) {
        float currObsX = obsSoa.xVals[i];
        float currObsY = obsSoa.yVals[i];
        float currObsRad = obsSoa.radVals[i];
        Point currObsScreenPos =
            Camera_WorldPositionToScreen(state->camera, currObsX, currObsY,
                                         settings->width, settings->height);
        float currObsScreenRad =
            Camera_WorldMeasurementToScreen(state->camera, currObsRad);
        // Only render if the obs should be on the screen
        if (Physics_CheckCircleWithinRect(currObsX, currObsY, currObsRad,
                                          leftBound, rightBound, lowerBound,
                                          upperBound)) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // WHITE
            Draw_DrawFilledCircle(renderer, currObsScreenPos.x,
                                  currObsScreenPos.y, currObsScreenRad);
        }
    }

    int forwardProgressAsInt = (int)state->forwardProgressTravelled;
    char str[20];
    snprintf(str, sizeof(str), "%d", forwardProgressAsInt);

    TTF_SetTextString(scoreTextObj, str, 0);
    TTF_DrawRendererText(scoreTextObj, 20, 20);

    SDL_RenderPresent(renderer);
}

int main() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Renderer *renderer = NULL;
    SDL_Window *window = NULL;

    int windowWidth = 500;
    int windowHeight = 1000;

    if (!SDL_CreateWindowAndRenderer("OML", windowWidth, windowHeight, 0,
                                     &window, &renderer)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    };
    SDL_RaiseWindow(window);

    // Init font rendering
    TTF_Init();
    TTF_Font *font = TTF_OpenFont("./assets/Jersey10-Regular.ttf", 36.0f);
    if (!font) {
        SDL_Log("Failed to load font: %s", SDL_GetError());
        return 1;
    }

    // Create font engine
    TTF_TextEngine *textEngine = TTF_CreateRendererTextEngine(renderer);
    TTF_Text *scoreTextObj =
        TTF_CreateText(textEngine, font, "Hello, world", 0);

    // Generate world
    uint32_t seed = 400;
    WorldGen worldGen = newWorldGen(seed);

    // Init game
    GameSettings settings = newGameSettings(windowWidth, windowHeight);
    Camera camera = {
        .x = 0,
        .y = 0,
        .pixelsPerMeter = PIXELS_PER_METER,
    };
    GameState state = newGameState(camera);

    // Generate Hero
    Hero hero = {
        .x = 0,
        .y = 0,
        .vx = 0,
        .vy = settings.speed,
        .rad = 0.3,
    };

    float targetFps = 120;
    int quit = 0;
    SDL_Event event;
    bool spacePressed = false;

    uint64_t lastFrameTime = SDL_GetTicks();
    while (!quit && !state.gameOver) {
        // INITIALIZE FRAME
        uint64_t frameStart = SDL_GetTicks();

        // dt is the amount of seconds since the last frame
        float dt = (float)(frameStart - lastFrameTime) / 1000.0f;
        lastFrameTime = frameStart;

        // TODO: Move this out to some event tracking struct + function to
        // update it Poll events
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_QUIT:
                quit = 1;
                break;
            case SDL_EVENT_KEY_DOWN:
                spacePressed = true;
                break;
            case SDL_EVENT_KEY_UP:
                spacePressed = false;
                break;
            }
        }

        // ------- HANDLE GAME AND RENDER GRAPHICS -------
        // Black background
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);
        updateGame(&worldGen, &settings, &state, &hero, dt, spacePressed,
                   SDL_GetTicks());
        renderGame(renderer, &worldGen, &state, &settings, &hero, scoreTextObj);

        // ------ END GAME AND RENDERING -------
        // End of frame processing
        // ms since start of frame is frameEnd - frameStart
        uint64_t frameEnd = SDL_GetTicks();

        // now to hit our target fps we should sleep for (1000/targetFps) -
        // (frameEnd - frameStart)
        int toWait = (int)((1000.0f / targetFps) - (frameEnd - frameStart));
        if (toWait > 0) {
            SDL_Delay(toWait);
        }
    }

    TTF_CloseFont(font);
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
