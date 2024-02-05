#define _USE_MATH_DEFINES
#include "renderer.hpp"
#include <glad/glad.h>
#include <linmath.h>
#include <iostream>
#include <fstream>
#include <exception>
#include <cmath>

struct mesh_internal_t {
	mesh_t* mesh;
	u32 vindex;
	u32 vcount;
	u32 iindex;
	u32 icount;
};

struct light_internal_t {
	light_t* light;
};

struct texture_internal_t {
	u32 gl;
};

struct gbuffer_t {
	u32 framebuffer;
	u32 geometry;
	u32 normal;
	u32 albedo_specular;
	u32 depth;
	u32 shadows;
	shader_t light_pass;
	u32 quad_vbo;
};

struct shadow_map_t {
	u32 framebuffer;
	u32 texture;
	u32 width;
	u32 height;
	shader_t depth_shader;
	shader_t shadow_composite;
};

#define SHADER_VERTEX_PREALLOCATION_DEFAULT 1024
#define SHADER_INDEX_PREALLOCATION_DEFAULT 1024

struct shader_internal_t {
	shader_t shader;
	u32 vertex_size;
	GLuint program;
	GLuint vao;
	GLuint vbo;
	GLuint ibo;
	u32 vbuffer_size;
	u32 ibuffer_size;
	u32 vbuffer_capacity;
	u32 ibuffer_capacity;

	std::vector<shader_input_t> inputs;
	std::vector<shader_uniform_t> uniforms;
	std::vector<shader_texture_attachment_t> texture_attachments;
};

struct renderer_internal_t {
	std::vector<mesh_internal_t> meshes;
	std::vector<shader_internal_t> shaders;
	std::vector<texture_internal_t> textures;
	std::vector<light_internal_t> lights;
	gbuffer_t gbuffer;
	shadow_map_t shadow_map;
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

renderer_c::renderer_c(GLFWwindow* window, camera_c& camera) : camera(camera) {
	this->window = window;
	glfwMakeContextCurrent(window);
	if (gladLoadGLLoader((GLADloadproc) glfwGetProcAddress) == 0) {
		throw std::runtime_error("Failed to initialize GLAD");
	}

	this->internal = new renderer_internal_t;
	
	shader_stage_t vshader = create_shader_stage(shader_stage_type::VERTEX, "assets/shaders/default.vert");
	shader_stage_t fshader = create_shader_stage(shader_stage_type::FRAGMENT, "assets/shaders/default.frag");

	shader_descriptor_t desc = {
		.stages = {
			shader_stage_type::VERTEX,
			shader_stage_type::FRAGMENT,
		},
		.starting_stage = shader_stage_type::VERTEX,
		.inputs = {
			{ shader_data_type::F32, 3 },
			{ shader_data_type::F32, 2 },
			{ shader_data_type::F32, 3 },
		},
		.uniforms = {
			{ shader_data_type::F32, 3, "unif_material_color" },
			{ shader_data_type::TEXTURE, 1, "unif_texture_albedo" },
			{ shader_data_type::TEXTURE, 1, "unif_texture_normal" },
			{ shader_data_type::TEXTURE, 1, "unif_texture_specular" },
			{ shader_data_type::MAT4x4, 1, "unif_mvp" },
		},
		.texture_attachments = {
			{ shader_texture_attachment_type::ALBEDO, "unif_texture_albedo" },
			{ shader_texture_attachment_type::NORMAL, "unif_texture_normal" },
			{ shader_texture_attachment_type::SPECULAR, "unif_texture_specular" },
		},
	};

	std::vector<shader_stage_t> stages = {
		vshader,
		fshader,
	};

	create_shader(desc, stages);

	shader_stage_t vlight_pass = create_shader_stage(shader_stage_type::VERTEX, "assets/shaders/default_light.vert");
	shader_stage_t flight_pass = create_shader_stage(shader_stage_type::FRAGMENT, "assets/shaders/default_light.frag");

	shader_descriptor_t light_pass_desc = {
		.stages = {
			shader_stage_type::VERTEX,
			shader_stage_type::FRAGMENT,
		},
		.starting_stage = shader_stage_type::VERTEX,
		.inputs = {
			{ shader_data_type::F32, 2 },
		},
		.uniforms = {
			{ shader_data_type::TEXTURE, 1, "unif_gbuffer_geometry" },
			{ shader_data_type::TEXTURE, 1, "unif_gbuffer_normal" },
			{ shader_data_type::TEXTURE, 1, "unif_gbuffer_albedo_specular" },
			{ shader_data_type::TEXTURE, 1, "unif_gbuffer_shadows" },
			{ shader_data_type::F32, 2, "unif_screen" },
		},
		.texture_attachments = {
			{ shader_texture_attachment_type::UNKNOWN, "unif_gbuffer_geometry" },
			{ shader_texture_attachment_type::NORMAL, "unif_gbuffer_normal" },
			{ shader_texture_attachment_type::UNKNOWN, "unif_gbuffer_albedo_specular" },
		},
	};

	std::vector<shader_stage_t> light_pass_stages = {
		vlight_pass,
		flight_pass,
	};

	this->internal->gbuffer.light_pass = create_shader(light_pass_desc, light_pass_stages);

	glFrontFace(GL_CCW);
	glCullFace(GL_BACK);
	glEnable(GL_DEPTH_TEST);

	glGenFramebuffers(1, &this->internal->gbuffer.framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, this->internal->gbuffer.framebuffer);

	int w, h;
	glfwGetWindowSize(window, &w, &h);

	glGenTextures(5, &this->internal->gbuffer.geometry);
	glBindTexture(GL_TEXTURE_2D, this->internal->gbuffer.geometry);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, this->internal->gbuffer.geometry, 0);
	
