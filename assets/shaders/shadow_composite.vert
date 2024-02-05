#version 410 core

layout (location = 0) in vec3 in_pos;

out vec4 f_pos_light_space;

uniform mat4 unif_model;
uniform mat4 unif_light_vp;
uniform mat4 unif_vp;

void main() {
	f_pos_light_space = unif_light_vp * unif_model * vec4(in_pos, 1.0);
	gl_Position = unif_vp * unif_model * vec4(in_pos, 1.0);
}