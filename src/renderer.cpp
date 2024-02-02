#include "renderer.hpp"
#include <glad/glad.h>
#include <linmath.h>
#include <iostream>
#include <fstream>
#include <exception>
#include <filesystem>

struct mesh_internal_t {
	mesh_t* mesh;
	u32 index;
	u32 size;
};

struct texture_internal_t {
	u32 gl;
};

struct gbuffer_t {
	u32 framebuffer;
	u32 geometry;
	u32 albedo_specular;
	u32 normal;
	shader_t light_pass;
};

struct shader_internal_t {
	shader_t shader;
	u32 vertex_size;
	GLuint program;
	GLuint vao;
	GLuint vbo;
	u32 buffer_size;

	std::vector<shader_uniform_t> uniforms;
	std::vector<shader_texture_attachment_t> texture_attachments;
};

struct renderer_internal_t {
	std::vector<mesh_internal_t> meshes;
	std::vector<shader_internal_t> shaders;
	std::vector<texture_internal_t> textures;
	gbuffer_t gbuffer;
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

inline const GLenum texture_format_to_gl(texture_format format) {
	switch (format) {
	case texture_format::RGBA:
		return GL_RGBA;
	case texture_format::BGRA:
		return GL_BGRA;
	case texture_format::RGB:
		return GL_RGB;
	case texture_format::BGR:
		return GL_BGR;
	default:
		throw std::runtime_error("Invalid texture format");
	}
}

inline const GLenum texture_filter_to_gl_mag(texture_filter filter) {
	switch (filter) {
	case texture_filter::NEAREST:
		return GL_NEAREST;
	case texture_filter::LINEAR:
		return GL_LINEAR;
	default:
		throw std::runtime_error("Invalid texture filter");
	}
}

inline const GLenum texture_filter_to_gl_min(texture_filter filter) {
	switch (filter) {
	case texture_filter::NEAREST:
		return GL_NEAREST_MIPMAP_NEAREST;
	case texture_filter::LINEAR:
		return GL_LINEAR_MIPMAP_LINEAR;
	default:
		throw std::runtime_error("Invalid texture filter");
	}
}

inline const GLenum texture_wrap_to_gl(texture_wrap filter) {
	switch (filter) {
	case texture_wrap::REPEAT:
		return GL_REPEAT;
	case texture_wrap::MIRRORED_REPEAT:
		return GL_MIRRORED_REPEAT;
	case texture_wrap::CLAMP_TO_EDGE:
		return GL_CLAMP_TO_EDGE;
	case texture_wrap::CLAMP_TO_BORDER:
		return GL_CLAMP_TO_BORDER;
	default:
		throw std::runtime_error("Invalid texture wrap");
	}
}

static b8 internal_shader_uniform_exists(const shader_internal_t & shader, const std::string & name);

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
		layout (location = 2) in vec3 in_normal;

		out vec3 v_pos;
		out vec2 v_uv;
		out vec3 v_normal;

		uniform mat4 unif_mvp;

