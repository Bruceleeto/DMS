#include "dms.h"
#include "pvrtex.h"

static DMSModel* gModel = NULL;
const float SCREEN_CENTER_X = 320.0f;
const float SCREEN_CENTER_Y = 240.0f;
const float FOV_COTANGENT = 1.732050808f;   
const float DEFAULT_MODEL_SCALE = 1.0f;
float modelX = 0.0f;    // Center X
float modelY = 0.0f;    // Center Y
float modelZ = 4.0f;    // Initial Z depth
float moveSpeed = 5.0f; 
float rotSpeed = 0.05f;

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

void setupModelMatrix(float x_rot, float y_rot, float z_rot, float posX, float posY, float posZ) {
    mat_identity();

    mat_perspective(SCREEN_CENTER_X,
                   SCREEN_CENTER_Y,
                   FOV_COTANGENT, 
                   1.0f,
                   10000.0f);

    mat_translate(posX, posY, -posZ);
    mat_rotate_x(x_rot);
    mat_rotate_y(y_rot);
    mat_rotate_z(z_rot);
    mat_scale(DEFAULT_MODEL_SCALE, DEFAULT_MODEL_SCALE, DEFAULT_MODEL_SCALE);
}

void RenderDMSModel(const DMSModel* model, float rotX, float rotY, float rotZ, 
    float posX, float posY, float posZ, pvr_dr_state_t* dr_state) {
    if (!model) return;

    setupModelMatrix(rotX, rotY, rotZ, posX, posY, posZ);

    for (int m = 0; m < model->meshCount; m++) {
        const DMSMesh* mesh = &model->meshes[m];
        
        kos_texture_t* texture = NULL;
        if (mesh->textureId >= 0 && mesh->textureId < model->textureCount) {
            texture = model->textures[mesh->textureId];
        }
        
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
                uint32_t idx1 = mesh->indices[i] & 0x00FFFFFF;
                uint32_t idx2 = mesh->indices[i + 1] & 0x00FFFFFF;
                uint32_t idx3 = mesh->indices[i + 2] & 0x00FFFFFF;
                
                const DMSVertex* v1 = &vertexBuffer[idx1];
                const DMSVertex* v2 = &vertexBuffer[idx2];
                const DMSVertex* v3 = &vertexBuffer[idx3];
                
                pvr_vertex_t *vert = (pvr_vertex_t *)pvr_dr_target(*dr_state);
                vert->flags = PVR_CMD_VERTEX;
                mat_trans_single3_nomod(v1->x, v1->y, v1->z, vert->x, vert->y, vert->z);
                vert->u = v1->u;
                vert->v = v1->v;
                vert->argb = 0xFFFFFFFF;
                pvr_dr_commit(vert);
                
                vert = (pvr_vertex_t *)pvr_dr_target(*dr_state);
                vert->flags = PVR_CMD_VERTEX;
                mat_trans_single3_nomod(v2->x, v2->y, v2->z, vert->x, vert->y, vert->z);
                vert->u = v2->u;
                vert->v = v2->v;
                vert->argb = 0xFFFFFFFF;
                pvr_dr_commit(vert);
                
                vert = (pvr_vertex_t *)pvr_dr_target(*dr_state);
                vert->flags = PVR_CMD_VERTEX_EOL;
                mat_trans_single3_nomod(v3->x, v3->y, v3->z, vert->x, vert->y, vert->z);
                vert->u = v3->u;
                vert->v = v3->v;
                vert->argb = 0xFFFFFFFF;
                pvr_dr_commit(vert);
                
                i += 3;
            }
        }
    }
}

