#version 410 core
in vec2 v_uv;
out vec4 out_color;
uniform vec4 unif_material_color;
uniform sampler2D unif_texture;
void main() {
	out_color = vec4(unif_material_color);
}