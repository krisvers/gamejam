#include "renderer.hpp"
#include <glad/glad.h>
#include <linmath.h>
#include <iostream>
#include <fstream>
#include <exception>
#include <filesystem>

struct mesh_internal_t {
	mesh_t * mesh;
	u32 index;
	u32 size;
};

struct shader_internal_t {
	shader_t shader;
	u32 vertex_size;
	GLuint program;
	GLuint vao;
	GLuint vbo;
	u32 buffer_size;

	std::vector<shader_uniform_t> uniforms;
};

struct renderer_internal_t {
	std::vector<mesh_internal_t> meshes;
	std::vector<shader_internal_t> shaders;
};

inline const GLenum shader_data_type_to_gl(shader_data_type type) {
	switch (type) {
	case shader_data_type::U8:
		return GL_UNSIGNED_BYTE;
	case shader_data_type::U16:
		return GL_UNSIGNED_SHORT;
	case shader_data_type::U32:
		return GL_UNSIGNED_INT;
	case shader_data_type::S8:
		return GL_BYTE;
	case shader_data_type::S16:
		return GL_SHORT;
	case shader_data_type::S32:
		return GL_INT;
	case shader_data_type::F32:
		return GL_FLOAT;
	case shader_data_type::MAT4x4:
		return GL_NONE;
	default:
		throw std::runtime_error("Invalid shader data type");
	}
}

inline const GLenum shader_stage_to_gl(shader_stage_type type) {
	switch (type) {
	case shader_stage_type::VERTEX:
		return GL_VERTEX_SHADER;
	case shader_stage_type::FRAGMENT:
		return GL_FRAGMENT_SHADER;
	case shader_stage_type::GEOMETRY:
		return GL_GEOMETRY_SHADER;
	default:
		throw std::runtime_error("Invalid shader stage type");
	}
}

renderer_c::renderer_c(GLFWwindow* window, camera_c* camera) {
	this->window = window;
	this->camera = camera;
	glfwMakeContextCurrent(window);
	if (gladLoadGLLoader((GLADloadproc) glfwGetProcAddress) == 0) {
		throw std::runtime_error("Failed to initialize GLAD");
	}

	this->internal = new renderer_internal_t;

	const char* vshader_source = R"(
		#version 410 core
		layout (location = 0) in vec3 in_pos;
		layout (location = 1) in vec2 in_uv;
		out vec2 v_uv;
		void main() {
			gl_Position = vec4(in_pos, 1.0);
			v_uv = in_uv.xy;
		}
	)";

	const char* fshader_source = R"(
		#version 410 core
		in vec2 v_uv;
		out vec4 out_color;
		uniform vec4 unif_material_color;
		uniform sampler2D unif_texture;
		void main() {
			out_color = vec4(unif_material_color);
		}
	)";

	GLuint vshader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vshader, 1, &vshader_source, NULL);
	glCompileShader(vshader);
	GLint success;
	glGetShaderiv(vshader, GL_COMPILE_STATUS, &success);
	if (!success) {
		GLchar info_log[512];
		glGetShaderInfoLog(vshader, 512, NULL, info_log);
		throw std::runtime_error(info_log);
	}

	GLuint fshader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fshader, 1, &fshader_source, NULL);
	glCompileShader(fshader);
	glGetShaderiv(fshader, GL_COMPILE_STATUS, &success);
	if (!success) {
		GLchar info_log[512];
		glGetShaderInfoLog(fshader, 512, NULL, info_log);
		throw std::runtime_error(info_log);
	}

	GLuint program = glCreateProgram();
	glAttachShader(program, vshader);
	glAttachShader(program, fshader);
	glLinkProgram(program);
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success) {
		GLchar info_log[512];
		glGetProgramInfoLog(program, 512, NULL, info_log);
		throw std::runtime_error(info_log);
	}

	glDeleteShader(vshader);
	glDeleteShader(fshader);

	shader_descriptor_t desc = {
		.stages = { shader_stage_type::VERTEX, shader_stage_type::FRAGMENT },
		.starting_stage = shader_stage_type::VERTEX,
		.inputs = {
			{ shader_data_type::F32, 3 },
			{ shader_data_type::F32, 2 },
		},
		.uniforms = {
			{ shader_data_type::F32, 4, new std::string("unif_material_color") },
			{ shader_data_type::S32, 1, new std::string("unif_texture") },
		},
	};

	std::vector<shader_stage_t> stages = {
		vshader,
		fshader,
	};

	create_shader(desc, stages);
}

shader_stage_t renderer_c::create_shader_stage(shader_stage_type type, const char* filepath) {
	std::ifstream file(filepath);
	if (!file.good()) {
		std::string error = "File open error ";
		error += filepath;
		throw std::runtime_error(error.c_str());
	}
	if (!file.is_open()) {
		std::string error = "Failed to find shader ";
		error += filepath;
		throw std::runtime_error(error.c_str());
	}
	std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	file.close();

	GLenum gl_type = shader_stage_to_gl(type);
	GLuint shader = glCreateShader(gl_type);

	const char* csource = source.c_str();
	glShaderSource(shader, 1, &csource, NULL);
	glCompileShader(shader);
	GLint success;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		GLchar info_log[512];
		glGetShaderInfoLog(shader, 512, NULL, info_log);
		throw std::runtime_error(info_log);
	}

	return shader;
}

