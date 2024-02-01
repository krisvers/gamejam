#ifndef RENDERER_HPP
#define RENDERER_HPP

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <vector>
#include <string>
#include <exception>
#include "types.hpp"
#include "camera.hpp"
#include "utils.hpp"

enum class shader_stage_type {
	VERTEX = 0,
	FRAGMENT,
	GEOMETRY,
	COMPUTE,
};

typedef u32 texture_t;
typedef u32 shader_stage_t;
typedef u32 shader_t;

enum class texture_format {
	RGB = 0,
	RGBA,
	BGR,
	BGRA,
};

enum class shader_data_type {
	U8 = 0,
	U16,
	U32,
	S8,
	S16,
	S32,
	F32,
	MAT4x4,
};

struct shader_input_t {
	shader_data_type type;
	u32 size;
};

struct shader_uniform_t {
	shader_data_type type;
	u32 size;
	std::string* name;
};

struct shader_descriptor_t {
	std::vector<shader_stage_type> stages;
	shader_stage_type starting_stage;
	std::vector<shader_input_t> inputs;
	std::vector<shader_uniform_t> uniforms;
};

struct material_t {
	f32 r, g, b, a;
	texture_t texture;
};

struct mesh_t {
	u32 id;
	transform_t transform;
	material_t material;
	shader_t shader;
};

inline const usize shader_data_type_size(shader_data_type type) {
	switch (type) {
	case shader_data_type::U8:
		return sizeof(u8);
	case shader_data_type::U16:
		return sizeof(u16);
	case shader_data_type::U32:
		return sizeof(u32);
	case shader_data_type::S8:
		return sizeof(s8);
	case shader_data_type::S16:
		return sizeof(s16);
	case shader_data_type::S32:
		return sizeof(s32);
	case shader_data_type::F32:
		return sizeof(f32);
	case shader_data_type::MAT4x4:
		return sizeof(f32) * 16;
	default:
		return 0;
	}
}

struct renderer_c {
	GLFWwindow* window;
	camera_c* camera;
	struct renderer_internal_t * internal;

	renderer_c(GLFWwindow* window, camera_c* camera);

	shader_stage_t create_shader_stage(shader_stage_type type, const char* filepath);
	shader_t create_shader(const shader_descriptor_t& descriptor, const std::vector<shader_stage_t>& stages);
	void shader_uniform(shader_t shader, const std::string& name, void* data, usize size);

	mesh_t* create_mesh(const transform_t& transform, const material_t& material, shader_t shader);
	void mesh_upload(mesh_t* mesh, void * data, usize bytesize);

	texture_t* create_texture(u32 width, u32 height, u8 bpp, texture_format format, void* data, usize bytesize);

	void draw();
};

#endif
