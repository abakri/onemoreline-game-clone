#include "client.c"
#include "game.c"
#include "time.h"

const int TEXTURE_BUFFER_SIZE = 0;
const int CAT_TEXTURE_IDX = 0;

// COLORS
const int DARKEST_IDX = 0;
const int DARK_IDX = 1;
const int MID_IDX = 2;
const int LIGHT_IDX = 3;
const int LIGHTEST_IDX = 4;

typedef struct {
    Uint8 r;
    Uint8 g;
    Uint8 b;
    Uint8 a;
} Color;

// TODO: A lot of the drawing here will likely be replaced with game assets
void renderGame(SDL_Renderer *renderer, ClientData *clientData,
                SDL_Texture *textureBuffer[1], Color colors[5],
                TTF_Text *scoreTextObj) {
    Color DARKEST = colors[DARKEST_IDX];
    // Color DARK = colors[DARK_IDX];
    // Color MID = colors[MID_IDX];
    Color LIGHT = colors[LIGHT_IDX];
    Color LIGHTEST = colors[LIGHTEST_IDX];

    SDL_SetRenderDrawColor(renderer, DARKEST.r, DARKEST.g, DARKEST.b,
                           DARKEST.a);
    SDL_RenderClear(renderer);

    // Since state buffer is a float array, we must cast as int
    int screenHeight = (int)clientData->STATE_BUFFER[SCREEN_HEIGHT_IDX];
    int screenWidth = (int)clientData->STATE_BUFFER[SCREEN_WIDTH_IDX];

    // Draw boundaries on the left and right
    float leftBoundaryX = clientData->STATE_BUFFER[LEFT_BOUNDARY_X_IDX];
    float rightBoundaryX = clientData->STATE_BUFFER[RIGHT_BOUNDARY_X_IDX];
    SDL_SetRenderDrawColor(renderer, LIGHTEST.r, LIGHTEST.g, LIGHTEST.b,
                           LIGHTEST.a);

    float boundaryWidth = 15.f;
    SDL_FRect leftBoundaryRect = (SDL_FRect){
        .x = leftBoundaryX - boundaryWidth,
        .y = 0, // top of screen
        .w = boundaryWidth,
        .h = screenHeight,
    };
    SDL_FRect rightBoundaryRect = (SDL_FRect){
        .x = rightBoundaryX,
        .y = 0, // top of screen
        .w = boundaryWidth,
        .h = screenHeight,
    };
    SDL_SetRenderDrawColor(renderer, LIGHTEST.r, LIGHTEST.g, LIGHTEST.b,
                           LIGHTEST.a);
    SDL_RenderFillRect(renderer, &leftBoundaryRect);
    SDL_RenderFillRect(renderer, &rightBoundaryRect);

    // Everything outside of the boundaries should be black
    SDL_FRect leftOOBRect = (SDL_FRect){
        .x = 0,
        .y = 0, // top of screen
        .w = leftBoundaryX - boundaryWidth,
        .h = screenHeight,
    };

    SDL_FRect rightOOBRect = (SDL_FRect){
        .x = rightBoundaryX + boundaryWidth,
        .y = 0, // top of screen
        .w = screenWidth - rightBoundaryX - boundaryWidth,
        .h = screenHeight,
    };
    SDL_SetRenderDrawColor(renderer, 28, 13, 27, 255); // BLACK
    SDL_RenderFillRect(renderer, &leftOOBRect);
    SDL_RenderFillRect(renderer, &rightOOBRect);

    // Draw character
    float heroX = clientData->STATE_BUFFER[HERO_X_IDX];
    float heroY = clientData->STATE_BUFFER[HERO_Y_IDX];
    float heroRad = clientData->STATE_BUFFER[HERO_RAD_IDX];
    // SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // WHITE
    // Draw_DrawFilledCircle(renderer, heroX, heroY, heroRad);
    SDL_FRect heroRect;
    heroRect.x = heroX - heroRad;
    heroRect.y = heroY - heroRad;
    heroRect.w = 2 * heroRad;
    heroRect.h = 2 * heroRad;
    SDL_RenderTexture(renderer, textureBuffer[CAT_TEXTURE_IDX], NULL,
                      &heroRect);

    // Draw orbit lines
    int stateSpaceDown = (int)clientData->STATE_BUFFER[SPACE_KEY_DOWN_IDX];
    int isOrbiting = (int)clientData->STATE_BUFFER[IS_ORBITING_IDX];
    float closestObsX = clientData->STATE_BUFFER[CLOSEST_OBS_X_IDX];
    float closestObsY = clientData->STATE_BUFFER[CLOSEST_OBS_Y_IDX];

    if (stateSpaceDown && !isOrbiting) {
        SDL_SetRenderDrawColor(renderer, LIGHT.r, LIGHT.g, LIGHT.b,
                               LIGHT.a); // RED
        SDL_RenderLine(renderer, heroX, heroY, closestObsX, closestObsY);
    }
    if (isOrbiting) {
        float orbitX = clientData->STATE_BUFFER[ORBITING_X_IDX];
        float orbitY = clientData->STATE_BUFFER[ORBITING_Y_IDX];
        SDL_SetRenderDrawColor(renderer, LIGHT.r, LIGHT.g, LIGHT.b,
                               LIGHT.a); // RED
        SDL_RenderLine(renderer, heroX, heroY, orbitX, orbitY);
    }

    // Draw the obstacles
    SDL_SetRenderDrawColor(renderer, LIGHTEST.r, LIGHTEST.g, LIGHTEST.b,
                           LIGHTEST.a);
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

    // Setup textures
    SDL_Texture *textureBuffer[1];
    SDL_Texture *catTexture =
        IMG_LoadTexture(renderer, "./assets/sprites/cat.png");
    if (!catTexture) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    textureBuffer[CAT_TEXTURE_IDX] = catTexture;

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

    // Create colors
    Color colors[5];
    // Darkest
    colors[0] = (Color){.r = 55, .g = 33, .b = 52, .a = 255};
    // Dark
    colors[1] = (Color){.r = 71, .g = 69, .b = 118, .a = 255};
    // Mid
    colors[2] = (Color){.r = 72, .g = 136, .b = 183, .a = 255};
    // Light
    colors[3] = (Color){.r = 109, .g = 188, .b = 185, .a = 255};
    // Lightest
    colors[4] = (Color){.r = 140, .g = 239, .b = 182, .a = 255};

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
        updateGame(&game, dt, spacePressed, SDL_GetTicks());
        updateClientData(&game, &clientData);
        renderGame(renderer, &clientData, textureBuffer, colors, scoreTextObj);

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

    SDL_DestroyTexture(catTexture);
    TTF_DestroyText(scoreTextObj);
    TTF_DestroyRendererTextEngine(textEngine);
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
