#include "camera.h"
#include "matrix.h"
#include "primitive.h"
#include "dms.h"
#include "pvrtex.h"
#include "perf_profile.h"


#define mat_trans_nodiv_nomod(x, y, z, x2, y2, z2, w2) do { \
        register float __x __asm__("fr12") = (x); \
        register float __y __asm__("fr13") = (y); \
        register float __z __asm__("fr14") = (z); \
        register float __w __asm__("fr15") = 1.0f; \
        __asm__ __volatile__( "ftrv  xmtrx, fv12\n" \
                              : "=f" (__x), "=f" (__y), "=f" (__z), "=f" (__w) \
                              : "0" (__x), "1" (__y), "2" (__z), "3" (__w) ); \
        x2 = __x; y2 = __y; z2 = __z; w2 = __w; \
    } while(false)


/* Camera */
#define GAME_CAMERA_POSITION 0.0f, 0.0f, 0.0f
#define GAME_CAMERA_TARGET   0.0f, 0.0f, 0.0f
#define GAME_CAMERA_UP       0.0f, 1.0f, 0.0f
camera_t camera;
matrix_t cam_pvm __attribute__((aligned(32)));

DMSModel* dms_model = NULL;

input_t input;

/* PVR init params */
static pvr_init_params_t pvr_params = {
    {
        PVR_BINSIZE_32, PVR_BINSIZE_0,
        PVR_BINSIZE_0,  PVR_BINSIZE_0,
        PVR_BINSIZE_0
    },
    /* Vertex buffer size */
    1024 * 1024 * 2.9,
    1,
    0,
    0,
    /* Extra Tile Bins */
    4
};
void DrawBoundingSphere(const Vector3* center, float radius, pvr_dr_state_t* dr_state) {
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;
    
    // Use line strip mode
    pvr_poly_cxt_col(&cxt, PVR_LIST_OP_POLY);
    cxt.gen.shading = PVR_SHADE_FLAT;
    cxt.gen.culling = PVR_CULLING_NONE;
    pvr_poly_compile(&hdr, &cxt);
    primitive_header(&hdr, sizeof(pvr_poly_hdr_t), dr_state);
    
    // Draw 3 circles for X, Y, Z axes
    const int segments = 16;
    
    // XY circle
    for (int i = 0; i <= segments; i++) {
        float angle = (i % segments) * 2.0f * F_PI / segments;
        float x = center->x + radius * fcos(angle);
        float y = center->y + radius * fsin(angle);
        float z = center->z;
        
        pvr_vertex_t *vert = (pvr_vertex_t *)pvr_dr_target(*dr_state);
        vert->flags = (i == segments) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        mat_trans_single3_nomod(x, y, z, vert->x, vert->y, vert->z);
        vert->argb = 0xFF00FF00; // Green
        pvr_dr_commit(vert);
    }
    
    // XZ circle
    for (int i = 0; i <= segments; i++) {
        float angle = (i % segments) * 2.0f * F_PI / segments;
        float x = center->x + radius * fcos(angle);
        float y = center->y;
        float z = center->z + radius * fsin(angle);
        
        pvr_vertex_t *vert = (pvr_vertex_t *)pvr_dr_target(*dr_state);
        vert->flags = (i == segments) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        mat_trans_single3_nomod(x, y, z, vert->x, vert->y, vert->z);
        vert->argb = 0xFFFF0000; // Red
        pvr_dr_commit(vert);
    }
}
 

// todo add frustnum culling but for larger meshes..  not just a cheap per mesh. 

static pvr_vertex_t* global_vertex_buffer = NULL;
static int global_vertex_buffer_size = 0;

