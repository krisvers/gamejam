#version 410 core

layout (location = 0) in vec3 in_pos;

uniform mat4 unif_light_vp;
uniform mat4 unif_model;

void main() {
	gl_Position = unif_light_vp * unif_model * vec4(in_pos, 1.0);
}