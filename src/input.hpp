#ifndef INPUT_HPP
#define INPUT_HPP

#include "types.hpp"
#include <GLFW/glfw3.h>
#include <array>

struct input {
	static void register_input(GLFWwindow* window);
	static void update();

	static b8 key(u16 key);
	static b8 key_down(u16 key);
	static b8 key_up(u16 key);

	static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
};

#endif