shader_t renderer_c::create_shader(const shader_descriptor_t& desc, const std::vector<shader_stage_t>& stages) {
	shader_internal_t shader_internal = {
		.shader = static_cast<shader_t>(this->internal->shaders.size()),
		.vertex_size = 0,
		.program = 0,
		.vao = 0,
		.vbo = 0,
		.buffer_size = 0,
		.uniforms = desc.uniforms,
	};

	glGenVertexArrays(1, &shader_internal.vao);
	glGenBuffers(1, &shader_internal.vbo);

	glBindVertexArray(shader_internal.vao);
	glBindBuffer(GL_ARRAY_BUFFER, shader_internal.vbo);
	glBufferData(GL_ARRAY_BUFFER, 0, NULL, GL_DYNAMIC_DRAW);

	usize stride = 0;
	for (usize i = 0; i < desc.inputs.size(); i++) {
		stride += desc.inputs[i].size * shader_data_type_size(desc.inputs[i].type);
	}

	shader_internal.vertex_size = stride;

	usize offset = 0;
	for (usize i = 0; i < desc.inputs.size(); i++) {
		glVertexAttribPointer(i, desc.inputs[i].size, shader_data_type_to_gl(desc.inputs[i].type), GL_FALSE, stride, (void*)offset);
		glEnableVertexAttribArray(i);
		offset += desc.inputs[i].size * shader_data_type_size(desc.inputs[i].type);
	}

	glBindVertexArray(0);

	shader_internal.program = glCreateProgram();
	for (usize i = 0; i < stages.size(); i++) {
		glAttachShader(shader_internal.program, stages[i]);
	}

	glLinkProgram(shader_internal.program);
	GLint success;
	glGetProgramiv(shader_internal.program, GL_LINK_STATUS, &success);
	if (!success) {
		GLchar info_log[512];
		glGetProgramInfoLog(shader_internal.program, 512, NULL, info_log);
		throw std::runtime_error(info_log);
	}

	this->internal->shaders.push_back(shader_internal);
	return shader_internal.shader;
}

void renderer_c::shader_uniform(shader_t shader, const std::string& name, void* data, usize size) {
	if (this->internal->shaders.size() <= shader) {
		throw std::runtime_error("Shader does not exist");
	}

	if (this->internal->shaders[shader].uniforms.size() <= 0) {
		throw std::runtime_error("Shader has no uniforms");
	}

	usize uniform = USIZE_MAX;
	for (usize i = 0; i < this->internal->shaders[shader].uniforms.size(); i++) {
		if (this->internal->shaders[shader].uniforms[i].name->compare(name) == 0) {
			uniform = i;
			break;
		}
	}

	if (uniform == USIZE_MAX) {
		throw std::runtime_error("Uniform does not exist");
	}

	if (size != this->internal->shaders[shader].uniforms[uniform].size * shader_data_type_size(this->internal->shaders[shader].uniforms[uniform].type)) {
		throw std::runtime_error("Uniform size does not match");
	}

	GLint location = glGetUniformLocation(this->internal->shaders[shader].program, name.c_str());
	if (location == -1) {
		throw std::runtime_error("Uniform does not exist");
	}

	usize amount = size / shader_data_type_size(this->internal->shaders[shader].uniforms[uniform].type);
	switch (this->internal->shaders[shader].uniforms[uniform].type) {
	case shader_data_type::U32:
		if (amount == 1) {
			glUniform1ui(location, reinterpret_cast<u32*>(data)[0]);
		} else if (amount == 2) {
			glUniform2ui(location, reinterpret_cast<u32*>(data)[0], reinterpret_cast<u32*>(data)[1]);
		} else if (amount == 3) {
			glUniform3ui(location, reinterpret_cast<u32*>(data)[0], reinterpret_cast<u32*>(data)[1], reinterpret_cast<u32*>(data)[2]);
		} else if (amount == 4) {
			glUniform4ui(location, reinterpret_cast<u32*>(data)[0], reinterpret_cast<u32*>(data)[1], reinterpret_cast<u32*>(data)[2], reinterpret_cast<u32*>(data)[3]);
		} else {
			glUniform1uiv(location, amount, reinterpret_cast<u32*>(data));
		}
		break;
	case shader_data_type::S32:
		if (amount == 1) {
			glUniform1i(location, reinterpret_cast<s32*>(data)[0]);
		} else if (amount == 2) {
			glUniform2i(location, reinterpret_cast<s32*>(data)[0], reinterpret_cast<s32*>(data)[1]);
		} else if (amount == 3) {
			glUniform3i(location, reinterpret_cast<s32*>(data)[0], reinterpret_cast<s32*>(data)[1], reinterpret_cast<s32*>(data)[2]);
		} else if (amount == 4) {
			glUniform4i(location, reinterpret_cast<s32*>(data)[0], reinterpret_cast<s32*>(data)[1], reinterpret_cast<s32*>(data)[2], reinterpret_cast<s32*>(data)[3]);
		} else {
			glUniform1iv(location, amount, reinterpret_cast<s32*>(data));
		}
		break;
	case shader_data_type::F32:
		if (amount == 1) {
			glUniform1f(location, reinterpret_cast<f32*>(data)[0]);
		} else if (amount == 2) {
			glUniform2f(location, reinterpret_cast<f32*>(data)[0], reinterpret_cast<f32*>(data)[1]);
		} else if (amount == 3) {
			glUniform3f(location, reinterpret_cast<f32*>(data)[0], reinterpret_cast<f32*>(data)[1], reinterpret_cast<f32*>(data)[2]);
		} else if (amount == 4) {
			glUniform4f(location, reinterpret_cast<f32*>(data)[0], reinterpret_cast<f32*>(data)[1], reinterpret_cast<f32*>(data)[2], reinterpret_cast<f32*>(data)[3]);
		} else {
			glUniform1fv(location, amount, reinterpret_cast<f32*>(data));
		}
		break;
	case shader_data_type::MAT4x4:
		glUniformMatrix4fv(location, 1, false, reinterpret_cast<f32*>(data));
		break;
	default:
		throw std::runtime_error("Invalid shader data type");
	}
}

