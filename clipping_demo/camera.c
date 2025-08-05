#include "camera.h"
#include "matrix.h"

/* Camera functions */
void Camera_Init(camera_t *camera, float width, float height, float angle)
{
    // Initialize current position/target/up
    camera->position[0] = 0.0f;
    camera->position[1] = 0.0f;
    camera->position[2] = 0.0f;

    camera->target[0]   = 0.0f;
    camera->target[1]   = 0.0f;
    camera->target[2]   = 0.0f;

    camera->up[0] = 0.0f;
    camera->up[1] = 1.0f;
    camera->up[2] = 0.0f;

    // Initialize next position/target/up
    camera->next_position[0] = 0.0f;
    camera->next_position[1] = 0.0f;
    camera->next_position[2] = 0.0f;

    camera->next_target[0]   = 0.0f;
    camera->next_target[1]   = 0.0f;
    camera->next_target[2]   = 0.0f;

    camera->next_up[0] = 0.0f;
    camera->next_up[1] = 1.0f;
    camera->next_up[2] = 0.0f;

    // Screen size, aspect, FOV angle
    camera->width  = width;
    camera->height = height;
    camera->angle  = angle;
    camera->aspect = width / height;

    // We'll keep interpolation, but set it to 0 to disable smoothing
    camera->interpolation = 0.0f;
}

void Camera_Update(camera_t *camera)
{
    // Instead of interpolating, just set current to next directly
    camera->position[0] = camera->next_position[0];
    camera->position[1] = camera->next_position[1];
    camera->position[2] = camera->next_position[2];

    camera->target[0]   = camera->next_target[0];
    camera->target[1]   = camera->next_target[1];
    camera->target[2]   = camera->next_target[2];

    camera->up[0] = camera->next_up[0];
    camera->up[1] = camera->next_up[1];
    camera->up[2] = camera->next_up[2];

    // Build final PVM matrix: screen * projection * view
    mat_identity();
    matrix_mul_screen(camera->width, camera->height);
    matrix_mul_projection(camera->angle, camera->aspect);
    matrix_mul_view(camera->position, camera->target, camera->up);
    mat_store(&(camera->pvm));
}

void Camera_GetPVM(camera_t *camera, matrix_t *pvm)
{
    // Copy out the final matrix
    int i, j;
    for(i = 0; i < 4; i++)
        for(j = 0; j < 4; j++)
            (*pvm)[i][j] = (camera->pvm)[i][j];
}

void Camera_GetDirection(camera_t *camera, float *dir_3f)
{
    // direction = target - position
    dir_3f[0] = camera->target[0] - camera->position[0];
    dir_3f[1] = camera->target[1] - camera->position[1];
    dir_3f[2] = camera->target[2] - camera->position[2];
    // normalize if nonzero
    if(dir_3f[0] || dir_3f[1] || dir_3f[2])
        vec3f_normalize(dir_3f[0], dir_3f[1], dir_3f[2]);
}

void Camera_GetPosition(camera_t *camera, float *dir_3f)
{
    dir_3f[0] = camera->position[0];
    dir_3f[1] = camera->position[1];
    dir_3f[2] = camera->position[2];
}

void Camera_Set(camera_t *camera, float position[3], float target[3], float up[3])
{
    camera->position[0] = position[0];
    camera->position[1] = position[1];
    camera->position[2] = position[2];
    camera->next_position[0] = position[0];
    camera->next_position[1] = position[1];
    camera->next_position[2] = position[2];

    camera->target[0] = target[0];
    camera->target[1] = target[1];
    camera->target[2] = target[2];
    camera->next_target[0] = target[0];
    camera->next_target[1] = target[1];
    camera->next_target[2] = target[2];

    camera->up[0] = up[0];
    camera->up[1] = up[1];
    camera->up[2] = up[2];
    camera->next_up[0] = up[0];
    camera->next_up[1] = up[1];
    camera->next_up[2] = up[2];
}

void Camera_Get(camera_t *camera, float position[3], float target[3], float up[3])
{
    position[0] = camera->position[0];
    position[1] = camera->position[1];
    position[2] = camera->position[2];

    target[0] = camera->target[0];
    target[1] = camera->target[1];
    target[2] = camera->target[2];

    up[0] = camera->up[0];
    up[1] = camera->up[1];
    up[2] = camera->up[2];
}

void Camera_SetMove(camera_t *camera, float position[3], float target[3], float up[3])
{
    // Just store the "next" values
    camera->next_position[0] = position[0];
    camera->next_position[1] = position[1];
    camera->next_position[2] = position[2];

    camera->next_target[0] = target[0];
    camera->next_target[1] = target[1];
    camera->next_target[2] = target[2];

    camera->next_up[0] = up[0];
    camera->next_up[1] = up[1];
    camera->next_up[2] = up[2];
}

