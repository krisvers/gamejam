#version 410 core

layout (location = 3) out float out_shade;

in vec4 f_pos_light_space;

uniform sampler2D unif_shadow_depth;

void main() {
	vec3 proj_coords = f_pos_light_space.xyz / f_pos_light_space.w;
	proj_coords = (proj_coords + 1.0) / 2.0;
	float closest_depth = texture(unif_shadow_depth, proj_coords.xy).r;
	float current_depth = proj_coords.z;

	if (current_depth > closest_depth) {
		out_shade = 1.0;
	} else {
		out_shade = 0.0;
	}
}