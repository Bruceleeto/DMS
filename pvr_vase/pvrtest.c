#include "dms.h"
#include "pvrtex.h"

static DMSModel* gVaseModel = NULL;

const float SCREEN_CENTER_X = 320.0f;
const float SCREEN_CENTER_Y = 240.0f;
const float FOV_COTANGENT = 1.732050808f;   
const float DEFAULT_MODEL_SCALE = 1.6f;
float modelX = 0.0f;
float modelY = 0.0f;
float modelZ = 5.0f;
float moveSpeed = 0.05f; 
float rotSpeed = 0.05f;

static kos_texture_t* background_texture = NULL;
static kos_texture_t* glass_texture = NULL;
static kos_texture_t* reflection_texture = NULL;

void setupRenderState(pvr_dr_state_t* dr_state, kos_texture_t* texture, int list_type) {
    pvr_poly_cxt_t cxt;
    memset(&cxt, 0, sizeof(cxt));
    
    if (texture) {
        pvr_poly_cxt_txr(&cxt, list_type,
                         texture->fmt,
                         texture->w, texture->h,
                         texture->ptr, PVR_FILTER_BILINEAR);
    } else {
        pvr_poly_cxt_col(&cxt, list_type);
    }
    
    cxt.gen.culling = PVR_CULLING_NONE;
    cxt.depth.comparison = PVR_DEPTHCMP_GEQUAL;
    cxt.depth.write = PVR_DEPTHWRITE_ENABLE;

    if (list_type == PVR_LIST_TR_POLY) {
        cxt.blend.src = PVR_BLEND_SRCALPHA;
        cxt.blend.dst = PVR_BLEND_INVSRCALPHA;
    } else {
        cxt.gen.alpha = PVR_ALPHA_DISABLE;
    }

    pvr_poly_hdr_t *hdr = (pvr_poly_hdr_t*)pvr_dr_target(*dr_state);
    pvr_poly_compile(hdr, &cxt);
    pvr_dr_commit(hdr);
}


void setupReflectionState(pvr_dr_state_t* dr_state, kos_texture_t* texture) {
    pvr_poly_cxt_t cxt;
    memset(&cxt, 0, sizeof(cxt));
    
    pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY,
                     texture->fmt,
                     texture->w, texture->h,
                     texture->ptr, PVR_FILTER_BILINEAR);
    
    cxt.gen.culling = PVR_CULLING_NONE;
    cxt.depth.comparison = PVR_DEPTHCMP_GEQUAL;
    cxt.depth.write = PVR_DEPTHWRITE_ENABLE;
    
    // Reflection pass blending 
    cxt.blend.src = PVR_BLEND_INVDESTALPHA;
    cxt.blend.dst = PVR_BLEND_DESTALPHA;
    
    pvr_poly_hdr_t *hdr = (pvr_poly_hdr_t*)pvr_dr_target(*dr_state);
    pvr_poly_compile(hdr, &cxt);
    pvr_dr_commit(hdr);
}


void setupModelMatrix(float scale, float x_rot, float y_rot, float z_rot, float posX, float posY, float posZ) {
    mat_identity();
    mat_perspective(SCREEN_CENTER_X, SCREEN_CENTER_Y, FOV_COTANGENT, 1.0f, 1000.0f);
    mat_translate(posX, posY, -posZ);
    mat_rotate_x(x_rot);
    mat_rotate_y(y_rot);
    mat_rotate_z(z_rot);
    mat_scale(scale, scale, scale);
}

