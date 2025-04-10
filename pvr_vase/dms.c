#include "dms.h"


void mat_mult(const Matrix* matrix1, const Matrix* matrix2, Matrix* dst) {
    mat_load((matrix_t*)matrix1);
    mat_apply((matrix_t*)matrix2);
    mat_store((matrix_t*)dst);
}

DMSModel* LoadDMSModel(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        printf("Failed to open %s\n", filename);
        return NULL;
    }

    uint32_t magic, version, meshCount, boneCount;
    fread(&magic, sizeof(uint32_t), 1, file);
    fread(&version, sizeof(uint32_t), 1, file);
    fread(&meshCount, sizeof(uint32_t), 1, file);
    fread(&boneCount, sizeof(uint32_t), 1, file);

    if (magic != 0x54534D44) { // "DMS\0" in little endian
        printf("Invalid file format: magic mismatch\n");
        fclose(file);
        return NULL;
    }

    DMSModel* model = (DMSModel*)calloc(1, sizeof(DMSModel));
    model->meshCount = meshCount;
    model->meshes = (DMSMesh*)calloc(meshCount, sizeof(DMSMesh));

    // If skeleton
    if (boneCount > 0) {
        model->skeleton = (Skeleton*)calloc(1, sizeof(Skeleton));
        model->skeleton->boneCount = boneCount;
        model->skeleton->bones = (Bone*)calloc(boneCount, sizeof(Bone));

        // Read bones
        for (uint32_t i = 0; i < boneCount; i++) {
            Bone* b = &model->skeleton->bones[i];
            fread(b->name, sizeof(char), 64, file);
            fread(&b->parent, sizeof(int), 1, file);
            fread(&b->bindPose, sizeof(Transform), 1, file);
            fread(&b->inverseBindMatrix, sizeof(Matrix), 1, file);
        }

        // Animations
        uint32_t animCount = 0;
        fread(&animCount, sizeof(uint32_t), 1, file);
        if (animCount > 0) {
            model->skeleton->animCount = animCount;
            model->skeleton->animations = (Animation*)calloc(animCount, sizeof(Animation));

            for (uint32_t i = 0; i < animCount; i++) {
                Animation* A = &model->skeleton->animations[i];
                fread(A->name, sizeof(char), 32, file);
                fread(&A->boneCount, sizeof(int), 1, file);
                fread(&A->frameCount, sizeof(int), 1, file);
                fread(&A->duration, sizeof(float), 1, file);

                size_t totalPoses = A->frameCount * A->boneCount;
                Transform* T = (Transform*)calloc(totalPoses, sizeof(Transform));
                fread(T, sizeof(Transform), totalPoses, file);
                A->framePoses = (Transform**)T;
            }
            // default anim
            model->skeleton->currentAnim = 0;
            model->skeleton->currentTime = 0.0f;
        }
    } else {
        // No skeleton - static model
        printf("Loading static model (no skeleton)\n");
        model->skeleton = NULL;
        
        // Still need to read past animation count field
        uint32_t animCount = 0;
        fread(&animCount, sizeof(uint32_t), 1, file);
    }

    // Meshes
    for (uint32_t m = 0; m < meshCount; m++) {
        DMSMesh* mesh = &model->meshes[m];
        fread(&mesh->vertexCount, sizeof(uint32_t), 1, file);
        fread(&mesh->indexCount, sizeof(uint32_t), 1, file);
        fread(&mesh->textureId, sizeof(int), 1, file);  // Read texture ID

        mesh->vertices = (DMSVertex*)calloc(mesh->vertexCount, sizeof(DMSVertex));
        
        // For animated models,  allocate animated vertices buffer
        if (model->skeleton) {
            mesh->animatedVertices = (DMSVertex*)calloc(mesh->vertexCount, sizeof(DMSVertex));
            
            // Read full Vertex1 data for animated models
            fread(mesh->vertices, sizeof(DMSVertex), mesh->vertexCount, file);
            
            // Initialize animated vertices with bind pose
            for (int i = 0; i < mesh->vertexCount; i++) {
                mesh->animatedVertices[i] = mesh->vertices[i];
            }
        } else {
            // For static models, read simplified StaticVertex data and convert
            typedef struct {
                float x, y, z;        // Position
                float nx, ny, nz;     // Normal
                float u, v;           // Texture coordinates
            } StaticVertex;

            StaticVertex* tempVerts = (StaticVertex*)malloc(mesh->vertexCount * sizeof(StaticVertex));
            fread(tempVerts, sizeof(StaticVertex), mesh->vertexCount, file);
            
            // Convert to Vertex1 format (with default bone data)
            for (int i = 0; i < mesh->vertexCount; i++) {
                mesh->vertices[i].x = tempVerts[i].x;
                mesh->vertices[i].y = tempVerts[i].y;
                mesh->vertices[i].z = tempVerts[i].z;
                mesh->vertices[i].nx = tempVerts[i].nx;
                mesh->vertices[i].ny = tempVerts[i].ny;
                mesh->vertices[i].nz = tempVerts[i].nz;
                mesh->vertices[i].u = tempVerts[i].u;
                mesh->vertices[i].v = tempVerts[i].v;
                mesh->vertices[i].boneId = 0;
                mesh->vertices[i].boneWeight = 0.0f;
            }
            
            free(tempVerts);
            mesh->animatedVertices = NULL; // Static models don't need this
        }

        int maxTextureId = -1;
        for (uint32_t m = 0; m < meshCount; m++) {
            if (model->meshes[m].textureId > maxTextureId) {
                maxTextureId = model->meshes[m].textureId;
            }
        }
        
        // Set texture count and allocate texture array
        model->textureCount = (maxTextureId >= 0) ? (maxTextureId + 1) : 0;
        if (model->textureCount > 0) {
            model->textures = (kos_texture_t**)calloc(model->textureCount, sizeof(kos_texture_t*));
        }

        // Read indices
        mesh->indices = (unsigned int*)calloc(mesh->indexCount, sizeof(unsigned int));
        fread(mesh->indices, sizeof(unsigned int), mesh->indexCount, file);

        // Precompute triangle count for this mesh
        unsigned int triCount = 0;
        if (mesh->indices == NULL || mesh->indexCount <= 0) {
            triCount = mesh->vertexCount / 3;
        } else {
            int i = 0;
            while (i < mesh->indexCount) {
                uint32_t rawIndex = mesh->indices[i];
                int isStrip = (rawIndex & 0x80000000) != 0;
                uint32_t sId = (rawIndex >> 24) & 0x7F;

                if (isStrip) {
                    // Start of a TRIANGLE_STRIP
                    int stripLength = 0;

                    while (i < mesh->indexCount) {
                        rawIndex = mesh->indices[i];
                        isStrip = (rawIndex & 0x80000000) != 0;
                        uint32_t thisSid = (rawIndex >> 24) & 0x7F;
                        if (!isStrip || thisSid != sId)
                            break;
                        stripLength++;
                        i++;
                    }

                    if (stripLength >= 3)
                        triCount += (stripLength - 2);
                }
                else {
                    // Normal triangle list
                    if (i + 2 < mesh->indexCount) {
                        triCount += 1;
                        i += 3;
                    }
                    else {
                        i++;
                    }
                }
            }
        }
        mesh->triangleCount = triCount;
    }

    fclose(file);
    return model;
}