		void main() {
			gl_Position = unif_mvp * vec4(in_pos, 1.0);
			v_pos = gl_Position.xyz;
			v_uv = in_uv.xy;
			v_normal = in_normal;
		}
	)";

	const char* fshader_source = R"(
		#version 410 core

		layout (location = 0) out vec3 out_geometry;
		layout (location = 1) out vec3 out_normal;
		layout (location = 2) out vec4 out_albedo_specular;

		in vec3 v_pos;
		in vec2 v_uv;
		in vec3 v_normal;
		
		uniform vec3 unif_material_color;
		uniform sampler2D unif_texture_albedo;
		uniform sampler2D unif_texture_normal;
		uniform sampler2D unif_texture_specular;

		void main() {
			out_geometry = v_pos;
			vec4 albedo = texture(unif_texture_albedo, v_uv);
			if (albedo.a == 0.0) {
				albedo = vec4(1.0, 1.0, 1.0, 1.0);
			}

			out_albedo_specular = vec4(unif_material_color * vec3(albedo), texture(unif_texture_specular, v_uv).r);
			out_normal = texture(unif_texture_normal, v_uv).rgb + v_normal;
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
			{ shader_data_type::F32, 3, "unif_material_color" },
			{ shader_data_type::TEXTURE, 1, "unif_texture_albedo" },
			{ shader_data_type::MAT4x4, 1, "unif_mvp" },
		},
		.texture_attachments = {
			{ shader_texture_attachment_type::ALBEDO, "unif_texture_albedo" },
		},
	};

	std::vector<shader_stage_t> stages = {
		vshader,
		fshader,
	};

	create_shader(desc, stages);

	const char* light_pass_source = R"(
		#version 410 core

		out vec4 out_color;

		uniform sampler2D unif_gbuffer_geometry;
		uniform sampler2D unif_gbuffer_normal;
		uniform sampler2D unif_gbuffer_albedo_specular;
		uniform vec2 unif_screen;

		void main() {
			out_color = vec4(texture(unif_gbuffer_albedo_specular, gl_FragCoord.xy / unif_screen).rgb, 1.0);
		}
	)";

	GLuint light_pass = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(light_pass, 1, &light_pass_source, NULL);
	glCompileShader(light_pass);
	glGetShaderiv(light_pass, GL_COMPILE_STATUS, &success);
	if (!success) {
		GLchar info_log[512];
		glGetShaderInfoLog(light_pass, 512, NULL, info_log);
		throw std::runtime_error(info_log);
	}

	desc = {
		.stages = {
			shader_stage_type::FRAGMENT,
		},
		.starting_stage = shader_stage_type::FRAGMENT,
		.inputs = {
			
		},
		.uniforms = {
			{ shader_data_type::MAT4x4, 1, "unif_mvp" },
			{ shader_data_type::MAT4x4, 1, "unif_gbuffer_geometry" },
			{ shader_data_type::MAT4x4, 1, "unif_gbuffer_normal" },
			{ shader_data_type::MAT4x4, 1, "unif_gbuffer_albedo_specular" },
			{ shader_data_type::MAT4x4, 1, "unif_screen" },
		},
		.texture_attachments = {  },
	};

	stages = {
		fshader,
	};

	this->internal->gbuffer.light_pass = create_shader(desc, stages);

	glGenFramebuffers(1, &this->internal->gbuffer.framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, this->internal->gbuffer.framebuffer);

	glGenTextures(3, &this->internal->gbuffer.geometry);
	glBindTexture(GL_TEXTURE_2D, this->internal->gbuffer.geometry);

	int w, h;
	glfwGetWindowSize(window, &w, &h);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, this->internal->gbuffer.geometry, 0);
	
	glBindTexture(GL_TEXTURE_2D, this->internal->gbuffer.albedo_specular);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, this->internal->gbuffer.albedo_specular, 0);
	
	glBindTexture(GL_TEXTURE_2D, this->internal->gbuffer.normal);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, this->internal->gbuffer.normal, 0);
}

renderer_c::~renderer_c() {
	for (usize i = 0; i < this->internal->meshes.size(); ++i) {
		delete this->internal->meshes[i].mesh;
	}

	for (usize i = 0; i < this->internal->shaders.size(); ++i) {
		glDeleteShader(this->internal->shaders[i].program);
		glDeleteVertexArrays(1, &this->internal->shaders[i].vao);
		glDeleteBuffers(1, &this->internal->shaders[i].vbo);
	}

	for (usize i = 0; i < this->internal->textures.size(); ++i) {
		glDeleteTextures(1, &this->internal->textures[i].gl);
	}
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

void renderer_c::destroy_shader_stage(shader_stage_t shader) {
	glDeleteShader(shader);
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
		.texture_attachments = desc.texture_attachments,
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

	for (usize i = 0; i < stages.size(); i++) {
		glDetachShader(shader_internal.program, stages[i]);
	}

	for (usize u = 0; u < desc.uniforms.size(); u++) {
		if (!internal_shader_uniform_exists(shader_internal, desc.uniforms[u].name)) {
			std::string error = "Uniform not found ";
			error += desc.uniforms[u].name;
			throw new std::runtime_error(error);
		}
	}

	for (usize u = 0; u < desc.texture_attachments.size(); u++) {
		if (!internal_shader_uniform_exists(shader_internal, desc.texture_attachments[u].associated_uniform)) {
			std::string error = "Texture attachment associated uniform not found ";
			error += desc.uniforms[u].name;
			throw new std::runtime_error(error);
		}
		for (usize i = 0; i < desc.uniforms.size(); i++) {
			if (std::strcmp(desc.texture_attachments[u].associated_uniform, desc.uniforms[i].name) == 0) {
				if (desc.uniforms[i].type != shader_data_type::TEXTURE) {
					std::string error = "Texture attachment associated uniform is not a texture ";
					error += desc.uniforms[i].name;
					throw new std::runtime_error(error);
				}
			}
		}
	}

	this->internal->shaders.push_back(shader_internal);
	return shader_internal.shader;
}

int renderer_c::shader_uniform(shader_t shader, const std::string& name, void* data, usize size) {
	if (this->internal->shaders.size() <= shader) {
		return 1;
	}

	if (this->internal->shaders[shader].uniforms.size() <= 0) {
		return 2;
	}

	usize uniform = USIZE_MAX;
	for (usize i = 0; i < this->internal->shaders[shader].uniforms.size(); i++) {
		if (name.compare(this->internal->shaders[shader].uniforms[i].name) == 0) {
			uniform = i;
			break;
		}
	}

	if (uniform == USIZE_MAX) {
		return 3;
	}

	if (size != this->internal->shaders[shader].uniforms[uniform].size * shader_data_type_size(this->internal->shaders[shader].uniforms[uniform].type)) {
		return 4;
	}

	GLint location = glGetUniformLocation(this->internal->shaders[shader].program, name.c_str());
	if (location == -1) {
		return 5;
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
		return 6;
	}

	return 0;
}

static b8 internal_shader_uniform_exists(const shader_internal_t& shader, const std::string& name) {
	if (shader.uniforms.size() <= 0) {
		return false;
	}

	usize uniform = USIZE_MAX;
	for (usize i = 0; i < shader.uniforms.size(); i++) {
		if (name.compare(shader.uniforms[i].name) == 0) {
			s32 location = glGetUniformLocation(shader.program, name.c_str());
			if (location == -1) {
				return false;
			} else {
				return true;
			}
		}
	}

	return false;
}

b8 renderer_c::shader_uniform_exists(shader_t shader, const std::string & name) {
	if (this->internal->shaders.size() <= shader) {
		return false;
	}

	return internal_shader_uniform_exists(this->internal->shaders[shader], name);
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

texture_t renderer_c::create_texture(const texture_descriptor_t & desc, void* data, usize bytesize) {
	texture_internal_t texture_internal = {
		.gl = 0,
	};

	glGenTextures(1, &texture_internal.gl);
	glBindTexture(GL_TEXTURE_2D, texture_internal.gl);
	
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, texture_filter_to_gl_min(desc.filter));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, texture_filter_to_gl_mag(desc.filter));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, texture_wrap_to_gl(desc.wrap));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, texture_wrap_to_gl(desc.wrap));

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, desc.width, desc.height, 0, texture_format_to_gl(desc.format), GL_UNSIGNED_BYTE, data);
	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);

	this->internal->textures.push_back(texture_internal);
	return static_cast<texture_t>(this->internal->textures.size());
}

