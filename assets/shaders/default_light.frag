#version 410 core

out vec4 out_color;

uniform sampler2D unif_gbuffer_geometry;
uniform sampler2D unif_gbuffer_normal;
uniform sampler2D unif_gbuffer_albedo_specular;
uniform sampler2D unif_gbuffer_shadows;
uniform vec2 unif_screen;
uniform vec3 unif_view_pos;
uniform float unif_time;

const float gamma = 2.2;
const float ambient = 0.2;
const float light_strength = 20;
const vec3 light_color = vec3(1.0, 1.0, 1.0);
const float default_shininess = 1000;

void main() {
	vec2 uv = gl_FragCoord.xy / unif_screen;
	vec3 position = texture(unif_gbuffer_geometry, uv).xyz;
	vec3 normal = texture(unif_gbuffer_normal, uv).rgb;
	vec3 albedo = texture(unif_gbuffer_albedo_specular, uv).rgb;
	float shininess = texture(unif_gbuffer_albedo_specular, uv).a;
	float shadow = texture(unif_gbuffer_shadows, uv).r;

	if (shininess == 0) {
		shininess = default_shininess;
	}

	vec3 light_pos = vec3(sin(unif_time), 1, cos(unif_time));

	vec3 light_dir = light_pos - position;
	float distance = length(light_dir);
	light_dir = normalize(light_dir);

	vec3 view_dir = normalize(light_pos - position);

	vec3 half_dir = normalize(light_dir + view_dir);
	float spec = pow(max(dot(normal, half_dir), 0.0), shininess);
	vec3 specular = spec * light_color;

	float diff = max(dot(normal, light_dir), 0.0);
	vec3 diffuse = diff * light_color;

	vec3 result = albedo * (ambient + (1.0 - shadow) * (diffuse + specular) * light_strength) / (distance * distance);
	if (unif_screen.x == 0) {
		result = diffuse + spec + position + normal + albedo + shadow + vec3(uv, 0.0) * shininess;
	}
	if (unif_screen.x > 800) {
		result = 1.0 - vec3(shadow, shadow, shadow);
	}

	out_color = vec4(pow(result, vec3(1.0 / gamma)), 1.0);
}