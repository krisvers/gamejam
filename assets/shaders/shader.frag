#version 410 core

in vec2 v_uv;
out vec4 out_color;

uniform vec4 unif_material_color;
uniform sampler2D unif_texture_albedo;

void main() {
	out_color = unif_material_color * texture(unif_texture_albedo, v_uv);
}
