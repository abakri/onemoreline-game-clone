#pragma once
#include "types.h"

void Log(char *log_string) {
#ifdef OML_SDL
    SDL_Log("%s", log_string);
#else
    strcat(log_string, "\n") printf("%s\n", log_string)
#endif
}