mesh_t* renderer_c::create_mesh(const transform_t& transform, const material_t& material, shader_t shader) {
	if (this->internal->shaders.size() <= shader) {
		throw std::runtime_error("Shader does not exist");
	}

	mesh_internal_t mesh_internal = {
		.mesh = new mesh_t {
			.id = static_cast<u32>(this->internal->meshes.size()),
			.transform = transform,
			.material = material,
			.shader = shader,
		},
		.index = U32_MAX,
		.size = U32_MAX,
	};

	this->internal->meshes.push_back(mesh_internal);
	return mesh_internal.mesh;
}

void renderer_c::mesh_upload(mesh_t* mesh, void* data, usize bytesize) {
	if (this->internal->meshes.size() <= mesh->id) {
		throw std::runtime_error("Mesh does not exist");
	}

	mesh_internal_t mesh_internal = this->internal->meshes[mesh->id];
	if (this->internal->shaders.size() <= mesh_internal.mesh->shader) {
		throw std::runtime_error("Shader does not exist");
	}

	glBindVertexArray(this->internal->shaders[mesh_internal.mesh->shader].vao);
	if (this->internal->shaders[mesh_internal.mesh->shader].buffer_size == 0) {
		glBindBuffer(GL_ARRAY_BUFFER, this->internal->shaders[mesh_internal.mesh->shader].vbo);
		glBufferData(GL_ARRAY_BUFFER, bytesize, data, GL_DYNAMIC_DRAW);
	} else {
		glBindBuffer(GL_ARRAY_BUFFER, this->internal->shaders[mesh_internal.mesh->shader].vbo);
		glBufferSubData(GL_ARRAY_BUFFER, this->internal->shaders[mesh_internal.mesh->shader].buffer_size, bytesize, data);
	}

	mesh_internal.index = this->internal->shaders[mesh_internal.mesh->shader].buffer_size;
	mesh_internal.size = bytesize / this->internal->shaders[mesh_internal.mesh->shader].vertex_size;
	this->internal->shaders[mesh_internal.mesh->shader].buffer_size += bytesize;
	this->internal->meshes[mesh->id] = mesh_internal;
}

void renderer_c::draw() {
	glClear(GL_COLOR_BUFFER_BIT);

	this->camera->calculate_matrices();
	for (usize i = 0; i < this->internal->meshes.size(); i++) {
		mesh_internal_t mesh_internal = this->internal->meshes[i];
		shader_internal_t shader_internal = this->internal->shaders[mesh_internal.mesh->shader];

		if (mesh_internal.size == U32_MAX) {
			continue;
		}

		glUseProgram(shader_internal.program);
		glBindVertexArray(shader_internal.vao);
		glBindBuffer(GL_ARRAY_BUFFER, shader_internal.vbo);
		try {
			this->shader_uniform(mesh_internal.mesh->shader, "unif_material_color", &mesh_internal.mesh->material.r, sizeof(f32) * 4);
		} catch (std::runtime_error e) {
			break;
		}
		try {
			this->shader_uniform(mesh_internal.mesh->shader, "unif_mvp", &this->camera->vp_matrix[0][0], sizeof(f32) * 16);
		} catch (std::runtime_error e) {
			break;
		}

		glDrawArrays(GL_TRIANGLES, mesh_internal.index, mesh_internal.size);
	}
}
