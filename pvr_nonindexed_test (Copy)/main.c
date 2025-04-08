#include <kos.h>


const float SCREEN_CENTER_X = 320.0f;
const float SCREEN_CENTER_Y = 240.0f;
const float FOV_COTANGENT = 1.732050808f;   
int frame_triangle_count = 0;

#define NUM_BALLS 72
#define SPHERE_STACKS 20
#define SPHERE_SLICES 20

typedef struct {
    float x, y, z;         
    float rx, ry, rz;      
    float drx, dry, drz;   
} BallInfo;

void resetFrameTriangleCount() {
    frame_triangle_count = 0;
}

void setupRenderState(pvr_dr_state_t* dr_state) {
    pvr_poly_cxt_t cxt;
    
    pvr_poly_cxt_col(&cxt, PVR_LIST_OP_POLY);
    
    cxt.gen.culling = PVR_CULLING_CCW;
    cxt.depth.comparison = PVR_DEPTHCMP_GEQUAL;  
    cxt.depth.write = PVR_DEPTHWRITE_ENABLE;     

    pvr_poly_hdr_t *hdr = (pvr_poly_hdr_t*)pvr_dr_target(*dr_state);
    pvr_poly_compile(hdr, &cxt);
    pvr_dr_commit(hdr);
}

void setupModelMatrix(float scale, float x_rot, float y_rot, float z_rot, float posX, float posY, float posZ) {
    mat_identity();

    mat_perspective(SCREEN_CENTER_X,
                   SCREEN_CENTER_Y,
                   FOV_COTANGENT, 
                   1.0f,
                   10000.0f);

    // Model transformations
    mat_translate(posX, posY, -posZ);
    mat_rotate_x(x_rot);
    mat_rotate_y(y_rot);
    mat_rotate_z(z_rot);
    mat_scale(scale, scale, scale);
}

void Render_ball(float radius, pvr_dr_state_t* dr_state, uint32_t color) {
    float stackStep = 3.14159265f / SPHERE_STACKS;
    float sliceStep = 6.28318530f / SPHERE_SLICES;
    
    for (int stack = 0; stack < SPHERE_STACKS; stack++) {
        float stackAngle = 3.14159265f / 2.0f - stack * stackStep;
        float nextStackAngle = 3.14159265f / 2.0f - (stack + 1) * stackStep;
        
        float z1 = sinf(stackAngle);
        float z2 = sinf(nextStackAngle);
        float r1 = cosf(stackAngle);
        float r2 = cosf(nextStackAngle);
        
        for (int slice = 0; slice <= SPHERE_SLICES; slice++) {
            float sliceAngle = slice * sliceStep;
            
            float x = cosf(sliceAngle);
            float y = sinf(sliceAngle);
            
            pvr_vertex_t *vert = (pvr_vertex_t *)pvr_dr_target(*dr_state);
            vert->flags = PVR_CMD_VERTEX;
            
            float vx = x * r2 * radius;
            float vy = y * r2 * radius;
            float vz = z2 * radius;
            
            mat_trans_single3_nomod(vx, vy, vz, vert->x, vert->y, vert->z);
            
            vert->u = 0.0f;
            vert->v = 0.0f;
            vert->argb = color;
            
            pvr_dr_commit(vert);
            
            vert = (pvr_vertex_t *)pvr_dr_target(*dr_state);
            vert->flags = (slice == SPHERE_SLICES) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
            
            vx = x * r1 * radius;
            vy = y * r1 * radius;
            vz = z1 * radius;
            
            mat_trans_single3_nomod(vx, vy, vz, vert->x, vert->y, vert->z);
            
            vert->u = 0.0f;
            vert->v = 0.0f;
            vert->argb = color;
            
            pvr_dr_commit(vert);
            
            if (slice > 0) {
                frame_triangle_count++;
            }
        }
        
        frame_triangle_count += (SPHERE_SLICES - 1);
    }
}

