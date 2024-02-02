#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <cmath>
#include <exception>
#include "renderer.hpp"
#include "camera.hpp"
#include "platforms.hpp"

extern "C" {
#include "ktga/ktga.h"
}

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
	
	camera_c camera = camera_c(80, 0.1f, 100.0f, 4.0f / 3.0f);
	renderer_c renderer = renderer_c(window, &camera);
	glfwSwapInterval(1);

	std::vector<shader_stage_t> shaders = {
		renderer.create_shader_stage(shader_stage_type::VERTEX, "assets/shaders/shader.vert"),
		renderer.create_shader_stage(shader_stage_type::FRAGMENT, "assets/shaders/shader.frag"),
	};

	shader_descriptor_t desc = {
		.stages = {
			shader_stage_type::VERTEX,
			shader_stage_type::FRAGMENT,
		},
		.starting_stage = shader_stage_type::VERTEX,
		.inputs = {
			{ shader_data_type::F32, 3 },
			{ shader_data_type::F32, 2 },
		},
		.uniforms = {
			{ shader_data_type::F32, 4, "unif_material_color" },
			{ shader_data_type::TEXTURE, 1, "unif_texture_albedo" },
			{ shader_data_type::MAT4x4, 1, "unif_mvp" },
		},
		.texture_attachments = {
			{ shader_texture_attachment_type::ALBEDO, "unif_texture_albedo" },
		},
	};

	shader_t shader = renderer.create_shader(desc, shaders);

	transform_t transform = {
		.x = 0.0f, .y = 0.0f, .z = 0.0f,
		.rx = 0.0f, .ry = 0.0f, .rz = 0.0f,
		.sx = 1.0f, .sy = 1.0f, .sz = 1.0f,
	};

	texture_t albedo = 0;
	{
		ktga_t tga;
		std::ifstream file("assets/textures/test.tga", std::ios::binary);
		if (!file.is_open()) {
			throw std::runtime_error("Failed to open file");
		}

		std::vector<char> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		ktga_load(&tga, buffer.data(), static_cast<unsigned long long int>(buffer.size()));
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
		.a = 1.0f,
		.textures = { albedo },
	};

	mesh_t* mesh = renderer.create_mesh(transform, material, shader);

	float vertices[] = {
		-0.5f, -0.5f, 0.0f,		0, 0,
		 0.5f, -0.5f, 0.0f,		1, 0,
		 0.0f,  0.5f, 0.0f,		0.5f, 1,
	};
	renderer.mesh_upload(mesh, vertices, sizeof(vertices));

	while (!glfwWindowShouldClose(window)) {
		mesh->material.r = std::abs(std::sin(glfwGetTime()));
		mesh->material.g = std::abs(std::cos(glfwGetTime()));
		mesh->material.b = std::abs(std::sin(glfwGetTime() / 2));
		camera.transform.z = std::abs(std::cos(glfwGetTime()));
		renderer.draw();
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
