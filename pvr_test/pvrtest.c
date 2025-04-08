#include "dms.h"
#include "pvrtex.h"

static DMSModel* gModel = NULL;
const float SCREEN_CENTER_X = 320.0f;
const float SCREEN_CENTER_Y = 240.0f;
const float FOV_COTANGENT = 1.732050808f;   
int frame_triangle_count = 0;
int total_model_triangles = 0;

#define NUM_BALLS 44

typedef struct {
    float x, y, z;         
    float rx, ry, rz;      
    float drx, dry, drz;   
} BallInfo;

// Function prototypes
void resetFrameTriangleCount() {
    frame_triangle_count = 0;
}




static dttex_info_t model_texture;

void setupRenderState(pvr_dr_state_t* dr_state, kos_texture_t* texture) {
    pvr_poly_cxt_t cxt;
    
    if (texture) {
        pvr_poly_cxt_txr(&cxt, PVR_LIST_OP_POLY,
                         texture->fmt,
                         texture->w, texture->h,
                         texture->ptr, PVR_FILTER_BILINEAR);
    } else {
        pvr_poly_cxt_col(&cxt, PVR_LIST_OP_POLY);
    }
    
    cxt.gen.culling = PVR_CULLING_CCW;
    cxt.depth.comparison = PVR_DEPTHCMP_GEQUAL;  
    cxt.depth.write = PVR_DEPTHWRITE_ENABLE;     

    pvr_poly_hdr_t *hdr = (pvr_poly_hdr_t*)pvr_dr_target(*dr_state);
    pvr_poly_compile(hdr, &cxt);
    pvr_dr_commit(hdr);
}