void UpdateDMSModelAnimation(DMSModel* model, float deltaTime) {
    if (!model || !model->skeleton || model->skeleton->animCount == 0) return;

    Skeleton* sk = model->skeleton;
    Animation* anim = &sk->animations[sk->currentAnim];
    sk->currentTime += deltaTime;

    // Loop
    while (sk->currentTime >= anim->duration) {
        sk->currentTime -= anim->duration;
    }

    if (anim->frameCount < 2) return;

    float timePerFrame = anim->duration / (float)anim->frameCount;
    int frame = (int)(sk->currentTime / timePerFrame);
    int nextFrame = (frame + 1) % anim->frameCount;
    float alpha = fmodf(sk->currentTime, timePerFrame) / timePerFrame;

    for (int i = 0; i < sk->boneCount; i++) {
        Bone* bone = &sk->bones[i];
        Transform* curr = &((Transform*)anim->framePoses)[frame * sk->boneCount + i];
        Transform* nxt  = &((Transform*)anim->framePoses)[nextFrame * sk->boneCount + i];

        // Interp
        bone->localPose.translation = Vector3Lerp(curr->translation, nxt->translation, alpha);
        bone->localPose.rotation    = QuaternionSlerp(curr->rotation, nxt->rotation, alpha);
        bone->localPose.scale       = Vector3Lerp(curr->scale, nxt->scale, alpha);

        Matrix __attribute__((aligned(32))) S = MatrixScale(bone->localPose.scale.x, bone->localPose.scale.y, bone->localPose.scale.z);
        Matrix __attribute__((aligned(32))) R = QuaternionToMatrix(bone->localPose.rotation);
        Matrix __attribute__((aligned(32))) T = MatrixTranslate(bone->localPose.translation.x, bone->localPose.translation.y, bone->localPose.translation.z);
        Matrix __attribute__((aligned(32))) SR, local;
        
        mat_mult(&S, &R, &SR);
        mat_mult(&SR, &T, &local);
        
        if (bone->parent >= 0) {
            mat_mult(&local, &sk->bones[bone->parent].worldPose, &bone->worldPose);
        } else {
            bone->worldPose = local;
        }
    }
}





void UpdateDMSMeshAnimation(DMSMesh* mesh, const Skeleton* sk) {
    if (!mesh || !sk) return;

    //  an aligned matrix for hardware operations
    static Matrix __attribute__((aligned(32))) tempMatrix;

    for (int i = 0; i < mesh->vertexCount; i++) {
        mesh->animatedVertices[i] = mesh->vertices[i];

        // If weighted
        if (mesh->vertices[i].boneWeight > 0.0f) {
            uint8_t bId = mesh->vertices[i].boneId;
            if (bId < sk->boneCount) {
                mat_mult(&sk->bones[bId].inverseBindMatrix, &sk->bones[bId].worldPose, &tempMatrix);
                
                Vector3 bindPos = (Vector3){mesh->vertices[i].x, mesh->vertices[i].y, mesh->vertices[i].z};
                Vector3 newPos = Vector3Transform(bindPos, tempMatrix);
                mesh->animatedVertices[i].x = newPos.x;
                mesh->animatedVertices[i].y = newPos.y;
                mesh->animatedVertices[i].z = newPos.z;
            }
        }
    }
}