int main(int argc, char **argv) {
    pvr_init_params_t params = {
        { PVR_BINSIZE_32, PVR_BINSIZE_0, PVR_BINSIZE_0, PVR_BINSIZE_0, PVR_BINSIZE_0 },
        1024 * 1024 * 2.9,  // adjust vertex buffer
        0,            // DMA enabled
        0,            // FSAA disabled
        1,            // Autosort enabled
        4             // Allow for  overflow bins
    };
    
    pvr_init(&params);
 
    // Load the 3D model
    gModel = LoadDMSModel("/rd/darkseid.dms");
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
            sprintf(texturePath, "/rd/darkseid%d.dt", i);
            
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
        // not needed tbh.
        if (!pvrtex_load("/rd/fox2.dt", &model_texture)) {
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
    
    // Animation cycling variables
    uint32_t anim_button_last_press = 0;
    uint32_t anim_button_delay = 500; 
    int current_animation = 0;
    int animation_count = 0;
    
    // Get animation count if model has a skeleton
    if (gModel && gModel->skeleton) {
        animation_count = gModel->skeleton->animCount;
        printf("Model has %d animations\n", animation_count);
    }

    float rotX = 0.0f, rotY = 0.0f, rotZ = 0.0f;
    
    uint32 frames = 0;
    uint32 last_time = timer_ms_gettime64();
    float fps = 0.0f;

    while(1) {
        vid_border_color(0, 0, 0);
        pvr_wait_ready();
        vid_border_color(255, 0, 0);

        pvr_scene_begin();
        
        // First render the 3D model
        pvr_list_begin(PVR_LIST_OP_POLY);
        
        pvr_dr_state_t dr_state;
        pvr_dr_init(&dr_state);
        
        if (gModel && gModel->skeleton && gModel->skeleton->animCount > 0) {
            float dt = 1.0f / 60.0f; 
            UpdateDMSModelAnimation(gModel, dt);
            
            for (int i = 0; i < gModel->meshCount; i++) {
                UpdateDMSMeshAnimation(&gModel->meshes[i], gModel->skeleton);
            }
        }

        RenderDMSModel(gModel, rotX, rotY, rotZ, modelX, modelY, modelZ, &dr_state);
        
        pvr_scene_finish();
        vid_border_color(0, 255, 0);

        frames++;
        uint32 current_time = timer_ms_gettime64();
        if (current_time - last_time >= 2000) {  
            fps = (frames * 1000.0f) / (current_time - last_time);
            
            printf("FPS: %.2f\n", fps);
            
            frames = 0;
            last_time = current_time;
        }
        
        // Controller handling
        maple_device_t* cont = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        if (cont) {
            cont_state_t* state = (cont_state_t *)maple_dev_status(cont);
            if (state) {
                if (state->buttons & CONT_DPAD_LEFT)
                    modelX -= moveSpeed * modelZ * 0.001f; 
                if (state->buttons & CONT_DPAD_RIGHT)
                    modelX += moveSpeed * modelZ * 0.001f; 

                if (state->buttons & CONT_DPAD_UP)
                    modelY += moveSpeed * modelZ * 0.001f;
                if (state->buttons & CONT_DPAD_DOWN)
                    modelY -= moveSpeed * modelZ * 0.001f;

                if ((state->buttons & CONT_A) && 
                    (current_time - anim_button_last_press > anim_button_delay) &&
                    animation_count > 0) {
                    
                    current_animation = (current_animation + 1) % animation_count;
                    if (gModel && gModel->skeleton) {
                        gModel->skeleton->currentAnim = current_animation;
                        gModel->skeleton->currentTime = 0.0f; // Reset animation time
                        printf("Switched to animation %d\n", current_animation);
                    }
                    anim_button_last_press = current_time;
                }

                if (state->buttons & CONT_B)
                    rotY += rotSpeed;

                if (state->buttons & CONT_X)
                    rotX -= rotSpeed;
                if (state->buttons & CONT_Y)
                    rotX += rotSpeed;
                    
                if (state->ltrig > 0)
                    modelZ -= moveSpeed * modelZ * 0.01f;
                if (state->rtrig > 0)
                    modelZ += moveSpeed * modelZ * 0.01f;

                rotY = 3.14f;

                if (modelZ < 1.0f) modelZ = 1.0f;
                if (modelZ > 20.0f) modelZ = 20.0f;
    
                if (state->buttons & CONT_START) 
                    break;
            }
        }
    }
    
    // Free textures
    if (gModel && gModel->textures) {
        for (int i = 0; i < gModel->textureCount; i++) {
            if (gModel->textures[i]) {
                free(gModel->textures[i]);
            }
        }
        free(gModel->textures);
    }

    pvr_shutdown();
    return 0;
}