main: SDL_main.c
	clang -std=c99 -Wall -Werror -DOML_SDL SDL_main.c -o sdl_main `pkg-config --cflags --libs sdl3 sdl3-ttf`
