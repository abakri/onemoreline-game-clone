#include "client.c"
#include "game.c"
#include "time.h"

// TODO: A lot of the drawing here will likely be replaced with game assets
void renderGame(SDL_Renderer *renderer, ClientData *clientData,
                TTF_Text *scoreTextObj) {
    // Since state buffer is a float array, we must cast as int
    int screenHeight = (int)clientData->STATE_BUFFER[SCREEN_HEIGHT_IDX];

    // Draw boundaries on the left and right
    float leftBoundaryX = clientData->STATE_BUFFER[LEFT_BOUNDARY_X_IDX];
    float rightBoundaryX = clientData->STATE_BUFFER[RIGHT_BOUNDARY_X_IDX];
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // WHITE
    SDL_RenderLine(renderer, leftBoundaryX,
                   0, // top of screen
                   leftBoundaryX,
                   screenHeight // bottom of screen
    );
    SDL_RenderLine(renderer, rightBoundaryX,
                   0, // top of screen
                   rightBoundaryX, screenHeight);

    // Draw character
    float heroX = clientData->STATE_BUFFER[HERO_X_IDX];
    float heroY = clientData->STATE_BUFFER[HERO_Y_IDX];
    float heroRad = clientData->STATE_BUFFER[HERO_RAD_IDX];
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // WHITE
    Draw_DrawFilledCircle(renderer, heroX, heroY, heroRad);

    // Draw orbit lines
    int stateSpaceDown = (int)clientData->STATE_BUFFER[SPACE_KEY_DOWN_IDX];
    int isOrbiting = (int)clientData->STATE_BUFFER[IS_ORBITING_IDX];
    float closestObsX = clientData->STATE_BUFFER[CLOSEST_OBS_X_IDX];
    float closestObsY = clientData->STATE_BUFFER[CLOSEST_OBS_Y_IDX];

    if (stateSpaceDown && !isOrbiting) {
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // RED
        SDL_RenderLine(renderer, heroX, heroY, closestObsX, closestObsY);
    }
    if (isOrbiting) {
        float orbitX = clientData->STATE_BUFFER[ORBITING_X_IDX];
        float orbitY = clientData->STATE_BUFFER[ORBITING_Y_IDX];
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // RED
        SDL_RenderLine(renderer, heroX, heroY, orbitX, orbitY);
    }

    // Draw the obstacles
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // WHITE
    for (int i = 0; i < OBS_SOA_BUFFER_SIZE; i++) {
        float obsX = clientData->OBS_X_BUFFER[i];
        float obsY = clientData->OBS_Y_BUFFER[i];
        float obsRad = clientData->OBS_RAD_BUFFER[i];

        // If there is no valid obstacle in the slot, break
        // since the buffer will have no more valid data
        if (obsRad <= 0)
            break;

        Draw_DrawFilledCircle(renderer, obsX, obsY, obsRad);
    }

    int forwardProgress = (int)clientData->STATE_BUFFER[SCORE_IDX];
    char str[20];
    snprintf(str, sizeof(str), "%d", forwardProgress);

    TTF_SetTextString(scoreTextObj, str, 0);
    TTF_DrawRendererText(scoreTextObj, 20, 20);

    SDL_RenderPresent(renderer);
}

int main(void) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Renderer *renderer = NULL;
    SDL_Window *window = NULL;

    int windowWidth = 1000;
    int windowHeight = 720;

    // Some internal settings
    int capFps = true;
    int useVsync = true;

    if (!SDL_CreateWindowAndRenderer("OML", windowWidth, windowHeight, 0,
                                     &window, &renderer)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    };
    SDL_RaiseWindow(window);
    if (useVsync) {
        if (!SDL_SetRenderVSync(renderer, 1)) {
            SDL_Log("SDL_Init failed: %s", SDL_GetError());
            return 1;
        }
    }

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

    // Generate world (seeded based on current time)
    time_t currentTime = time(NULL);
    uint32_t seed = (uint32_t)currentTime;

    // Init game
    Game game;
    createGame(&game, windowWidth, windowHeight, seed);

    // Create client data
    ClientData clientData;
    updateClientData(&game, &clientData);

    float targetFps = 200;
    int quit = 0;
    SDL_Event event;
    bool spacePressed = false;

    uint64_t lastFrameTime = SDL_GetTicks();
    while (!quit && !game.gameOver) {
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
        updateGame(&game, dt, spacePressed, SDL_GetTicks());
        updateClientData(&game, &clientData);
        renderGame(renderer, &clientData, scoreTextObj);

        // ------ END GAME AND RENDERING -------
        // End of frame processing
        // ms since start of frame is frameEnd - frameStart
        uint64_t frameEnd = SDL_GetTicks();

        // now to hit our target fps we should sleep for (1000/targetFps) -
        // (frameEnd - frameStart)
        if (capFps) {
            int toWait = (int)((1000.0f / targetFps) - (frameEnd - frameStart));
            if (toWait > 0) {
                SDL_Delay(toWait);
            }
        }
    }

    TTF_DestroyText(scoreTextObj);
    TTF_DestroyRendererTextEngine(textEngine);
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