void draw_background_texture(kos_texture_t* bgtex) {
    if (!bgtex) return;  

    pvr_poly_cxt_t cxt;
    pvr_poly_cxt_txr(&cxt,
                     PVR_LIST_OP_POLY,
                     bgtex->fmt,
                     bgtex->w,
                     bgtex->h,
                     bgtex->ptr,
                     PVR_FILTER_BILINEAR);

    cxt.gen.culling        = PVR_CULLING_NONE;
    cxt.depth.comparison   = PVR_DEPTHCMP_ALWAYS;
    cxt.depth.write        = PVR_DEPTHWRITE_DISABLE;
    cxt.gen.alpha          = PVR_ALPHA_DISABLE;

    pvr_poly_hdr_t hdr;
    pvr_poly_compile(&hdr, &cxt);
    pvr_prim(&hdr, sizeof(hdr));

    pvr_vertex_t v[4];

    v[0].flags = PVR_CMD_VERTEX;
    v[0].x     = 0.0f;
    v[0].y     = 0.0f;
    v[0].z     = 1.0f;
    v[0].u     = 0.0f;
    v[0].v     = 0.0f;
    v[0].argb  = 0xFFFFFFFF;
    v[0].oargb = 0;

    v[1].flags = PVR_CMD_VERTEX;
    v[1].x     = 640.0f;
    v[1].y     = 0.0f;
    v[1].z     = 1.0f;
    v[1].u     = 1.0f;
    v[1].v     = 0.0f;
    v[1].argb  = 0xFFFFFFFF;
    v[1].oargb = 0;

    v[2].flags = PVR_CMD_VERTEX;
    v[2].x     = 0.0f;
    v[2].y     = 480.0f;
    v[2].z     = 1.0f;
    v[2].u     = 0.0f;
    v[2].v     = 1.0f;
    v[2].argb  = 0xFFFFFFFF;
    v[2].oargb = 0;

    v[3].flags = PVR_CMD_VERTEX_EOL;
    v[3].x     = 640.0f;
    v[3].y     = 480.0f;
    v[3].z     = 1.0f;
    v[3].u     = 1.0f;
    v[3].v     = 1.0f;
    v[3].argb  = 0xFFFFFFFF;
    v[3].oargb = 0;

    pvr_prim(&v[0], sizeof(v));
}

