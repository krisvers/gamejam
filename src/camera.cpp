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

	this->transform.x = 0;
	this->transform.y = 0;
	this->transform.z = 0;
	this->transform.rx = 0;
	this->transform.ry = 0;
	this->transform.rz = 0;
}

camera_c::camera_c(float near, float far, float aspect) {
	this->near = near;
	this->far = far;
	this->aspect = aspect;
	this->is_ortho = true;

	this->transform.x = 0;
	this->transform.y = 0;
	this->transform.z = 0;
	this->transform.rx = 0;
	this->transform.ry = 0;
	this->transform.rz = 0;
}

void camera_c::calculate_matrices() {
	std::memset(this->view_matrix, 0, sizeof(f32) * 16);
	std::memset(this->perspective_matrix, 0, sizeof(f32) * 16);

	mat4x4_translate(this->view_matrix, -this->transform.x, -this->transform.y, -this->transform.z);

	if (this->is_ortho) {
		mat4x4_ortho(this->perspective_matrix, -this->aspect, this->aspect, -1, 1, -this->far, this->far);
	} else {
		mat4x4_perspective(this->perspective_matrix, this->fov * (M_PI / 180.0f), this->aspect, this->near, this->far);
	}

	mat4x4_mul(this->vp_matrix, this->perspective_matrix, this->view_matrix);
}
