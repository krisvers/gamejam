#version 410 core

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec2 in_uv;
layout (location = 2) in vec3 in_normal;

out vec3 v_pos;
out vec2 v_uv;
out vec3 v_normal;

uniform mat4 unif_mvp;
uniform mat4 unif_model;

void main() {
	gl_Position = unif_mvp * vec4(in_pos, 1.0);
	v_pos = vec3(unif_model * vec4(in_pos, 1.0));
	v_uv = in_uv;
	v_normal = vec3(unif_model * vec4(in_normal, 1.0));
}