void renderer_c::draw() {
	this->camera->calculate_matrices();

	u32 attachments[3] = {
		GL_COLOR_ATTACHMENT0,
		GL_COLOR_ATTACHMENT1,
		GL_COLOR_ATTACHMENT2,
	};
	for (usize i = 0; i < this->internal->meshes.size(); i++) {
		mesh_internal_t mesh_internal = this->internal->meshes[i];
		shader_internal_t shader_internal = this->internal->shaders[mesh_internal.mesh->shader];

		if (mesh_internal.size == U32_MAX) {
			continue;
		}

		glUseProgram(shader_internal.program);
		glBindVertexArray(shader_internal.vao);
		glBindBuffer(GL_ARRAY_BUFFER, shader_internal.vbo);

		this->shader_uniform(mesh_internal.mesh->shader, "unif_material_color", &mesh_internal.mesh->material.r, sizeof(f32) * 3);
		this->shader_uniform(mesh_internal.mesh->shader, "unif_mvp", &this->camera->vp_matrix[0][0], sizeof(f32) * 16);

		for (usize j = 0; j < shader_internal.texture_attachments.size(); j++) {
			if (j >= mesh_internal.mesh->material.textures.size()) {
				glActiveTexture(GL_TEXTURE0 + j);
				glBindTexture(GL_TEXTURE_2D, 0);
				s32 texture = -1;
				this->shader_uniform(mesh_internal.mesh->shader, shader_internal.texture_attachments[j].associated_uniform, &texture, sizeof(s32));
				continue;
			}

			texture_internal_t texture_internal = this->internal->textures[mesh_internal.mesh->material.textures[j] - 1];
			glActiveTexture(GL_TEXTURE0 + j);
			glBindTexture(GL_TEXTURE_2D, texture_internal.gl);
			this->shader_uniform(mesh_internal.mesh->shader, shader_internal.texture_attachments[j].associated_uniform, &j, sizeof(s32));
		}

		glDrawArrays(GL_TRIANGLES, mesh_internal.index, mesh_internal.size);
	}

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, this->internal->gbuffer.geometry);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, this->internal->gbuffer.normal);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, this->internal->gbuffer.albedo_specular);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glUseProgram(this->internal->gbuffer.light_pass);
	int w, h;
	glfwGetWindowSize(this->window, &w, &h);
	vec2 screen = { static_cast<f32>(w), static_cast<f32>(h) };
	shader_uniform(this->internal->gbuffer.light_pass, "unif_screen", &screen, sizeof(screen));
	s32 texture = 0;
	shader_uniform(this->internal->gbuffer.light_pass, "unif_gbuffer_geometry", &texture, sizeof(texture));
	texture = 1;
	shader_uniform(this->internal->gbuffer.light_pass, "unif_gbuffer_normal", &texture, sizeof(texture));
	texture = 2;
	shader_uniform(this->internal->gbuffer.light_pass, "unif_gbuffer_albedo_specular", &texture, sizeof(texture));
	glDrawBuffers(3, attachments);
}