void RenderDMSMesh(const DMSModel* model, int meshIndex, float scale, float rotX, float rotY, float rotZ, 
    float posX, float posY, float posZ, 
    pvr_dr_state_t* dr_state, bool use_env_mapping) {
    if (!model || meshIndex >= model->meshCount) return;
/* 
    if (use_env_mapping && model->meshes[meshIndex].vertexCount > 0) {
        const DMSVertex* vertexBuffer = model->skeleton ? 
            model->meshes[meshIndex].animatedVertices : 
            model->meshes[meshIndex].vertices;
        

    }

     */

    setupModelMatrix(scale, rotX, rotY, rotZ, posX, posY, posZ);

    const DMSMesh* mesh = &model->meshes[meshIndex];
    const DMSVertex* vertexBuffer = model->skeleton ? mesh->animatedVertices : mesh->vertices;

    float envMat[16];
    memset(envMat, 0, sizeof(envMat));
    envMat[0] = envMat[5] = envMat[10] = envMat[15] = 1.0f;

    if (use_env_mapping) {
        float cx = fcos(rotX), sx = fsin(rotX);
        float cy = fcos(rotY), sy = fsin(rotY);
        float cz = fcos(rotZ), sz = fsin(rotZ);

        envMat[0] = cy*cz;
        envMat[1] = cy*sz;
        envMat[2] = -sy;

        envMat[4] = sx*sy*cz - cx*sz;
        envMat[5] = sx*sy*sz + cx*cz;
        envMat[6] = sx*cy;

        envMat[8]  = cx*sy*cz + sx*sz;
        envMat[9]  = cx*sy*sz - sx*cz;
        envMat[10] = cx*cy;

        for(int i=0; i<12; i++) {
            envMat[i] *= 0.5f; // tweak
        }
    }

    int i = 0;
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
                
                if (use_env_mapping) {
                    // Normalize the normal before calculating texture coordinates
                    float nx = v->nx, ny = v->ny, nz = v->nz;
                    float len = fsqrt(nx*nx + ny*ny + nz*nz);
                    
                    if (len > 0.0001f) {
                        nx /= len;
                        ny /= len;
                        nz /= len;
                    } else {
                        nx = 0.0f;
                        ny = 0.0f;
                        nz = 1.0f;
                    }
                    
                    vert->u = (nx * envMat[0] + ny * envMat[4] + nz * envMat[8] + 0.5f);
                    vert->v = (nx * envMat[1] + ny * envMat[5] + nz * envMat[9] + 0.5f);
                    
                    // Make the reflection fully opaque for the glass part (textureId 0)
                    if (model->meshes[meshIndex].textureId == 0) {
                        vert->argb = 0xFFFFFFFF;  // Fully opaque for glass reflection
                    } else {
                        vert->argb = 0xE0FFFFFF;  // Regular opacity for silver
                    }
                } else {
                    vert->u = 1.0f - v->u;  // Flip horizontally
                    vert->v = v->v;
                    vert->argb = 0xC0FFFFFF;  // Original opacity
                }
                
                pvr_dr_commit(vert);
            }
            
            i += stripLength;
        } else {
            // Triangle list (3 vertices at a time)
            if (i + 2 < mesh->indexCount) {
                uint32_t idx1 = mesh->indices[i] & 0x00FFFFFF;
                uint32_t idx2 = mesh->indices[i + 1] & 0x00FFFFFF;
                uint32_t idx3 = mesh->indices[i + 2] & 0x00FFFFFF;
                
                const DMSVertex* v1 = &vertexBuffer[idx1];
                const DMSVertex* v2 = &vertexBuffer[idx2];
                const DMSVertex* v3 = &vertexBuffer[idx3];
                
                pvr_vertex_t *vert = (pvr_vertex_t *)pvr_dr_target(*dr_state);
                vert->flags = PVR_CMD_VERTEX;
                mat_trans_single3_nomod(v1->x, v1->y, v1->z, vert->x, vert->y, vert->z);
                
                if (use_env_mapping) {
                    // Normalize the normal before calculating texture coordinates
                    float nx = v1->nx, ny = v1->ny, nz = v1->nz;
                    float len = fsqrt(nx*nx + ny*ny + nz*nz);
                    
                    if (len > 0.0001f) {
                        nx /= len;
                        ny /= len;
                        nz /= len;
                    } else {
                        // Default normal if length is too small
                        nx = 0.0f;
                        ny = 0.0f;
                        nz = 1.0f;
                    }
                    
                    vert->u = (nx * envMat[0] + ny * envMat[4] + nz * envMat[8] + 0.5f);
                    vert->v = (nx * envMat[1] + ny * envMat[5] + nz * envMat[9] + 0.5f);
                    vert->argb = 0xE0FFFFFF;
                } else {
                    vert->u = v1->u;
                    vert->v = v1->v;
                    vert->argb = 0xC0FFFFFF;
                }
                
                pvr_dr_commit(vert);
                
                // Vertex 2
                vert = (pvr_vertex_t *)pvr_dr_target(*dr_state);
                vert->flags = PVR_CMD_VERTEX;
                mat_trans_single3_nomod(v2->x, v2->y, v2->z, vert->x, vert->y, vert->z);
                
                if (use_env_mapping) {
                    float nx = v2->nx, ny = v2->ny, nz = v2->nz;
                    float len = fsqrt(nx*nx + ny*ny + nz*nz);
                    
                    if (len > 0.0001f) {
                        nx /= len;
                        ny /= len;
                        nz /= len;
                    } else {
                        nx = 0.0f;
                        ny = 0.0f;
                        nz = 1.0f;
                    }
                    
                    vert->u = (nx * envMat[0] + ny * envMat[4] + nz * envMat[8] + 0.5f);
                    vert->v = (nx * envMat[1] + ny * envMat[5] + nz * envMat[9] + 0.5f);
                    vert->argb = 0xE0FFFFFF;
                } else {
                    vert->u = v2->u;
                    vert->v = v2->v;
                    vert->argb = 0xC0FFFFFF;
                }
                
                pvr_dr_commit(vert);
                
                vert = (pvr_vertex_t *)pvr_dr_target(*dr_state);
                vert->flags = PVR_CMD_VERTEX_EOL;
                mat_trans_single3_nomod(v3->x, v3->y, v3->z, vert->x, vert->y, vert->z);
                
                if (use_env_mapping) {
                    float nx = v3->nx, ny = v3->ny, nz = v3->nz;
                    float len = fsqrt(nx*nx + ny*ny + nz*nz);
                    
                    if (len > 0.0001f) {
                        nx /= len;
                        ny /= len;
                        nz /= len;
                    } else {
                        nx = 0.0f;
                        ny = 0.0f;
                        nz = 1.0f;
                    }
                    
                    vert->u = (nx * envMat[0] + ny * envMat[4] + nz * envMat[8] + 0.5f);
                    vert->v = (nx * envMat[1] + ny * envMat[5] + nz * envMat[9] + 0.5f);
                    vert->argb = 0xE0FFFFFF;
                } else {
                    vert->u = v3->u;
                    vert->v = v3->v;
                    vert->argb = 0xC0FFFFFF;
                }
                
                pvr_dr_commit(vert);
                
                i += 3;
            } else {
                i++;
            }
        }
    }
}


