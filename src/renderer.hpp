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

enum class texture_filter {
	NEAREST = 0,
	LINEAR,
};

enum class texture_wrap {
	REPEAT = 0,
	MIRRORED_REPEAT,
	CLAMP_TO_EDGE,
	CLAMP_TO_BORDER,
};

struct texture_descriptor_t {
	u32 width;
	u32 height;
	u8 bits_per_pixel;
	texture_format format;
	texture_filter filter;
	texture_wrap wrap;
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

	TEXTURE = S32,
};

struct shader_input_t {
	shader_data_type type;
	u32 size;
};

struct shader_uniform_t {
	shader_data_type type;
	u32 size;
	const char* name;
};

enum class shader_texture_attachment_type {
	UNKNOWN = 0,
	ALBEDO,
	NORMAL,
	SPECULAR,
	ROUGHNESS,
};

struct shader_texture_attachment_t {
	shader_texture_attachment_type type;
	const char* associated_uniform;
};

struct shader_descriptor_t {
	std::vector<shader_stage_type> stages;
	shader_stage_type starting_stage;
	std::vector<shader_input_t> inputs;
	std::vector<shader_uniform_t> uniforms;
	std::vector<shader_texture_attachment_t> texture_attachments;
	
};

struct material_t {
	f32 r, g, b;
	std::vector<texture_t> textures;
};

struct mesh_t {
	u32 id;
	transform_t transform;
	material_t material;
	shader_t shader;
};

struct light_t {
	u32 id;
	vec3 position;
	vec3 color;
	f32 intensity;
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
		return 1;
	}
}

struct vertex_t {
	vec3 pos;
	vec2 uv;
	vec3 normal;
};

struct renderer_c {
	GLFWwindow* window;
	camera_c& camera;
	struct renderer_internal_t * internal;

	renderer_c(GLFWwindow* window, camera_c& camera);
	~renderer_c();

	shader_stage_t create_shader_stage(shader_stage_type type, const char* filepath);
	void destroy_shader_stage(shader_stage_t shader);
	shader_t create_shader(const shader_descriptor_t& descriptor, const std::vector<shader_stage_t>& stages);
	s32 shader_uniform(shader_t shader, const std::string& name, void* data, usize size);
	s32 shader_uniform_unsafe(shader_t shader, const std::string& name, void* data, usize size, shader_data_type type);
	b8 shader_uniform_exists(shader_t shader, const std::string& name);
	void shader_use(shader_t shader);

	mesh_t* create_mesh(const transform_t& transform, const material_t& material, shader_t shader);
	void mesh_upload(mesh_t* mesh, void* vertex_data, usize vertex_bytesize, u32* index_data, usize index_bytesize);

	texture_t create_texture(const texture_descriptor_t& descriptor, void* data, usize bytesize);

	light_t* create_light(vec3 position, vec3 color, f32 intensity);

	void draw();
};

#endif
