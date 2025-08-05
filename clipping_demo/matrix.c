/**********************************************************
Matrix
行列演算用関数
**********************************************************/
#include <kos.h>
#include "matrix.h"

static matrix_t scr_m __attribute__((aligned(32))) = {
	{1.0f, 0.0f, 0.0f, 0.0f},
	{0.0f, 1.0f, 0.0f, 0.0f},
	{0.0f, 0.0f, 1.0f, 0.0f},
	{0.0f, 0.0f, 0.0f, 1.0f}};

/**********************************************************
MatrixMulScreen
スクリーン行列
**********************************************************/
void matrix_mul_screen(float width, float height)
{
	float w = width * 0.5f;
	float h = height * 0.5f;

	scr_m[0][0] = w;
	scr_m[1][1] = h;
	scr_m[3][0] = w;
	scr_m[3][1] = h;

	mat_apply(&scr_m);
}

static matrix_t proj_m __attribute__((aligned(32))) = {
	{1.0f, 0.0f, 0.0f, 0.0f},
	{0.0f, 1.0f, 0.0f, 0.0f},
	{0.0f, 0.0f, 1.0f, 0.0f},
	{0.0f, 0.0f, 0.0f, 1.0f}};

/**********************************************************
MatrixMulProjection
プロジェクション行列
**********************************************************/
void matrix_mul_projection(float angle, float aspect)
{
	float s = 1.0 / ftan(angle);

	proj_m[0][0] = s / aspect;
	proj_m[1][1] = -s;
	proj_m[2][2] = 0.0f;
	proj_m[3][2] = 1.0f;
	proj_m[2][3] = -1.0f;
	proj_m[3][3] = 0.0f;

	mat_apply(&proj_m);
}

void vec3f_cross(const vec3f_t *v1, const vec3f_t *v2, vec3f_t *r)
{
	r->x = v1->y * v2->z - v1->z * v2->y;
	r->y = v1->z * v2->x - v1->x * v2->z;
	r->z = v1->x * v2->y - v1->y * v2->x;
}

static matrix_t view_m __attribute__((aligned(32))) = {
	{1.0f, 0.0f, 0.0f, 0.0f},
	{0.0f, 1.0f, 0.0f, 0.0f},
	{0.0f, 0.0f, 1.0f, 0.0f},
	{0.0f, 0.0f, 0.0f, 1.0f}};

/**********************************************************
MatrixMulView
ビュー行列
**********************************************************/
void matrix_mul_view(float *position, float *target, float *up)
{
	vec3f_t *p = (vec3f_t *)position;
	vec3f_t *t = (vec3f_t *)target;
	vec3f_t *u = (vec3f_t *)up;

	vec3f_t x, y, z;

	z.x = p->x - t->x;
	z.y = p->y - t->y;
	z.z = p->z - t->z;

	vec3f_normalize(z.x, z.y, z.z);
	vec3f_cross(u, &z, &x);
	vec3f_normalize(x.x, x.y, x.z);
	vec3f_cross(&z, &x, &y);
	vec3f_normalize(x.x, x.y, x.z);

	view_m[0][0] = x.x;
	view_m[1][0] = x.y;
	view_m[2][0] = x.z;
	view_m[0][1] = y.x;
	view_m[1][1] = y.y;
	view_m[2][1] = y.z;
	view_m[0][2] = z.x;
	view_m[1][2] = z.y;
	view_m[2][2] = z.z;

	vec3f_dot(-p->x, -p->y, -p->z, x.x, x.y, x.z, view_m[3][0]);
	vec3f_dot(-p->x, -p->y, -p->z, y.x, y.y, y.z, view_m[3][1]);
	vec3f_dot(-p->x, -p->y, -p->z, z.x, z.y, z.z, view_m[3][2]);

	mat_apply(&view_m);
}

/**********************************************************
MatrixTranspose_Inverse
転置逆行列
**********************************************************/
static matrix_t trans_m __attribute__((aligned(32))) = {
	{1.0f, 0.0f, 0.0f, 0.0f},
	{0.0f, 1.0f, 0.0f, 0.0f},
	{0.0f, 0.0f, 1.0f, 0.0f},
	{0.0f, 0.0f, 0.0f, 1.0f}};

void matrix_transposed_inverse(void)
{
	float tmp;

	mat_store(&trans_m);

	tmp = trans_m[0][1];
	trans_m[0][1] = trans_m[1][0];
	trans_m[1][0] = tmp;
	tmp = trans_m[0][2];
	trans_m[0][2] = trans_m[2][0];
	trans_m[2][0] = tmp;
	tmp = trans_m[1][2];
	trans_m[1][2] = trans_m[2][1];
	trans_m[2][1] = tmp;

	trans_m[0][3] = -(trans_m[0][3]);
	trans_m[1][3] = -(trans_m[1][3]);
	trans_m[2][3] = -(trans_m[2][3]);

	mat_load(&trans_m);
}

void matrix_transform(float *invecs, float *outvecs, int veccnt, int outvecskip)
{
	float *in = invecs;
	float *out = outvecs;
	int outskip = outvecskip >> 2;
	int i;

	for (i = 0; i < veccnt; i++)
	{
		mat_trans_single3_nomod(in[0], in[1], in[2], out[0], out[1], out[2]);
		in += 3;
		out += outskip;
	}
}

void matrix_transform_add(float *invecs, float *outvecs, int veccnt, int outvecskip)
{
	float *in = invecs;
	float *out = outvecs;
	float tmp[3];
	int outskip = outvecskip >> 2;
	int i;

	for (i = 0; i < veccnt; i++)
	{
		mat_trans_single3_nomod(in[0], in[1], in[2], tmp[0], tmp[1], tmp[2]);
		out[0] += tmp[0];
		out[1] += tmp[1];
		out[2] += tmp[2];
		in += 3;
		out += outskip;
	}
}

float fast_atan2(float y, float x)
{
	float abs_x = x < 0.0f ? -x : x;
	float abs_y = y < 0.0f ? -y : y;
	float z;
	int c;

	if (x == 0.0f && y == 0.0f)
		return 0.0f;

	c = abs_y < abs_x;
	if (c)
		z = abs_y / abs_x;
	else
		z = abs_x / abs_y;

	/* based on : “Efficient approximations for the arctangent function”
				Rajan, S. Sichun Wang Inkol, R. Joyal, A., May 2006 */
	/* PI/4*x - x*(fabs(x) - 1)*(0.2447 + 0.0663*fabs(x)); */

	z = (F_PI / 4.0f * z - z * (z - 1.0f) * (0.2447f + 0.0663f * z));

	//z = (69 * z * z + 5 * z + 286) / (3 * z * z + 5) * z;

	if (c)
	{
		if (x >= 0.0f)
		{
			if (y < 0.0f)
				z = -z;
		}
		if (x < 0.0f)
		{
			if (y >= 0.0f)
				z = F_PI - z;
			if (y < 0.0f)
				z = z - F_PI;
		}
	}

	if (!c)
	{
		if (x >= 0.0f)
		{
			if (y >= 0.0f)
				z = F_PI / 2.0f - z;
			if (y < 0.0f)
				z = z - F_PI / 2.0f;
		}
		if (x < 0.0f)
		{
			if (y >= 0.0f)
				z = z + F_PI / 2.0f;
			if (y < 0.0f)
				z = -z - F_PI / 2.0f;
		}
	}

	return z;
}