void setupModelMatrix(float scale, float x_rot, float y_rot, float z_rot, float posX, float posY, float posZ) {
    mat_identity();

    // Use pre-computed constants
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

void RenderDMSModel(const DMSModel* model, float scale, float rotX, float rotY, float rotZ, 
    float posX, float posY, float posZ, pvr_dr_state_t* dr_state) {
    if (!model) return;

    setupModelMatrix(scale, rotX, rotY, rotZ, posX, posY, posZ);

    for (int m = 0; m < model->meshCount; m++) {
        const DMSMesh* mesh = &model->meshes[m];
        
        // Get texture for this mesh
        kos_texture_t* texture = NULL;
        if (mesh->textureId >= 0 && mesh->textureId < model->textureCount) {
            texture = model->textures[mesh->textureId];
        }
        
        // Set up rendering state with the appropriate texture
        setupRenderState(dr_state, texture);
        
        int i = 0;
        const DMSVertex* vertexBuffer = model->skeleton ? mesh->animatedVertices : mesh->vertices;

        while (i < mesh->indexCount) {
            uint32_t rawIndex = mesh->indices[i];
            int isStrip = (rawIndex & 0x80000000) != 0;
            uint32_t sId = (rawIndex >> 24) & 0x7F;

            if (isStrip) {
                // Find strip length
                int stripLength = 0;
                while ((i + stripLength) < mesh->indexCount) {
                    rawIndex = mesh->indices[i + stripLength];
                    if (!(rawIndex & 0x80000000) || ((rawIndex >> 24) & 0x7F) != sId)
                        break;
                    stripLength++;
                }

                // Each strip consists of (stripLength-2) triangles
                if (stripLength >= 3) {
                    frame_triangle_count += (stripLength - 2);
                }

                // Process entire strip at once
                for (int j = 0; j < stripLength; j++) {
                    uint32_t idx = mesh->indices[i + j] & 0x00FFFFFF;
                    const DMSVertex* v = &vertexBuffer[idx];
                    
                    pvr_vertex_t *vert = (pvr_vertex_t *)pvr_dr_target(*dr_state);
                    vert->flags = (j == stripLength - 1) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
                    mat_trans_single3_nomod(v->x, v->y, v->z, vert->x, vert->y, vert->z);
                    vert->u = v->u;
                    vert->v = v->v;
                    vert->argb = 0xFFFFFFFF;
                    
                    pvr_dr_commit(vert);
                }
                
                i += stripLength;
            } else {
                // Normal triangles
                uint32_t idx1 = mesh->indices[i] & 0x00FFFFFF;
                uint32_t idx2 = mesh->indices[i + 1] & 0x00FFFFFF;
                uint32_t idx3 = mesh->indices[i + 2] & 0x00FFFFFF;
                
                const DMSVertex* v1 = &vertexBuffer[idx1];
                const DMSVertex* v2 = &vertexBuffer[idx2];
                const DMSVertex* v3 = &vertexBuffer[idx3];
                
                // First vertex
                pvr_vertex_t *vert = (pvr_vertex_t *)pvr_dr_target(*dr_state);
                vert->flags = PVR_CMD_VERTEX;
                mat_trans_single3_nomod(v1->x, v1->y, v1->z, vert->x, vert->y, vert->z);
                vert->u = v1->u;
                vert->v = v1->v;
                vert->argb = 0xFFFFFFFF;
                pvr_dr_commit(vert);
                
                // Second vertex
                vert = (pvr_vertex_t *)pvr_dr_target(*dr_state);
                vert->flags = PVR_CMD_VERTEX;
                mat_trans_single3_nomod(v2->x, v2->y, v2->z, vert->x, vert->y, vert->z);
                vert->u = v2->u;
                vert->v = v2->v;
                vert->argb = 0xFFFFFFFF;
                pvr_dr_commit(vert);
                
                // Third vertex
                vert = (pvr_vertex_t *)pvr_dr_target(*dr_state);
                vert->flags = PVR_CMD_VERTEX_EOL;
                mat_trans_single3_nomod(v3->x, v3->y, v3->z, vert->x, vert->y, vert->z);
                vert->u = v3->u;
                vert->v = v3->v;
                vert->argb = 0xFFFFFFFF;
                pvr_dr_commit(vert);
                
                frame_triangle_count++;
                i += 3;
            }
        }
    }
}

int main(int argc, char **argv) {
    pvr_init_params_t params = {
        { PVR_BINSIZE_32, PVR_BINSIZE_0, PVR_BINSIZE_0, PVR_BINSIZE_0, PVR_BINSIZE_0 },
        1024 * 1024 * 3.0,  //vertex buffer
        1,            // DMA enabled
        0,            // FSAA disabled
        0,            // Autosort enabled
        4             // Allow for 3 overflow bins
    };
    
    pvr_init(&params);
    
    // Stats tracking variables
    uint32 frames = 0;
    uint32 last_time = timer_ms_gettime64();
    uint32 fps_display_timer = last_time;
    float fps = 0.0f;
    float pps = 0.0f;
    
    // Initialize balls in a grid pattern (10x5 grid)
    BallInfo balls[NUM_BALLS];
    const int GRID_COLS = 10;
    const int GRID_ROWS = 5;
    const float SPACING_X = 3.0f;
    const float SPACING_Y = 3.0f;
    const float START_X = -((GRID_COLS-1) * SPACING_X) / 2.0f;
    const float START_Y = -((GRID_ROWS-1) * SPACING_Y) / 2.0f;
    const float BASE_Z = 25.0f;  // Further back to accommodate all models
    
    int idx = 0;
    for (int y = 0; y < GRID_ROWS; y++) {
        for (int x = 0; x < GRID_COLS; x++) {
            if (idx < NUM_BALLS) {
                // Position
                balls[idx].x = START_X + (x * SPACING_X);
                balls[idx].y = START_Y + (y * SPACING_Y);
                balls[idx].z = BASE_Z;
                
                // Rotation and speeds - vary by position
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
 
    // Load the 3D model
    gModel = LoadDMSModel("/rd/ball.dms");
    if (!gModel) {
        printf("Failed to load animated model\n");
        return 1;
    }
    
    // Load textures for the model based on textureIds
    if (gModel->textureCount > 0) {
        printf("Model has %d textures\n", gModel->textureCount);
        
        // Load each texture
        for (int i = 0; i < gModel->textureCount; i++) {
            char texturePath[64];
            sprintf(texturePath, "/rd/ball%d.dt", i);
            
            dttex_info_t tex_info;
            if (pvrtex_load(texturePath, &tex_info)) {
                kos_texture_t* texture = (kos_texture_t*)malloc(sizeof(kos_texture_t));
                texture->ptr = tex_info.ptr;
                texture->w = tex_info.width;
                texture->h = tex_info.height;
                texture->fmt = tex_info.pvrformat;
                gModel->textures[i] = texture;
                printf("Loaded texture %d\n", i);
            } else {
                printf("Failed to load texture %d\n", i);
                gModel->textures[i] = NULL;
            }
        }
    } else {
        // If no textures in model, try to load default texture
        if (!pvrtex_load("/rd/ball0.dt", &model_texture)) {
            printf("Warning: Failed to load default texture\n");
        } else {
            // Create texture array with single texture
            gModel->textureCount = 1;
            gModel->textures = (kos_texture_t**)calloc(1, sizeof(kos_texture_t*));
            kos_texture_t* texture = (kos_texture_t*)malloc(sizeof(kos_texture_t));
            texture->ptr = model_texture.ptr;
            texture->w = model_texture.width;
            texture->h = model_texture.height;
            texture->fmt = model_texture.pvrformat;
            gModel->textures[0] = texture;
        }
    }

    total_model_triangles = 0;
    for (int m = 0; m < gModel->meshCount; m++) {
        int i = 0;
        int mesh_triangles = 0;
        
        while (i < gModel->meshes[m].indexCount) {
            uint32_t rawIndex = gModel->meshes[m].indices[i];
            
            if ((rawIndex & 0x80000000) != 0) {
                // Triangle strip
                uint32_t stripId = (rawIndex >> 24) & 0x7F;
                int stripLength = 0;
                
                while (i < gModel->meshes[m].indexCount) {
                    rawIndex = gModel->meshes[m].indices[i];
                    if (!(rawIndex & 0x80000000) || ((rawIndex >> 24) & 0x7F) != stripId)
                        break;
                    stripLength++;
                    i++;
                }
                
                if (stripLength >= 3) {
                    mesh_triangles += stripLength - 2;
                }
            } else {
                if (i + 2 < gModel->meshes[m].indexCount) {
                    mesh_triangles++;
                    i += 3;
                } else {
                    i++;
                }
            }
        }
        
        gModel->meshes[m].triangleCount = mesh_triangles;
        total_model_triangles += mesh_triangles;
    }
    
    // Total triangles for all models
    int benchmark_triangles = total_model_triangles * NUM_BALLS;
    
    printf("Model loaded: %d triangles per model, %d total triangles for benchmark\n", 
           total_model_triangles, benchmark_triangles);
    
    float modelScale = 1.2f;  

    while(1) {
        pvr_wait_ready();
        pvr_scene_begin();
        
        // First render the 3D model
        pvr_list_begin(PVR_LIST_OP_POLY);
        
        pvr_dr_state_t dr_state;
        pvr_dr_init(&dr_state);
        
        resetFrameTriangleCount();
        
        for (int i = 0; i < NUM_BALLS; i++) {
            // Update rotation
            balls[i].rx += balls[i].drx * 0.01f;
            balls[i].ry += balls[i].dry * 0.01f;
            
            if (balls[i].rx >= 6.28f) balls[i].rx -= 6.28f;
            if (balls[i].ry >= 6.28f) balls[i].ry -= 6.28f;
        }

        //  render all balls with their updated rotations
        for (int i = 0; i < NUM_BALLS; i++) {
            RenderDMSModel(gModel, modelScale, 
                        balls[i].rx, balls[i].ry, balls[i].rz,
                        balls[i].x, balls[i].y, balls[i].z, 
                        &dr_state);
        }
        
        pvr_list_finish();
        
        pvr_scene_finish();

        // Update FPS counter
        frames++;
        uint32 current_time = timer_ms_gettime64();
        if (current_time - fps_display_timer >= 1000) {
            float time_diff = (current_time - fps_display_timer) / 1000.0f;
            fps = frames / time_diff;
            
            // Calculate polygons per second
            pps = frame_triangle_count * fps;
            
            // Print the stats
            printf("FPS: %.2f | PPS: %.2fK | Tris: %d/%d\n", 
                   fps, pps/1000.0f, frame_triangle_count, benchmark_triangles);
            
            frames = 0;
            fps_display_timer = current_time;
        }
        
        // Controller handling
        maple_device_t* cont = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        if (cont) {
            cont_state_t* state = (cont_state_t *)maple_dev_status(cont);
            if (state && (state->buttons & CONT_START)) {
                break;
            }
        }
    }
    
    // Print final stats
    printf("Final stats - Average FPS: %.2f | PPS: %.2fK\n", fps, pps/1000.0f);
 
    // Free textures
    if (gModel && gModel->textures) {
        for (int i = 0; i < gModel->textureCount; i++) {
            if (gModel->textures[i]) {
                free(gModel->textures[i]);
            }
        }
        free(gModel->textures);
    }
    
    // Unload model
    //UnloadDMSModel(gModel);

    pvr_shutdown();
    return 0;
}
