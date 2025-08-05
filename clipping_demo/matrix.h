#ifndef _MATRIX_H_INCLUDED_
#define _MATRIX_H_INCLUDED_


/**********************************************************
MtrixScreen
スクリーン行列
**********************************************************/
extern void matrix_mul_screen(float width, float height);

/**********************************************************
MtrixProjection
プロジェクション行列
**********************************************************/
extern void matrix_mul_projection(float angle, float aspect);

/**********************************************************
MtrixView
ビュー行列
**********************************************************/
extern void matrix_mul_view(float *eye, float *target, float *up);
extern void matrix_transposed_inverse(void);

extern void matrix_transform(float *invecs, float *outvecs, int veccnt, int outvecskip);
extern void matrix_transform_add(float *invecs, float *outvecs, int veccnt, int outvecskip);
extern float fast_atan2(float y, float x);

#endif
