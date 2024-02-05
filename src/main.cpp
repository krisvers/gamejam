#define _USE_MATH_DEFINES
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
#include "kobj/kobj.hpp"

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
	
	camera_c camera = camera_c(80, 0.1f, 100.0, 4.0 / 3.0);
	renderer_c renderer = renderer_c(window, camera);
	glfwSwapInterval(1);

	transform_t transform = {
		.position = { 0, 0, 0 },
		.rotation = { 90, 0, 0 },
		.scale = { 5.0f, 5.0f, 5.0f },
	};

	texture_t albedo = 0;
	{
		ktga_t tga;
		std::ifstream file("assets/textures/albedo.tga", std::ios::binary);

		if (!file.is_open()) {
			std::string error = "Failed to open file assets/textures/albedo.tga";
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

		albedo = renderer.create_texture(tex_desc, tga.bitmap, tex_desc.width * tex_desc.height * tex_desc.bits_per_pixel / 8);
	}

	texture_t normal = 0;
	{
		ktga_t tga;
		std::ifstream file("assets/textures/normal.tga", std::ios::binary);

		if (!file.is_open()) {
			std::string error = "Failed to open file assets/textures/normal.tga";
			throw std::runtime_error(error);
		}

		std::vector<char> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		if (ktga_load(&tga, buffer.data(), static_cast<unsigned long long int>(buffer.size())) != 0) {
			std::string error = "Failed to load tga bitmap from assets/textures/normal.tga";
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

		normal = renderer.create_texture(tex_desc, tga.bitmap, tex_desc.width * tex_desc.height * tex_desc.bits_per_pixel / 8);
	}

	texture_t specular = 0;
	{
		texture_descriptor_t tex_desc = {
			.width = 1,
			.height = 1,
			.bits_per_pixel = 32,
			.format = texture_format::BGRA,
			.filter = texture_filter::NEAREST,
			.wrap = texture_wrap::CLAMP_TO_EDGE,
		};

		u32 bitmap[] = { 0x00000000 };

		specular = renderer.create_texture(tex_desc, bitmap, tex_desc.width * tex_desc.height * tex_desc.bits_per_pixel / 8);
	}

	material_t material = {
		.r = 1.0f,
		.g = 1.0f,
		.b = 1.0f,
		.textures = { albedo, normal, specular },
	};

	vertex_t testv[8] = {
		// Front vertices
		{ { -0.5f, -0.5f, 1.0f }, { 0, 0 }, { 0, 0, 1 } }, // 0
		{ { 0.5f, -0.5f, 0.5f }, { 1, 0 }, { 0, 0, 1 } },  // 1
		{ { 0.5f, 0.5f, 0.5f }, { 1, 1 }, { 0, 0, 1 } },   // 2
		{ { -0.5f, 0.5f, 1.0f }, { 0, 1 }, { 0, 0, 1 } },  // 3

		// Back vertices
		{ { -0.5f, -0.5f, -0.5f }, { 1, 0 }, { 0, 0, -1 } }, // 4
		{ { 0.5f, -0.5f, -1.0f }, { 0, 0 }, { 0, 0, -1 } },  // 5
		{ { 0.5f, 0.5f, -1.0f }, { 0, 1 }, { 0, 0, -1 } },   // 6
		{ { -0.5f, 0.5f, -0.5f }, { 1, 1 }, { 0, 0, -1 } }   // 7
	};

	u32 testi[36] = {
		// Front face
		0, 1, 2, 2, 3, 0,
		// Back face
		4, 5, 6, 6, 7, 4,
		// Top face
		3, 2, 6, 6, 7, 3,
		// Bottom face
		0, 1, 5, 5, 4, 0,
		// Right face
		1, 5, 6, 6, 2, 1,
		// Left face
		0, 4, 7, 7, 3, 0
	};

	renderer.mesh_upload(renderer.create_mesh(transform, material, 0), testv, sizeof(testv), testi, sizeof(testi));
	transform.scale[0] = 1;
	transform.scale[1] = 1;
	transform.scale[2] = 1;

	material.textures[0] = specular;
	material.textures[1] = specular;
	material.textures[2] = specular;

	mesh_t* light_mesh = renderer.create_mesh(transform, material, 0);
	renderer.mesh_upload(light_mesh, testv, sizeof(testv), testi, sizeof(testi));
	light_mesh->transform.position[1] = 1;

	light_mesh->transform.scale[0] *= 0.05f;
	light_mesh->transform.scale[1] *= 0.05f;
	light_mesh->transform.scale[2] *= 0.05f;

	material.textures[0] = albedo;
	material.textures[1] = normal;
	material.textures[2] = specular;

	std::vector<mesh_t*> meshes = std::vector<mesh_t*>(6);
	for (usize i = 0; i < meshes.size(); ++i) {
		meshes[i] = renderer.create_mesh(transform, material, 0);
		renderer.mesh_upload(meshes[i], testv, sizeof(testv), testi, sizeof(testi));
		meshes[i]->transform.position[0] = std::sinf(i) * 2;
		meshes[i]->transform.position[1] = std::sinf(i) / 5 + 0.5f;
		meshes[i]->transform.position[2] = std::cosf(i) * 2;
	}

	light_t* light = nullptr;
	{
		vec3 pos = { 0, 0, 0 };
		vec3 color = { 1, 1, 1 };
		light = renderer.create_light(pos, color, 20);
	}

	double time;
	float delta_time = 0;
	while (!glfwWindowShouldClose(window)) {
		time = glfwGetTime();
		/*
		mesh->material.r = std::abs(std::sin(glfwGetTime()));
		mesh->material.g = std::abs(std::cos(glfwGetTime()));
		mesh->material.b = std::abs(std::sin(glfwGetTime() / 2));
		*/

		light->position[0] = std::sinf(glfwGetTime());
		light->position[2] = std::cosf(glfwGetTime());

		light_mesh->transform.position[0] = light->position[0];
		light_mesh->transform.position[2] = light->position[2];
		light_mesh->transform.rotation[0] = std::sinf(glfwGetTime()) * 6;
		light_mesh->transform.rotation[1] = std::sinf(glfwGetTime()) * 6;
		light_mesh->transform.rotation[2] = std::cosf(glfwGetTime()) * 6;

		if (input::key_down(GLFW_KEY_ESCAPE)) {
			break;
		}

		vec3 forward = { std::sinf(camera.transform.rotation[1] * (M_PI / 180.0)), 0, -std::cosf(camera.transform.rotation[1] * (M_PI / 180.0)) };
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
