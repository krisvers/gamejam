#version 410 core

layout (location = 0) out vec3 out_geometry;
layout (location = 1) out vec3 out_normal;
layout (location = 2) out vec4 out_albedo_specular;

in vec3 v_pos;
in vec2 v_uv;
in vec3 v_normal;

uniform mat4 unif_model_rotation;
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

	out_normal = normalize(texture(unif_texture_normal, v_uv).xyz + v_normal);
	out_albedo_specular = vec4(unif_material_color * vec3(albedo), texture(unif_texture_specular, v_uv).r);
}