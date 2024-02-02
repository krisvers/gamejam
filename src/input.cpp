#include "input.hpp"

static std::array<b8, GLFW_KEY_LAST + 1> keys;
static std::array<b8, GLFW_KEY_LAST + 1> prev_keys;

void input::register_input(GLFWwindow* window) {
	glfwSetKeyCallback(window, input::key_callback);
}

void input::update() {
	prev_keys = keys;
	glfwPollEvents();
}

b8 input::key(u16 key) {
	if (key > GLFW_KEY_LAST + 1) {
		return false;
	}

	return keys[key];
}

b8 input::key_down(u16 key) {
	if (key > GLFW_KEY_LAST + 1) {
		return false;
	}

	return keys[key] && !prev_keys[key];
}

b8 input::key_up(u16 key) {
	if (key > GLFW_KEY_LAST + 1) {
		return false;
	}

	return !keys[key] && prev_keys[key];
}

void input::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	keys[key] = (action != 0);
}
