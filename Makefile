INCLUDES = ./include
FLAGS = -std=c++17 -DPYLAUNCHER
MACLIB = -Llib/mac/universal -lglfw3 -framework IOKit -framework Cocoa -framework OpenGL

mac-x86_64:
	clang++ $(shell find . -type f -name "*.cpp") lib/src/glad.c -o gamejam $(FLAGS) -I$(INCLUDES) -Llib/mac/x86_64 $(MACLIB)

mac-arm64:
	clang++ $(shell find . -type f -name "*.cpp") lib/src/glad.c -o gamejam $(FLAGS) -I$(INCLUDES) -Llib/mac/arm64 $(MACLIB)

pylaunch:
	pylauncher ./gamejam $(PWD)