void Camera_AddMove(camera_t *camera, float position[3], float target[3], float up[3])
{
    // Add the offsets to "next" values
    camera->next_position[0] = camera->position[0] + position[0];
    camera->next_position[1] = camera->position[1] + position[1];
    camera->next_position[2] = camera->position[2] + position[2];

    camera->next_target[0] = camera->target[0] + target[0];
    camera->next_target[1] = camera->target[1] + target[1];
    camera->next_target[2] = camera->target[2] + target[2];

    camera->next_up[0] = camera->up[0] + up[0];
    camera->next_up[1] = camera->up[1] + up[1];
    camera->next_up[2] = camera->up[2] + up[2];
}

void Camera_SetInterpolation(camera_t *camera, float interpolation)
{
     camera->interpolation = 0.0f;
    (void)interpolation; // do nothing
}

 void Input_Init(input_t *input, input_device device)
{
    input->device = device;
    input->state = INPUT_PLUG_OUT;

    input->dev_state.start = 0;
    input->dev_state.a = 0;
    input->dev_state.b = 0;
    input->dev_state.x = 0;
    input->dev_state.y = 0;
    input->dev_state.l = 0;
    input->dev_state.r = 0;
    input->dev_state.l_value = 0.0f;
    input->dev_state.r_value = 0.0f;
    input->dev_state.joy_vector[0] = 0.0f;
    input->dev_state.joy_vector[1] = 0.0f;
}

void Input_Update(input_t *input)
{
    maple_device_t *dev;
    cont_state_t *st;

    /* Check for controller */
    dev = maple_enum_type(input->device, MAPLE_FUNC_CONTROLLER);
    if (!dev)
    {
        input->state = INPUT_PLUG_OUT;
        return;
    }

    /* Controller found */
    input->state = INPUT_PLUG_IN;

    st = (cont_state_t *)maple_dev_status(dev);

    if (st->buttons & CONT_START)
        input->dev_state.start++;
    else
        input->dev_state.start = 0;
    if (st->buttons & CONT_A)
        input->dev_state.a++;
    else
        input->dev_state.a = 0;
    if (st->buttons & CONT_B)
        input->dev_state.b++;
    else
        input->dev_state.b = 0;
    if (st->buttons & CONT_X)
        input->dev_state.x++;
    else
        input->dev_state.x = 0;
    if (st->buttons & CONT_Y)
        input->dev_state.y++;
    else
        input->dev_state.y = 0;
    if (st->buttons & CONT_DPAD_UP)
        input->dev_state.up++;
    else
        input->dev_state.up = 0;
    if (st->buttons & CONT_DPAD_DOWN)
        input->dev_state.down++;
    else
        input->dev_state.down = 0;
    if (st->buttons & CONT_DPAD_LEFT)
        input->dev_state.left++;
    else
        input->dev_state.left = 0;
    if (st->buttons & CONT_DPAD_RIGHT)
        input->dev_state.right++;
    else
        input->dev_state.right = 0;

    if (st->ltrig)
    {
        input->dev_state.l++;
        input->dev_state.l_value = (unsigned short)st->ltrig;
    }
    else
        input->dev_state.l = 0;

    if (st->rtrig)
    {
        input->dev_state.r++;
        input->dev_state.r_value = (unsigned short)st->rtrig;
    }
    else
        input->dev_state.r = 0;

    // Analog stick
    if ((st->joyx != 0) || (st->joyy != 0))
    {
        float length = 0.0f;
        input->dev_state.joy_vector[0] = ((float)st->joyx / 255.0f);
        input->dev_state.joy_vector[1] = ((float)st->joyy / 255.0f);

        vec3f_length(input->dev_state.joy_vector[0],
                     input->dev_state.joy_vector[1],
                     0.0f, length);

        if (length > 1.0f) {
           float z = 0.0f;
           vec3f_normalize(input->dev_state.joy_vector[0],
                           input->dev_state.joy_vector[1],
                           z);
        }
    }
    else
    {
        input->dev_state.joy_vector[0] = 0.0f;
        input->dev_state.joy_vector[1] = 0.0f;
    }
}

input_state Input_GetState(input_t *input, input_device_state *state)
{
    if (input->state == INPUT_PLUG_OUT)
        return INPUT_PLUG_OUT;

    *state = (input->dev_state);
    return INPUT_PLUG_IN;
}