void DMS_Render(const DMSModel* model, matrix_t *pvm)
{
   if (!model) return;
   pvr_dr_state_t dr_state;
   pvr_dr_init(&dr_state);

   if (!global_vertex_buffer) {
       int max_verts = 0;
       for (int i = 0; i < model->meshCount; i++) {
           if (model->meshes[i].vertexCount > max_verts) {
               max_verts = model->meshes[i].vertexCount;
           }
       }
       
       max_verts = max_verts > 0 ? max_verts : 50000;
       global_vertex_buffer = (pvr_vertex_t*)memalign(32, max_verts * sizeof(pvr_vertex_t));
       global_vertex_buffer_size = max_verts;
   }

   for (int m = 0; m < model->meshCount; m++) {
       const DMSMesh* mesh = &model->meshes[m];
       if (!mesh->vertices || mesh->vertexCount <= 0 || mesh->indexCount <= 0)
           continue;

       const DMSVertex* srcVerts = mesh->animatedVertices
                                 ? mesh->animatedVertices
                                 : mesh->vertices;

       // Pre-compute final matrix once
       {
           PROFILE_START_CYCLES();
           
           mat_identity();
           mat_load(pvm);
           matrix_t finalMat __attribute__((aligned(32)));
           mat_store(&finalMat);
           mat_load(&finalMat);
           
           PROFILE_END_CYCLES(g_profiles.matrix_ops);
       }

 
    bool need_clipping = false;
    float x, y, z, w;
    mat_trans_nodiv_nomod(mesh->boundingCenter.x, mesh->boundingCenter.y, mesh->boundingCenter.z, 
                        x, y, z, w);



    if (w + mesh->boundingRadius < NEAR_Z) {
        continue;
    }

    if (w - mesh->boundingRadius < NEAR_Z) {
        need_clipping = true;
    }  



       pvr_poly_cxt_t cxt;
       pvr_poly_hdr_t hdr;
       
       {
           PROFILE_START_CYCLES();
           
           if (mesh->textureId >= 0 &&
               mesh->textureId < model->textureCount &&
               model->textures[mesh->textureId])
           {
               kos_texture_t* tex = model->textures[mesh->textureId];
               pvr_poly_cxt_txr(&cxt, PVR_LIST_OP_POLY,
                                tex->fmt, tex->w, tex->h, tex->ptr,
                                PVR_FILTER_BILINEAR);
           } else {
               pvr_poly_cxt_col(&cxt, PVR_LIST_OP_POLY);
           }
           cxt.gen.shading = PVR_SHADE_FLAT;
           cxt.gen.culling = PVR_CULLING_NONE;

           pvr_poly_compile(&hdr, &cxt);
           primitive_header(&hdr, sizeof(pvr_poly_hdr_t), &dr_state);
           
           PROFILE_END_CYCLES(g_profiles.header_setup);
       }

       if (need_clipping) {
           // CLIPPING PATH: Transform all vertices first, then clip
           {
               PROFILE_START_CYCLES();
               
               for (int i = 0; i < mesh->vertexCount; i++) {
                   mat_trans_single3_nomod(srcVerts[i].x, srcVerts[i].y, srcVerts[i].z, 
                                         global_vertex_buffer[i].x,
                                         global_vertex_buffer[i].y,
                                         global_vertex_buffer[i].z);
                   
                   global_vertex_buffer[i].flags = PVR_CMD_VERTEX;
                   global_vertex_buffer[i].u = srcVerts[i].u;
                   global_vertex_buffer[i].v = srcVerts[i].v;
                   global_vertex_buffer[i].argb = 0xFFFFFFFF;
               }
               
               PROFILE_END_CYCLES(g_profiles.transform);
           }
           
           {
               PROFILE_START_CYCLES();
               primitive_nclip_polygon_strip(global_vertex_buffer, (int*)mesh->indices, mesh->indexCount, &dr_state);
               PROFILE_END_CYCLES(g_profiles.clipping);
           }
       } else {
           // FAST PATH
           {
               PROFILE_START_CYCLES();
           
               int i = 0;
               while (i < mesh->indexCount) {
                   uint32_t rawIndex = mesh->indices[i];
                   int isStrip = (rawIndex & 0x80000000) != 0;
                   uint32_t sId = (rawIndex >> 24) & 0x7F;

                   if (isStrip) {
                       int stripLength = 0;
                       while ((i + stripLength) < mesh->indexCount) {
                           rawIndex = mesh->indices[i + stripLength];
                           if (!(rawIndex & 0x80000000) || ((rawIndex >> 24) & 0x7F) != sId)
                               break;
                           stripLength++;
                       }

                       for (int j = 0; j < stripLength; j++) {
                           uint32_t idx = mesh->indices[i + j] & 0x00FFFFFF;
                           const DMSVertex* v = &srcVerts[idx];
                           
                           pvr_vertex_t *vert = (pvr_vertex_t *)pvr_dr_target(dr_state);
                           vert->flags = (j == stripLength - 1) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
                           
                           mat_trans_single3_nomod(v->x, v->y, v->z, vert->x, vert->y, vert->z);
                           vert->u = v->u;
                           vert->v = v->v;
                           vert->argb = 0xFFFFFFFF;
                           
                           pvr_dr_commit(vert);
                       }
                       
                       i += stripLength;
                   } else {
                       if (i + 2 < mesh->indexCount) {
                           uint32_t idx1 = mesh->indices[i] & 0x00FFFFFF;
                           uint32_t idx2 = mesh->indices[i+1] & 0x00FFFFFF;
                           uint32_t idx3 = mesh->indices[i+2] & 0x00FFFFFF;
                           
                           const DMSVertex* v1 = &srcVerts[idx1];
                           const DMSVertex* v2 = &srcVerts[idx2];
                           const DMSVertex* v3 = &srcVerts[idx3];
                           
                           pvr_vertex_t *vert = (pvr_vertex_t *)pvr_dr_target(dr_state);
                           vert->flags = PVR_CMD_VERTEX;
                           mat_trans_single3_nomod(v1->x, v1->y, v1->z, vert->x, vert->y, vert->z);
                           vert->u = v1->u;
                           vert->v = v1->v;
                           vert->argb = 0xFFFFFFFF;
                           pvr_dr_commit(vert);
                           
                           vert = (pvr_vertex_t *)pvr_dr_target(dr_state);
                           vert->flags = PVR_CMD_VERTEX;
                           mat_trans_single3_nomod(v2->x, v2->y, v2->z, vert->x, vert->y, vert->z);
                           vert->u = v2->u;
                           vert->v = v2->v;
                           vert->argb = 0xFFFFFFFF;
                           pvr_dr_commit(vert);
                           
                           vert = (pvr_vertex_t *)pvr_dr_target(dr_state);
                           vert->flags = PVR_CMD_VERTEX_EOL;
                           mat_trans_single3_nomod(v3->x, v3->y, v3->z, vert->x, vert->y, vert->z);
                           vert->u = v3->u;
                           vert->v = v3->v;
                           vert->argb = 0xFFFFFFFF;
                           pvr_dr_commit(vert);
                           
                           i += 3;
                       } else {
                           i++;
                       }
                   }
               }
               
               PROFILE_END_CYCLES(g_profiles.vertex_submit);
           }
       }
   }
} 

 
static void game_init(void)
{
     // model_init();

     dms_model = LoadDMSModel("/rd/level.dms");
    if (dms_model) {
        printf("DMS model loaded successfully: %d meshes\n", dms_model->meshCount);
        for (int i = 0; i < dms_model->meshCount; i++) {
            printf("Mesh %d: %d vertices, %d indices\n", 
                   i, dms_model->meshes[i].vertexCount, dms_model->meshes[i].indexCount);
        }

        // Load textures if model->textureCount > 0
        if (dms_model->textureCount > 0) {
            printf("Model has %d textures\n", dms_model->textureCount);
        
            // Load each texture
            for (int i = 0; i < dms_model->textureCount; i++) {
                char texturePath[64];
                sprintf(texturePath, "/rd/level%d.dt", i);
                
                dttex_info_t tex_info;
                if (pvrtex_load(texturePath, &tex_info)) {
                    kos_texture_t* texture = (kos_texture_t*)malloc(sizeof(kos_texture_t));
                    texture->ptr = tex_info.ptr;
                    texture->w   = tex_info.width;
                    texture->h   = tex_info.height;
                    texture->fmt = tex_info.pvrformat;
                    
                    dms_model->textures[i] = texture;
                    printf("Loaded texture %d\n", i);
                } else {
                    printf("Failed to load texture %d\n", i);
                    dms_model->textures[i] = NULL;
                }
            }
        } else {
            // Fallback  
            dttex_info_t tex_info;
            if (!pvrtex_load("/rd/fox2.dt", &tex_info)) {
                printf("Warning: Failed to load default texture\n");
            } else {
                dms_model->textureCount = 1;
                dms_model->textures = (kos_texture_t**)calloc(1, sizeof(kos_texture_t*));
                
                kos_texture_t* texture = (kos_texture_t*)malloc(sizeof(kos_texture_t));
                texture->ptr = tex_info.ptr;
                texture->w   = tex_info.width;
                texture->h   = tex_info.height;
                texture->fmt = tex_info.pvrformat;
                dms_model->textures[0] = texture;
            }
        }
    } else {
        printf("Failed to load DMS model\n");
    }
    
    
    Camera_Init(&camera, 640.0f, 480.0f, F_PI / 6.0f);
    Camera_Set(&camera, (float[3]){GAME_CAMERA_POSITION}, (float[3]){GAME_CAMERA_TARGET}, (float[3]){GAME_CAMERA_UP});
    Camera_SetInterpolation(&camera, 0.05f);

    Input_Init(&input, INPUT_CONTROLLER_0);
}

 
static void set_camera(matrix_t *pvm, input_t *input)
{
    static float px = 0.0f, py = 10.0f, pz = 7.5f;
    static float yaw = 0.0f, pitch = 0.0f;

    input_device_state cont;
    if (Input_GetState(input, &cont))
    {
        yaw   += cont.joy_vector[0] * 0.03f;
        pitch -= cont.joy_vector[1] * 0.03f;
        if (pitch >  1.55f) pitch =  1.55f;
        if (pitch < -1.55f) pitch = -1.55f;

        float speedFwd    = (cont.r_value - cont.l_value) * 0.005f;
        float speedVert   = (cont.up - cont.down)       * 0.1f;
        float speedStrafe = (cont.right - cont.left)    * 0.1f;

        float cx = fsin(yaw)*fcos(pitch);
        float cy = fsin(pitch);
        float cz = -fcos(yaw)*fcos(pitch);

        // forward/back
        px += cx * speedFwd;
        py += cy * speedFwd;
        pz += cz * speedFwd;

        // up/down
        py += speedVert;

        // strafe
        float strafeAngle = yaw - (F_PI * 0.5f);
        px += fsin(strafeAngle)  * speedStrafe;
        pz += -fcos(strafeAngle) * speedStrafe;
    }

    //  camera pos & target
    float pos[3] = { px, py, pz };
    float cx = fsin(yaw)*fcos(pitch);
    float cy = fsin(pitch);
    float cz = -fcos(yaw)*fcos(pitch);
    float tar[3] = { px + cx, py + cy, pz + cz };
    float up[3]  = { 0.0f, 1.0f, 0.0f };

    Camera_SetMove(&camera, pos, tar, up);
    Camera_Update(&camera);
    Camera_GetPVM(&camera, pvm);
} 