	glBindTexture(GL_TEXTURE_2D, this->internal->gbuffer.normal);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, this->internal->gbuffer.normal, 0);

	glBindTexture(GL_TEXTURE_2D, this->internal->gbuffer.albedo_specular);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, this->internal->gbuffer.albedo_specular, 0);

	glBindTexture(GL_TEXTURE_2D, this->internal->gbuffer.shadows);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, this->internal->gbuffer.shadows, 0);

	glBindTexture(GL_TEXTURE_2D, this->internal->gbuffer.depth);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, w, h, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, this->internal->gbuffer.depth, 0);

	u32 attachments[4] = {
		GL_COLOR_ATTACHMENT0,
		GL_COLOR_ATTACHMENT1,
		GL_COLOR_ATTACHMENT2,
		GL_COLOR_ATTACHMENT3,
	};
	glDrawBuffers(4, attachments);

	glGenVertexArrays(1, &this->internal->shaders[this->internal->gbuffer.light_pass].vao);
	glBindVertexArray(this->internal->shaders[this->internal->gbuffer.light_pass].vao);
	glGenBuffers(1, &this->internal->gbuffer.quad_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, this->internal->gbuffer.quad_vbo);
	float vertices[12] = {
		-1, -1,
		-1,  1,
		 1, -1,
		 1,  1,
	};
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*) 0);
	glEnableVertexAttribArray(0);
	glBindVertexArray(0);

	/* shadow map */
	shader_stage_t vshadow_depth_pass = create_shader_stage(shader_stage_type::VERTEX, "assets/shaders/shadow_depth.vert");
	shader_stage_t fshadow_depth_pass = create_shader_stage(shader_stage_type::FRAGMENT, "assets/shaders/shadow_depth.frag");

	shader_descriptor_t shadow_depth_pass_desc = {
		.stages = {
			shader_stage_type::VERTEX,
			shader_stage_type::FRAGMENT,
		},
		.starting_stage = shader_stage_type::VERTEX,
		.inputs = {
			{ shader_data_type::F32, 3 },
			{ shader_data_type::F32, 2 },
			{ shader_data_type::F32, 3 },
		},
		.uniforms = {
			{ shader_data_type::MAT4x4, 1, "unif_light_vp" },
			{ shader_data_type::MAT4x4, 1, "unif_model" },
		},
		.texture_attachments = {

		},
	};

	std::vector<shader_stage_t> shadow_depth_pass_stages = {
		vshadow_depth_pass,
		fshadow_depth_pass,
	};

	this->internal->shadow_map.depth_shader = create_shader(shadow_depth_pass_desc, shadow_depth_pass_stages);

	shader_stage_t vshadow_composite_pass = create_shader_stage(shader_stage_type::VERTEX, "assets/shaders/shadow_composite.vert");
	shader_stage_t fshadow_composite_pass = create_shader_stage(shader_stage_type::FRAGMENT, "assets/shaders/shadow_composite.frag");

	shader_descriptor_t shadow_composite_pass_desc = {
		.stages = {
			shader_stage_type::VERTEX,
			shader_stage_type::FRAGMENT,
		},
		.starting_stage = shader_stage_type::VERTEX,
		.inputs = {
			{ shader_data_type::F32, 3 },
			{ shader_data_type::F32, 2 },
			{ shader_data_type::F32, 3 },
		},
		.uniforms = {
			{ shader_data_type::MAT4x4, 1, "unif_light_vp" },
			{ shader_data_type::MAT4x4, 1, "unif_model" },
			{ shader_data_type::MAT4x4, 1, "unif_vp" },
			{ shader_data_type::TEXTURE, 1, "unif_shadow_depth" },
		},
		.texture_attachments = {

		},
	};

	std::vector<shader_stage_t> shadow_composite_pass_stages = {
		vshadow_composite_pass,
		fshadow_composite_pass,
	};

	this->internal->shadow_map.shadow_composite = create_shader(shadow_composite_pass_desc, shadow_composite_pass_stages);

	this->internal->shadow_map.width = 1024;
	this->internal->shadow_map.height = 1024;

	glGenFramebuffers(1, &this->internal->shadow_map.framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, this->internal->shadow_map.framebuffer);

	glGenTextures(1, &this->internal->shadow_map.texture);
	glBindTexture(GL_TEXTURE_2D, this->internal->shadow_map.texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, this->internal->shadow_map.width, this->internal->shadow_map.height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, this->internal->shadow_map.texture, 0);

	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
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
		std::string error = "Shader compilation error for ";
		error += filepath;
		error += ":\n";
		error += info_log;
		std::cerr << error;
		throw std::runtime_error(error);
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
		.vbuffer_size = 0,
		.ibuffer_size = 0,
		.vbuffer_capacity = SHADER_VERTEX_PREALLOCATION_DEFAULT,
		.ibuffer_capacity = SHADER_INDEX_PREALLOCATION_DEFAULT,
		.inputs = desc.inputs,
		.uniforms = desc.uniforms,
		.texture_attachments = desc.texture_attachments,
	};

	usize stride = 0;
	for (usize i = 0; i < desc.inputs.size(); i++) {
		stride += desc.inputs[i].size * shader_data_type_size(desc.inputs[i].type);
	}

	shader_internal.vertex_size = stride;

	glGenVertexArrays(1, &shader_internal.vao);
	glBindVertexArray(shader_internal.vao);

	glGenBuffers(1, &shader_internal.vbo);
	glBindBuffer(GL_ARRAY_BUFFER, shader_internal.vbo);
	glBufferData(GL_ARRAY_BUFFER, shader_internal.vbuffer_capacity * shader_internal.vertex_size, nullptr, GL_DYNAMIC_DRAW);

	glGenBuffers(1, &shader_internal.ibo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shader_internal.ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, shader_internal.ibuffer_capacity * sizeof(u32), nullptr, GL_DYNAMIC_DRAW);

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
		std::cerr << info_log << '\n';
		throw std::runtime_error(info_log);
	}

	for (usize i = 0; i < stages.size(); i++) {
		glDetachShader(shader_internal.program, stages[i]);
	}

	for (usize u = 0; u < desc.uniforms.size(); u++) {
		if (!internal_shader_uniform_exists(shader_internal, desc.uniforms[u].name)) {
			std::string error = "Uniform not found ";
			error += desc.uniforms[u].name;
			std::cerr << error << '\n';
			throw new std::runtime_error(error);
		}
	}

	for (usize u = 0; u < desc.texture_attachments.size(); u++) {
		if (!internal_shader_uniform_exists(shader_internal, desc.texture_attachments[u].associated_uniform)) {
			std::string error = "Texture attachment associated uniform not found ";
			error += desc.uniforms[u].name;
			std::cerr << error << '\n';
			throw new std::runtime_error(error);
		}
		for (usize i = 0; i < desc.uniforms.size(); i++) {
			if (std::strcmp(desc.texture_attachments[u].associated_uniform, desc.uniforms[i].name) == 0) {
				if (desc.uniforms[i].type != shader_data_type::TEXTURE) {
					std::string error = "Texture attachment associated uniform is not a texture ";
					error += desc.uniforms[i].name;
					std::cerr << error << '\n';
					throw new std::runtime_error(error);
				}
			}
		}
	}

	this->internal->shaders.push_back(shader_internal);
	return shader_internal.shader;
}

