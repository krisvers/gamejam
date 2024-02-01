#ifndef CAMERA_HPP
#define CAMERA_HPP

#include "types.hpp"
#include "utils.hpp"
#include <linmath.h>

struct rect_t {
	float left;
	float right;
	float top;
	float bottom;
};

struct camera_c {
	transform_t transform;

	float fov;
	float near;
	float far;
	float aspect;

	b8 is_ortho;
	
	mat4x4 view_matrix;
	mat4x4 perspective_matrix;
	mat4x4 vp_matrix;

	/* perspective */
	camera_c(float fov, float near, float far, float aspect);
	/* orthogonal */
	camera_c(float near, float far, float aspect);
	void calculate_matrices();
};

#endif