int main(int argc, char **argv) {
    pvr_init_params_t params = {
        { PVR_BINSIZE_32, PVR_BINSIZE_0, PVR_BINSIZE_0, PVR_BINSIZE_0, PVR_BINSIZE_0 },
        1024 * 1024 * 3.0,  // vertex buffer
        1,            // DMA enabled
        0,            // FSAA disabled
        0,            // Autosort enabled
        4             // Allow for 3 overflow bins
    };
    
    pvr_init(&params);
    
    uint32 frames = 0;
    uint32 last_time = timer_ms_gettime64();
    uint32 fps_display_timer = last_time;
    float fps = 0.0f;
    float pps = 0.0f;
    
    BallInfo balls[NUM_BALLS];
    const int GRID_COLS = 10;
    const int GRID_ROWS = 10;
    const float SPACING_X = 2.0f;
    const float SPACING_Y = 2.0f;
    const float START_X = -((GRID_COLS-1) * SPACING_X) / 2.0f;
    const float START_Y = -((GRID_ROWS-1) * SPACING_Y) / 2.0f;
    const float BASE_Z = 18.0f;  
    
    int idx = 0;
    for (int y = 0; y < GRID_ROWS; y++) {
        for (int x = 0; x < GRID_COLS; x++) {
            if (idx < NUM_BALLS) {
                // Position
                balls[idx].x = START_X + (x * SPACING_X);
                balls[idx].y = START_Y + (y * SPACING_Y);
                balls[idx].z = BASE_Z;
                
                balls[idx].rx = 0.0f;
                balls[idx].ry = 0.0f;
                balls[idx].rz = 0.0f;
                balls[idx].drx = 0.3f + (idx % 5) * 0.1f;  
                balls[idx].dry = 0.5f + (idx % 7) * 0.1f;  
                balls[idx].drz = 0.0f;
                
                idx++;
            }
        }
    }
    
    int triangles_per_ball = SPHERE_STACKS * SPHERE_SLICES * 2;
    int benchmark_triangles = triangles_per_ball * NUM_BALLS;
    

    
    float ballRadius = 1.0f;  

    while(1) {
        pvr_wait_ready();
        pvr_scene_begin();
        
        // Render the 3D balls
        pvr_list_begin(PVR_LIST_OP_POLY);
        
        pvr_dr_state_t dr_state;
        pvr_dr_init(&dr_state);
        
        resetFrameTriangleCount();
        
        // Set up common render state
        setupRenderState(&dr_state);
        
        for (int i = 0; i < NUM_BALLS; i++) {
            // Update rotation
            balls[i].rx += balls[i].drx * 0.01f;
            balls[i].ry += balls[i].dry * 0.01f;
            
            if (balls[i].rx >= 6.28f) balls[i].rx -= 6.28f;
            if (balls[i].ry >= 6.28f) balls[i].ry -= 6.28f;
            
            setupModelMatrix(ballRadius, balls[i].rx, balls[i].ry, balls[i].rz,
                           balls[i].x, balls[i].y, balls[i].z);
            
            uint32_t color = 0xFF000000 | 
                             ((i * 5) % 256) | // Red
                             (((i * 7) % 256) << 8) | // Green
                             (((i * 11) % 256) << 16); // Blue
            
            Render_ball(ballRadius, &dr_state, color);
        }
        
        pvr_list_finish();
        
        pvr_scene_finish();

        frames++;
        uint32 current_time = timer_ms_gettime64();
        if (current_time - fps_display_timer >= 1000) {
            float time_diff = (current_time - fps_display_timer) / 1000.0f;
            fps = frames / time_diff;
            
            pps = frame_triangle_count * fps;
            
            printf("FPS: %.2f | PPS: %.2fK | Tris: %d/%d\n", 
                   fps, pps/1000.0f, frame_triangle_count, benchmark_triangles);
            
            frames = 0;
            fps_display_timer = current_time;
        }
        
        maple_device_t* cont = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        if (cont) {
            cont_state_t* state = (cont_state_t *)maple_dev_status(cont);
            if (state && (state->buttons & CONT_START)) {
                break;
            }
        }
    }
    
    printf("Final stats - Average FPS: %.2f | PPS: %.2fK\n", fps, pps/1000.0f);
    
    pvr_shutdown();
    return 0;
}