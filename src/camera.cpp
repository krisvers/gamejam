#include "camera.hpp"
#include <linmath.h>
#define _USE_MATH_DEFINES
#include <cmath>
#include <cstring>

camera_c::camera_c(float fov, float near, float far, float aspect) {
	this->fov = fov;
	this->near = near;
	this->far = far;
	this->aspect = aspect;
	this->is_ortho = false;

	this->transform.position[0] = 0;
	this->transform.position[1] = 0;
	this->transform.position[2] = 0;
	this->transform.rotation[0] = 0;
	this->transform.rotation[1] = 0;
	this->transform.rotation[2] = 0;
}

camera_c::camera_c(float near, float far, float aspect) {
	this->near = near;
	this->far = far;
	this->aspect = aspect;
	this->is_ortho = true;

	this->transform.position[0] = 0;
	this->transform.position[1] = 0;
	this->transform.position[2] = 0;
	this->transform.rotation[0] = 0;
	this->transform.rotation[1] = 0;
	this->transform.rotation[2] = 0;
}

void camera_c::calculate_matrices() {
	std::memset(this->view_matrix, 0, sizeof(f32) * 16);
	std::memset(this->perspective_matrix, 0, sizeof(f32) * 16);

	mat4x4_translate(this->view_matrix, -this->transform.position[0], -this->transform.position[1], -this->transform.position[2]);

	if (this->is_ortho) {
		mat4x4_ortho(this->perspective_matrix, -this->aspect, this->aspect, -1, 1, -this->far, this->far);
	} else {
		mat4x4_perspective(this->perspective_matrix, this->fov * (M_PI / 180.0f), this->aspect, this->near, this->far);
	}

	mat4x4_rotate_X(this->perspective_matrix, this->perspective_matrix, -this->transform.rotation[0] * (M_PI / 180.0f));
	mat4x4_rotate_Y(this->perspective_matrix, this->perspective_matrix, this->transform.rotation[1] * (M_PI / 180.0f));
	mat4x4_rotate_Z(this->perspective_matrix, this->perspective_matrix, -this->transform.rotation[2] * (M_PI / 180.0f));

	mat4x4_mul(this->vp_matrix, this->perspective_matrix, this->view_matrix);
}
