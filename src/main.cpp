#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <linmath.h>
#include <iostream>
#include <fstream>
#include <cmath>
#include <exception>
#include "renderer.hpp"
#include "input.hpp"
#include "camera.hpp"
#include "platforms.hpp"
#include "ktga/ktga.hpp"

#ifdef PYLAUNCHER
#include <filesystem>
#endif

int main(int argc, char ** argv) {
	glfwSetErrorCallback([](int error, const char* description) {
		std::cerr << "Error: " << description << "\n";
	});

	if (!glfwInit()) {
		return -1;
	}
	
	/* pylauncher workaround (glfwInit() resets the working dir for some reason on macOS) */
	#ifdef PYLAUNCHER
	if (argc >= 2) {
		std::filesystem::current_path(argv[1]);
	}
	#endif

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	#ifdef PLATFORM_APPLE_MACOS
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
	#endif

	GLFWwindow* window = glfwCreateWindow(800, 600, "GameJam Engine", NULL, NULL);
	if (window == nullptr) {
		glfwTerminate();
		return -1;
	}

	input::register_input(window);
	
	camera_c camera = camera_c(80, 0.1f, 100.0f, 4.0f / 3.0f);
	renderer_c renderer = renderer_c(window, &camera);
	glfwSwapInterval(1);

	transform_t transform = {
		.position = { 0, 0, 0 },
		.rotation = { 0, 0, 0 },
		.scale = { 1, 1, 1 },
	};

	texture_t albedo = 0;
	{
		ktga_t tga;
		std::ifstream file("assets/textures/test.tga", std::ios::binary);

		if (!file.is_open()) {
			std::string error = "Failed to open file assets/textures/test.tga";
			throw std::runtime_error(error);
		}

		std::vector<char> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		if (ktga_load(&tga, buffer.data(), static_cast<unsigned long long int>(buffer.size())) != 0) {
			std::string error = "Failed to load tga bitmap from assets/textures/test.tga";
			throw std::runtime_error(error);
		}

		file.close();

		texture_descriptor_t tex_desc = {
			.width = tga.header.img_w,
			.height = tga.header.img_h,
			.bits_per_pixel = tga.header.bpp,
			.format = texture_format::BGRA,
			.filter = texture_filter::NEAREST,
			.wrap = texture_wrap::CLAMP_TO_EDGE,
		};

		albedo = renderer.create_texture(tex_desc, tga.bitmap, sizeof(tga.bitmap));
	}

	material_t material = {
		.r = 1.0f,
		.g = 0.0f,
		.b = 0.7f,
		.textures = { albedo },
	};

	mesh_t* mesh = renderer.create_mesh(transform, material, 0);

	float vertices[] = {
		-0.5f, -0.5f, 0.0f,		0, 0,
		 0.5f, -0.5f, 0.0f,		1, 0,
		 0.0f,  0.5f, 0.0f,		0.5f, 1,
	};
	renderer.mesh_upload(mesh, vertices, sizeof(vertices));

	double time;
	float delta_time = 0;
	while (!glfwWindowShouldClose(window)) {
		time = glfwGetTime();
		mesh->material.r = std::abs(std::sin(glfwGetTime()));
		mesh->material.g = std::abs(std::cos(glfwGetTime()));
		mesh->material.b = std::abs(std::sin(glfwGetTime() / 2));

		if (input::key_down(GLFW_KEY_ESCAPE)) {
			break;
		}

		vec3 forward = { std::sinf(camera.transform.rotation[1] * (M_PI / 180.0f)), 0, -std::cosf(camera.transform.rotation[1] * (M_PI / 180.0f)) };
		vec3 up = { 0, 1, 0 };
		vec3 right;
		vec3_mul_cross(right, forward, up);

		vec3_scale(forward, forward, delta_time);
		vec3_scale(right, right, delta_time);
		vec3_scale(up, up, delta_time);
		
		if (input::key(GLFW_KEY_W)) {
			vec3_add(camera.transform.position, camera.transform.position, forward);
		}
		if (input::key(GLFW_KEY_S)) {
			vec3_sub(camera.transform.position, camera.transform.position, forward);
		}
		if (input::key(GLFW_KEY_E)) {
			vec3_add(camera.transform.position, camera.transform.position, up);
		}
		if (input::key(GLFW_KEY_Q)) {
			vec3_sub(camera.transform.position, camera.transform.position, up);
		}
		if (input::key(GLFW_KEY_D)) {
			vec3_add(camera.transform.position, camera.transform.position, right);
		}
		if (input::key(GLFW_KEY_A)) {
			vec3_sub(camera.transform.position, camera.transform.position, right);
		}

		if (input::key(GLFW_KEY_LEFT)) {
			camera.transform.rotation[1] -= delta_time * 100;
		}
		if (input::key(GLFW_KEY_RIGHT)) {
			camera.transform.rotation[1] += delta_time * 100;
		}
		if (input::key(GLFW_KEY_DOWN)) {
			camera.transform.rotation[0] -= delta_time * 100;
		}
		if (input::key(GLFW_KEY_UP)) {
			camera.transform.rotation[0] += delta_time * 100;
		}

		renderer.draw();
		glfwSwapBuffers(window);
		input::update();
		delta_time = glfwGetTime() - time;
	}

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