int main(int argc, char **argv) {
    // Initialize PVR
    pvr_init_params_t params = {
        {
            PVR_BINSIZE_32,  
            PVR_BINSIZE_0,  
            PVR_BINSIZE_32,  
            PVR_BINSIZE_0,
            PVR_BINSIZE_0
        },
        1024 * 1024,  // Vertex buffer size
        0, 0, 1, 4    // Other params
    };
    pvr_init(&params);
 
    // Load the vase model
    gVaseModel = LoadDMSModel("/rd/vase.dms");
    if (!gVaseModel) {
        printf("Failed to load vase model\n");
        return 1;
    }
    
    // Print mesh information
    printf("Model loaded with %d meshes\n", gVaseModel->meshCount);
    for (int i = 0; i < gVaseModel->meshCount; i++) {
        printf("Mesh %d: textureId = %d, %d vertices, %d indices\n", 
               i, 
               gVaseModel->meshes[i].textureId,
               gVaseModel->meshes[i].vertexCount,
               gVaseModel->meshes[i].indexCount);
    }
    

    dttex_info_t bgtex;
    if (pvrtex_load("/rd/web/Backgrnd.dt", &bgtex)) {
        printf("Loading background texture\n");
        background_texture = (kos_texture_t*)malloc(sizeof(kos_texture_t));
        background_texture->ptr = bgtex.ptr;
        background_texture->w = bgtex.width;
        background_texture->h = bgtex.height;
        background_texture->fmt = bgtex.pvrformat;
    } 
    
    // Glass texture (ID 0)
    dttex_info_t glasstex;
    if (pvrtex_load("/rd/web/glass.dt", &glasstex)) {
        printf("Loading glass texture\n");
        glass_texture = (kos_texture_t*)malloc(sizeof(kos_texture_t));
        glass_texture->ptr = glasstex.ptr;
        glass_texture->w = glasstex.width;
        glass_texture->h = glasstex.height;
        glass_texture->fmt = glasstex.pvrformat;
    } 
    
    // Silver texture (ID 1)
    dttex_info_t silvertex;
    if (pvrtex_load("/rd/web/reflection.dt", &silvertex)) {
        printf("Loading silver texture\n");
        reflection_texture = (kos_texture_t*)malloc(sizeof(kos_texture_t));
        reflection_texture->ptr = silvertex.ptr;
        reflection_texture->w = silvertex.width;
        reflection_texture->h = silvertex.height;
        reflection_texture->fmt = silvertex.pvrformat;
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
    
        // Draw background and opaque objects in the opaque list
        pvr_list_begin(PVR_LIST_OP_POLY);
        
        // Draw background
        draw_background_texture(background_texture);
        
        // Draw the SILVER/BASE part (textureId 1) in the opaque list
        for (int m = 0; m < gVaseModel->meshCount; m++) {
            int textureId = gVaseModel->meshes[m].textureId;
            // Silver part only (textureId 1) goes in opaque list
            if (textureId == 1 && reflection_texture) {
                pvr_dr_state_t dr_state;
                pvr_dr_init(&dr_state);
                setupRenderState(&dr_state, reflection_texture, PVR_LIST_OP_POLY);
                RenderDMSMesh(gVaseModel, m, DEFAULT_MODEL_SCALE, rotX, rotY, rotZ,
                            modelX, modelY, modelZ, &dr_state, true);  // true = use env mapping
            }
        }
        
        pvr_list_finish();
        
        // Draw the glass part in the translucent list
        pvr_list_begin(PVR_LIST_TR_POLY);

        // First pass: Alpha mask with the glass texture
        for (int m = 0; m < gVaseModel->meshCount; m++) {
            int textureId = gVaseModel->meshes[m].textureId;
            if (textureId == 0 && glass_texture) {
                pvr_dr_state_t dr_state;
                pvr_dr_init(&dr_state);
                setupRenderState(&dr_state, glass_texture, PVR_LIST_TR_POLY);
                RenderDMSMesh(gVaseModel, m, DEFAULT_MODEL_SCALE, rotX, rotY, rotZ,
                              modelX, modelY, modelZ, &dr_state, false);
            }
        }
        
        // Second pass: Reflection/environment mapped pass for glass
        for (int m = 0; m < gVaseModel->meshCount; m++) {
            int textureId = gVaseModel->meshes[m].textureId;
            if (textureId == 0 && reflection_texture) { // Using silver texture as reflection
                pvr_dr_state_t dr_state;
                pvr_dr_init(&dr_state);
                setupReflectionState(&dr_state, reflection_texture);
                RenderDMSMesh(gVaseModel, m, DEFAULT_MODEL_SCALE, rotX, rotY, rotZ,
                              modelX, modelY, modelZ, &dr_state, true); // true = use env mapping
            }
        }
        
        pvr_list_finish();
        
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
                    modelX -= moveSpeed;
                if (state->buttons & CONT_DPAD_RIGHT)
                    modelX += moveSpeed;
                if (state->buttons & CONT_DPAD_UP)
                    modelY += moveSpeed;
                if (state->buttons & CONT_DPAD_DOWN)
                    modelY -= moveSpeed;

                if (state->buttons & CONT_A)
                    rotY -= rotSpeed;
                if (state->buttons & CONT_B)
                    rotY += rotSpeed;
                if (state->buttons & CONT_X)
                    rotX -= rotSpeed;
                if (state->buttons & CONT_Y)
                    rotX += rotSpeed;
                    
                if (state->ltrig > 0)
                    modelZ -= moveSpeed;
                if (state->rtrig > 0)
                    modelZ += moveSpeed;

                if (modelZ < 1.0f) modelZ = 1.0f;
                if (modelZ > 20.0f) modelZ = 20.0f;
    
                if (state->buttons & CONT_START) 
                    break;
            }
        }
    }
    
    // Free textures
    if (reflection_texture) {
        pvr_mem_free(reflection_texture->ptr);
        free(reflection_texture);
    }
    
    if (glass_texture) {
        pvr_mem_free(glass_texture->ptr);
        free(glass_texture);
    }
    
    if (background_texture) {
        pvr_mem_free(background_texture->ptr);
        free(background_texture);
    }
    
    // Free model
    if (gVaseModel) {
        for (int m = 0; m < gVaseModel->meshCount; m++) {
            if (gVaseModel->meshes[m].vertices)
                free(gVaseModel->meshes[m].vertices);
            if (gVaseModel->meshes[m].animatedVertices)
                free(gVaseModel->meshes[m].animatedVertices);
            if (gVaseModel->meshes[m].indices)
                free(gVaseModel->meshes[m].indices);
        }
        free(gVaseModel->meshes);
        free(gVaseModel);
    }

    pvr_shutdown();
    return 0;
}