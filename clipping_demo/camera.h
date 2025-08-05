#ifndef _CAMERA_H_INCLUDED_
#define _CAMERA_H_INCLUDED_

#include <kos.h>

// Camera-related types and functions
typedef struct
{
    float position[3];
    float target[3];
    float up[3];

    float next_position[3];
    float next_target[3];
    float next_up[3];

    float width, height; /* カメラサイズ */
    float angle;         /* 画角 */
    float aspect;        /* アスペクト比 */

    float interpolation; /* 補間係数 */
    int none;

    matrix_t pvm;
    matrix_t view;
} camera_t __attribute__((aligned(32)));

extern void Camera_Init(camera_t *camera, float width, float height, float angle);

extern void Camera_Update(camera_t *camera);
extern void Camera_GetPVM(camera_t *camera, matrix_t *pvm);
extern void Camera_GetDirection(camera_t *camera, float *dir_3f);
extern void Camera_GetPosition(camera_t *camera, float *dir_3f);

extern void Camera_Set(camera_t *camera, float position[3], float target[3], float up[3]);
extern void Camera_Get(camera_t *camera, float position[3], float target[3], float up[3]);
extern void Camera_SetMove(camera_t *camera, float position[3], float target[3], float up[3]);
extern void Camera_AddMove(camera_t *camera, float position[3], float target[3], float up[3]);
extern void Camera_SetInterpolation(camera_t *camera, float interpolation);

// Input-related types and enums
typedef enum {
    INPUT_CONTROLLER_0,
    INPUT_CONTROLLER_1,
    INPUT_CONTROLLER_2,
    INPUT_CONTROLLER_3
} input_device;

typedef enum {
    INPUT_PLUG_OUT,
    INPUT_PLUG_IN
} input_state;

typedef struct {
    unsigned int start;
    unsigned int a, b, x, y;
    unsigned int up, down, right, left;
    unsigned int l, r;
    unsigned short l_value, r_value;
    float joy_vector[2];
} input_device_state;

typedef struct
{
    input_device_state dev_state;
    input_device device;
    input_state state;
} input_t;

// Input-related functions
extern void Input_Init(input_t *input, input_device device);

extern void Input_Update(input_t *input);
extern input_state Input_GetState(input_t *input, input_device_state *state);
extern void Camera_GetView(camera_t *camera, matrix_t *view);

#endif