s32 renderer_c::shader_uniform(shader_t shader, const std::string& name, void* data, usize size) {
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

s32 renderer_c::shader_uniform_unsafe(shader_t shader, const std::string& name, void* data, usize size, shader_data_type type) {
	if (this->internal->shaders.size() <= shader) {
		return 1;
	}

	if (this->internal->shaders[shader].uniforms.size() <= 0) {
		return 2;
	}

	GLint location = glGetUniformLocation(this->internal->shaders[shader].program, name.c_str());
	if (location == -1) {
		return 5;
	}

	usize amount = size / shader_data_type_size(type);
	switch (type) {
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

void renderer_c::shader_use(shader_t shader) {
	if (this->internal->shaders.size() <= shader) {
		throw std::runtime_error("Shader does not exist");
	}

	glUseProgram(this->internal->shaders[shader].program);
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
		.vindex = 0,
		.vcount = 0,
		.iindex = 0,
		.icount = 0,
	};

	this->internal->meshes.push_back(mesh_internal);
	return mesh_internal.mesh;
}

void renderer_c::mesh_upload(mesh_t* mesh, void* vertex_data, usize vertex_bytesize, u32* index_data, usize index_bytesize) {
	if (this->internal->meshes.size() <= mesh->id) {
		throw std::runtime_error("Mesh does not exist");
	}

	mesh_internal_t& mesh_internal = this->internal->meshes[mesh->id];
	if (this->internal->shaders.size() <= mesh_internal.mesh->shader) {
		throw std::runtime_error("Shader does not exist");
	}

	shader_internal_t& shader_internal = this->internal->shaders[mesh_internal.mesh->shader];

	std::cout << "mehs :;\n";
	for (usize i = 0; i < vertex_bytesize / sizeof(float); ++i) {
		if (i % 8 == 0) {
			std::cout << '\n';
		}
		std::cout << reinterpret_cast<float*>(vertex_data)[i] << ' ';
	}

	usize vcount = vertex_bytesize / shader_internal.vertex_size;
	usize icount = index_bytesize / sizeof(u32);
	usize new_vsize = shader_internal.vbuffer_size + vcount;
	usize new_isize = shader_internal.ibuffer_size + icount;

	if (new_vsize < shader_internal.vbuffer_capacity) {
		glBindBuffer(GL_ARRAY_BUFFER, shader_internal.vbo);
		glBufferSubData(GL_ARRAY_BUFFER, shader_internal.vbuffer_size * shader_internal.vertex_size, vertex_bytesize, vertex_data);
	} else {
		std::string error = "Not implemented yet (resizing vbuffer) ";
		error += __FILE__;
		error += ":";
		error += __LINE__;
		std::cerr << error << '\n';
		throw new std::runtime_error(error);
	}

	u32* ibo_buffer = new u32[icount];
	for (usize i = 0; i < icount; i++) {
		ibo_buffer[i] = index_data[i] + shader_internal.vbuffer_size;
	}

	if (new_isize < shader_internal.ibuffer_capacity) {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shader_internal.ibo);
		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, shader_internal.ibuffer_size * sizeof(u32), index_bytesize, ibo_buffer);
	} else {
		std::string error = "Not implemented yet (resizing ibuffer) ";
		error += __FILE__;
		error += ":";
		error += __LINE__;
		std::cerr << error << '\n';
		throw new std::runtime_error(error);
	}

	delete ibo_buffer;

	mesh_internal.vcount = vcount;
	mesh_internal.vindex = shader_internal.vbuffer_size;
	mesh_internal.icount = icount;
	mesh_internal.iindex = shader_internal.ibuffer_size;

	shader_internal.vbuffer_size = new_vsize;
	shader_internal.ibuffer_size = new_isize;
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

light_t* renderer_c::create_light(vec3 position, vec3 color, f32 intensity) {
	light_internal_t light_internal = {
		.light = new light_t {
			.id = static_cast<u32>(this->internal->lights.size()),
			.intensity = intensity,
		}
	};
	vec3_dup(light_internal.light->position, position);
	vec3_dup(light_internal.light->color, color);

	this->internal->lights.push_back(light_internal);
	return light_internal.light;
}

void renderer_c::draw() {
	this->camera.calculate_matrices();

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);

	/* geometry pass */
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, this->internal->gbuffer.framebuffer);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	for (usize i = 0; i < this->internal->meshes.size(); i++) {
		mesh_internal_t mesh_internal = this->internal->meshes[i];
		shader_internal_t shader_internal = this->internal->shaders[mesh_internal.mesh->shader];

		if (mesh_internal.vcount == 0 || mesh_internal.vindex + mesh_internal.vcount > shader_internal.vbuffer_capacity) {
			continue;
		}

		if (mesh_internal.icount == 0 || mesh_internal.iindex + mesh_internal.icount > shader_internal.ibuffer_capacity) {
			continue;
		}

		shader_use(shader_internal.shader);

		mat4x4 m, r, mvp;
		mat4x4_translate(m, mesh_internal.mesh->transform.position[0], mesh_internal.mesh->transform.position[1], mesh_internal.mesh->transform.position[2]);
		mat4x4_identity(r);
		mat4x4_rotate_X(r, r, mesh_internal.mesh->transform.rotation[0] * (M_PI / 180.0));
		mat4x4_rotate_Y(r, r, mesh_internal.mesh->transform.rotation[1] * (M_PI / 180.0));
		mat4x4_rotate_Z(r, r, mesh_internal.mesh->transform.rotation[2] * (M_PI / 180.0));
		shader_uniform_unsafe(mesh_internal.mesh->shader, "unif_model_rotation", &r, sizeof(f32) * 16, shader_data_type::MAT4x4);
		mat4x4_mul(m, m, r);
		mat4x4_scale_aniso(m, m, mesh_internal.mesh->transform.scale[0], mesh_internal.mesh->transform.scale[1], mesh_internal.mesh->transform.scale[2]);

		mat4x4_mul(mvp, this->camera.vp_matrix, m);
		shader_uniform_unsafe(mesh_internal.mesh->shader, "unif_mvp", &mvp, sizeof(f32) * 16, shader_data_type::MAT4x4);
		shader_uniform_unsafe(mesh_internal.mesh->shader, "unif_model", &m, sizeof(f32) * 16, shader_data_type::MAT4x4);
		shader_uniform(mesh_internal.mesh->shader, "unif_material_color", &mesh_internal.mesh->material.r, sizeof(f32) * 3);

		for (usize j = 0; j < shader_internal.texture_attachments.size(); j++) {
			if (j >= mesh_internal.mesh->material.textures.size()) {
				s32 texture = -1;
				shader_uniform(mesh_internal.mesh->shader, shader_internal.texture_attachments[j].associated_uniform, &texture, sizeof(s32));
				continue;
			}

			texture_internal_t texture_internal = this->internal->textures[mesh_internal.mesh->material.textures[j] - 1];
			glActiveTexture(GL_TEXTURE0 + j);
			glBindTexture(GL_TEXTURE_2D, texture_internal.gl);
			shader_uniform(mesh_internal.mesh->shader, shader_internal.texture_attachments[j].associated_uniform, &j, sizeof(s32));
		}

		glBindVertexArray(shader_internal.vao);
		glBindBuffer(GL_ARRAY_BUFFER, shader_internal.vbo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shader_internal.ibo);
		std::cout << "mesh couthns:\n" << (GLsizei) mesh_internal.icount << " " << (const void*) mesh_internal.iindex << '\n';
		glDrawElements(GL_TRIANGLES, mesh_internal.icount, GL_UNSIGNED_INT, (const void *) (mesh_internal.iindex));
	}

	/* shadow depth and shade texture pass */
	{
		glClear(GL_DEPTH_BUFFER_BIT);
		glViewport(0, 0, this->internal->shadow_map.width, this->internal->shadow_map.height);
		shader_use(this->internal->shadow_map.depth_shader);
		for (usize i = 0; i < this->internal->meshes.size(); i++) {
			mesh_internal_t mesh_internal = this->internal->meshes[i];
			shader_internal_t shader_internal = this->internal->shaders[mesh_internal.mesh->shader];

			for (usize j = 0; j < this->internal->lights.size(); ++j) {
				mat4x4 light_proj;
				mat4x4_perspective(light_proj, 45.0f, 1.0f, 0.1f, 25.0f * this->internal->lights[j].light->intensity);

				mat4x4 light_view;
				vec3 center = { 0, 1, 0 };
				vec3 up = { 0, 1, 0 };
				mat4x4_look_at(light_view, this->internal->lights[j].light->position, center, up);

				mat4x4 light_vp;
				mat4x4_mul(light_vp, light_proj, light_view);

				mat4x4 m;
				mat4x4_translate(m, mesh_internal.mesh->transform.position[0], mesh_internal.mesh->transform.position[1], mesh_internal.mesh->transform.position[2]);
				mat4x4_rotate_Y(m, m, mesh_internal.mesh->transform.rotation[1] * (M_PI / 180.0));
				mat4x4_rotate_Z(m, m, mesh_internal.mesh->transform.rotation[2] * (M_PI / 180.0));
				mat4x4_rotate_X(m, m, mesh_internal.mesh->transform.rotation[0] * (M_PI / 180.0));
				mat4x4_scale_aniso(m, m, mesh_internal.mesh->transform.scale[0], mesh_internal.mesh->transform.scale[1], mesh_internal.mesh->transform.scale[2]);

				shader_uniform(this->internal->shadow_map.depth_shader, "unif_light_vp", &light_vp, sizeof(f32) * 16);
				shader_uniform(this->internal->shadow_map.depth_shader, "unif_model", &m, sizeof(f32) * 16);

				glBindVertexArray(shader_internal.vao);
				glBindBuffer(GL_ARRAY_BUFFER, shader_internal.vbo);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shader_internal.ibo);
				glDrawElements(GL_TRIANGLES, mesh_internal.icount, GL_UNSIGNED_INT, (void*) (mesh_internal.iindex));
			}
		}

		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, this->internal->gbuffer.framebuffer);
		glDepthMask(GL_FALSE);
		shader_use(this->internal->shadow_map.shadow_composite);
		for (usize i = 0; i < this->internal->meshes.size(); i++) {
			mesh_internal_t mesh_internal = this->internal->meshes[i];
			shader_internal_t shader_internal = this->internal->shaders[mesh_internal.mesh->shader];

			for (usize j = 0; j < this->internal->lights.size(); ++j) {
				mat4x4 light_proj;
				mat4x4_perspective(light_proj, 45.0f, 1.0f, 0.1f, 25.0f * this->internal->lights[j].light->intensity);

				mat4x4 light_view;
				vec3 center = { 0, 1, 0 };
				vec3 up = { 0, 1, 0 };
				mat4x4_look_at(light_view, this->internal->lights[j].light->position, center, up);

				mat4x4 light_vp;
				mat4x4_mul(light_vp, light_proj, light_view);

				mat4x4 m;
				mat4x4_translate(m, mesh_internal.mesh->transform.position[0], mesh_internal.mesh->transform.position[1], mesh_internal.mesh->transform.position[2]);
				mat4x4_rotate_Y(m, m, mesh_internal.mesh->transform.rotation[1] * (M_PI / 180.0));
				mat4x4_rotate_Z(m, m, mesh_internal.mesh->transform.rotation[2] * (M_PI / 180.0));
				mat4x4_rotate_X(m, m, mesh_internal.mesh->transform.rotation[0] * (M_PI / 180.0));
				mat4x4_scale_aniso(m, m, mesh_internal.mesh->transform.scale[0], mesh_internal.mesh->transform.scale[1], mesh_internal.mesh->transform.scale[2]);

				shader_uniform(this->internal->shadow_map.shadow_composite, "unif_light_vp", &light_vp, sizeof(f32) * 16);
				shader_uniform(this->internal->shadow_map.shadow_composite, "unif_model", &m, sizeof(f32) * 16);
				shader_uniform(this->internal->shadow_map.shadow_composite, "unif_vp", &this->camera.vp_matrix, sizeof(f32) * 16);

				s32 texture = 0;
				glActiveTexture(GL_TEXTURE0 + texture);
				glBindTexture(GL_TEXTURE_2D, this->internal->shadow_map.texture);
				shader_uniform(this->internal->shadow_map.shadow_composite, "unif_shadow_depth", &texture, sizeof(s32));

				glBindVertexArray(shader_internal.vao);
				glBindBuffer(GL_ARRAY_BUFFER, shader_internal.vbo);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shader_internal.ibo);
				glDrawElements(GL_TRIANGLES, mesh_internal.icount, GL_UNSIGNED_INT, (void*) (mesh_internal.iindex));
			}
		}
	}

	int w, h;
	glfwGetWindowSize(this->window, &w, &h);
	glViewport(0, 0, w, h);
	/* light/shadow pass */
	{
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, this->internal->gbuffer.framebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		shader_use(this->internal->gbuffer.light_pass);
		vec2 screen = { static_cast<f32>(w), static_cast<f32>(h) };
		shader_uniform(this->internal->gbuffer.light_pass, "unif_screen", &screen, sizeof(f32) * 2);
		s32 texture = 0;
		glActiveTexture(GL_TEXTURE0 + texture);
		glBindTexture(GL_TEXTURE_2D, this->internal->gbuffer.geometry);
		shader_uniform(this->internal->gbuffer.light_pass, "unif_gbuffer_geometry", &texture, sizeof(texture));
		texture = 1;
		glActiveTexture(GL_TEXTURE0 + texture);
		glBindTexture(GL_TEXTURE_2D, this->internal->gbuffer.normal);
		shader_uniform(this->internal->gbuffer.light_pass, "unif_gbuffer_normal", &texture, sizeof(texture));
		texture = 2;
		glActiveTexture(GL_TEXTURE0 + texture);
		glBindTexture(GL_TEXTURE_2D, this->internal->gbuffer.albedo_specular);
		shader_uniform(this->internal->gbuffer.light_pass, "unif_gbuffer_albedo_specular", &texture, sizeof(texture));
		texture = 3;
		glActiveTexture(GL_TEXTURE0 + texture);
		glBindTexture(GL_TEXTURE_2D, this->internal->gbuffer.shadows);
		shader_uniform(this->internal->gbuffer.light_pass, "unif_gbuffer_shadows", &texture, sizeof(texture));
		shader_uniform_unsafe(this->internal->gbuffer.light_pass, "unif_view_pos", &this->camera.transform.position, sizeof(vec3), shader_data_type::F32);
		float time = glfwGetTime();
		shader_uniform_unsafe(this->internal->gbuffer.light_pass, "unif_time", &time, sizeof(f32), shader_data_type::F32);

		glBindVertexArray(this->internal->shaders[this->internal->gbuffer.light_pass].vao);
		glBindBuffer(GL_ARRAY_BUFFER, this->internal->gbuffer.quad_vbo);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	}
}