/*-------------------------------------------
  Main update/draw
-------------------------------------------*/
static void game_update(void)
{
    Input_Update(&input);
    set_camera(&cam_pvm, &input);
}

static void game_draw(void)
{
    if (dms_model && dms_model->meshCount > 0) {
        if (dms_model->meshes[0].vertexCount > 0 && dms_model->meshes[0].indexCount > 0) {
             if (dms_model->skeleton && dms_model->skeleton->animCount > 0) {
                PROFILE_START_CYCLES();
                
                float dt = 1.0f / 60.0f;
                UpdateDMSModelAnimation(dms_model, dt);
                for (int i = 0; i < dms_model->meshCount; i++) {
                    UpdateDMSMeshAnimation(&dms_model->meshes[i], dms_model->skeleton);
                }
                
                PROFILE_END_CYCLES(g_profiles.animation);
            }
            DMS_Render(dms_model, &cam_pvm);
        }
    }
}


 
int main(int argc, char* argv[])
{
    pvr_init(&pvr_params);
    
    // Initialize performance profiling
    perf_profile_init();

    game_init();

    uint32 frames = 0;
    uint32 last_time = timer_ms_gettime64();
    uint32 profile_print_time = last_time;
    float fps = 0.0f;

    while (1)
    {
        maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        if (dev) {
            cont_state_t *st = (cont_state_t *)maple_dev_status(dev);
            if (st && ((st->buttons & 0x60e) == 0x60e))
                break;
        }

        // PROFILE: Total frame time
        PROFILE_START_CYCLES();

        game_update();

        vid_border_color(0, 0, 0);
        pvr_wait_ready();
        vid_border_color(255, 0, 0);

        pvr_scene_begin();
        game_draw();
        pvr_scene_finish();

        vid_border_color(0, 255, 0);
        
        PROFILE_END_CYCLES(g_profiles.total_frame);

        frames++;
        uint32 current_time = timer_ms_gettime64();
        
         if (current_time - last_time >= 2000) {
            fps = (frames * 1000.0f) / (current_time - last_time);
            printf("FPS: %.2f\n", fps);

            frames = 0;
            last_time = current_time;
        }
        
         if (current_time - profile_print_time >= 5000) {
            perf_profile_print();
            perf_profile_reset();
            profile_print_time = current_time;
        }
    }

    pvr_shutdown();
    return 0;
}
