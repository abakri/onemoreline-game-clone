main: main.c
	clang -std=c99 -Wall -Werror -framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL libraylib.a main.c -o main
