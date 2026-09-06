WASM_CC ?= /opt/homebrew/opt/llvm/bin/clang

sdl: SDL_main.c
	clang -DOML_SDL -std=c99 -Wall -Werror -Wfloat-conversion -pedantic \
	  -o sdl_main SDL_main.c `pkg-config --cflags --libs sdl3 sdl3-ttf`

wasm: WASM_main.c
	$(WASM_CC) -DOML_WASM -std=c99 -Wall -Werror -Wfloat-conversion -pedantic -fno-math-errno\
	  --target=wasm32 -Os -nostdlib -mbulk-memory \
	  -Wl,--no-entry -Wl,--export-all \
	  -o web/game.wasm WASM_main.c

.PHONY: sdl wasm
