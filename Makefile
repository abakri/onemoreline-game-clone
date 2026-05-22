main: main.c
	clang -std=c99 -Wall -Werror main.c -o main `pkg-config --cflags --libs sdl3 sdl3-ttf`
