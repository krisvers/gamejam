#version 410 core

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec2 in_uv;

out vec2 v_uv;

uniform mat4 unif_mvp;

void main() {
	gl_Position = unif_mvp * vec4(in_pos, 1.0);
	v_uv = in_uv.xy;
}
