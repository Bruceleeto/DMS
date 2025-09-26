#include "dms.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>    
#include <GL/gl.h>
#include "gl_png.h"   

// Load DMS model from file
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

    if (magic != DMS_MAGIC_NUMBER) {
        printf("Invalid file format: magic mismatch 0x%08lX vs 0x%08X\n", 
               (unsigned long)magic, DMS_MAGIC_NUMBER);
        fclose(file);
        return NULL;
    }

    DMSModel* model = (DMSModel*)calloc(1, sizeof(DMSModel));
    model->meshCount = meshCount;
    model->meshes = (DMSMesh*)calloc(meshCount, sizeof(DMSMesh));

    printf("Loading DMS model with %lu meshes and %lu bones\n", 
           (unsigned long)meshCount, (unsigned long)boneCount);

    // Load skeleton if present
    if (boneCount > 0) {
        model->skeleton = (DMSSkeleton*)calloc(1, sizeof(DMSSkeleton));
        model->skeleton->boneCount = boneCount;
        model->skeleton->bones = (DMSBone*)calloc(boneCount, sizeof(DMSBone));

        // Read bones
        for (uint32_t i = 0; i < boneCount; i++) {
            DMSBone* bone = &model->skeleton->bones[i];
            fread(bone->name, sizeof(char), 64, file);
            fread(&bone->parent, sizeof(int), 1, file);
            fread(&bone->bindPose, sizeof(DMSTransform), 1, file);
            fread(&bone->inverseBindMatrix, sizeof(Matrix), 1, file);
        }

        // Read animations
        uint32_t animCount;
        fread(&animCount, sizeof(uint32_t), 1, file);
        
        if (animCount > 0) {
            model->skeleton->animCount = animCount;
            model->skeleton->animations = (DMSAnimation*)calloc(animCount, sizeof(DMSAnimation));

            for (uint32_t i = 0; i < animCount; i++) {
                DMSAnimation* anim = &model->skeleton->animations[i];
                
                fread(anim->name, sizeof(char), 32, file);
                fread(&anim->boneCount, sizeof(int), 1, file);
                fread(&anim->frameCount, sizeof(int), 1, file);
                fread(&anim->duration, sizeof(float), 1, file);

                size_t totalPoses = anim->frameCount * anim->boneCount;
                DMSTransform* poses = (DMSTransform*)calloc(totalPoses, sizeof(DMSTransform));
                fread(poses, sizeof(DMSTransform), totalPoses, file);
                anim->framePoses = (DMSTransform**)poses;
                
                printf("  Animation %lu: %s, %d frames, duration %.2fs\n", 
                       (unsigned long)i, anim->name, anim->frameCount, anim->duration);
            }
            
            // Initialize animation playback
            model->skeleton->currentAnim = 0;
            model->skeleton->currentTime = 0.0f;
        }
    } else {
        model->skeleton = NULL;
        
        uint32_t animCount;
        fread(&animCount, sizeof(uint32_t), 1, file);
    }

    // Find the highest texture ID to determine texture count
    int maxTextureId = -1;

    // Load mesh data
    for (uint32_t m = 0; m < meshCount; m++) {
        DMSMesh* mesh = &model->meshes[m];
        
        fread(&mesh->vertexCount, sizeof(uint32_t), 1, file);
        fread(&mesh->indexCount, sizeof(uint32_t), 1, file);
        fread(&mesh->textureId, sizeof(int), 1, file);
        
        if (mesh->textureId > maxTextureId) {
            maxTextureId = mesh->textureId;
        }

        printf("  Mesh %lu: %d vertices, %d indices, texture ID %d\n", 
               (unsigned long)m, mesh->vertexCount, mesh->indexCount, mesh->textureId);

        // Allocate and load vertices
        if (mesh->vertexCount > 0) {
            mesh->vertices = (DMSVertex*)memalign(32, mesh->vertexCount * sizeof(DMSVertex));
            memset(mesh->vertices, 0, mesh->vertexCount * sizeof(DMSVertex));
            
            if (model->skeleton) {
                // Animated model - read full vertex data
                mesh->animatedVertices = (DMSVertex*)calloc(mesh->vertexCount, sizeof(DMSVertex));
                fread(mesh->vertices, sizeof(DMSVertex), mesh->vertexCount, file);
                
                // Initialize animated vertices with bind pose
                for (int i = 0; i < mesh->vertexCount; i++) {
                    mesh->animatedVertices[i] = mesh->vertices[i];
                }
            } else {
                // Static model - read simplified vertex data
                typedef struct {
                    float x, y, z;        // Position
                    float nx, ny, nz;     // Normal
                    float u, v;           // Texture coordinates
                } StaticVertex;

                StaticVertex* tempVerts = (StaticVertex*)malloc(mesh->vertexCount * sizeof(StaticVertex));
                fread(tempVerts, sizeof(StaticVertex), mesh->vertexCount, file);
                
                // Convert to DMSVertex format
                for (int i = 0; i < mesh->vertexCount; i++) {
                    mesh->vertices[i].x = tempVerts[i].x;
                    mesh->vertices[i].y = tempVerts[i].y;
                    mesh->vertices[i].z = tempVerts[i].z;
                    mesh->vertices[i].nx = tempVerts[i].nx * 127.0f;  
                    mesh->vertices[i].ny = tempVerts[i].ny * 127.0f;
                    mesh->vertices[i].nz = tempVerts[i].nz * 127.0f;
                    mesh->vertices[i].u = tempVerts[i].u;
                    mesh->vertices[i].v = tempVerts[i].v;
                    mesh->vertices[i].boneId = 0;
                    mesh->vertices[i].boneWeight = 0.0f;
                }
                
                free(tempVerts);
                mesh->animatedVertices = NULL;
            }
        }

        // Allocate and load indices
        if (mesh->indexCount > 0) {
            mesh->indices = (unsigned int*)calloc(mesh->indexCount, sizeof(unsigned int));
            fread(mesh->indices, sizeof(unsigned int), mesh->indexCount, file);
        }
        
        mesh->triangleCount = 0; 
    }

    // Set up texture array
    model->textureCount = (maxTextureId >= 0) ? (maxTextureId + 1) : 0;
    if (model->textureCount > 0) {
        model->textures = (Texture2D*)calloc(model->textureCount, sizeof(Texture2D));
    }

    fclose(file);
    return model;
}

// Load textures for a DMS model
int LoadDMSTextures(DMSModel* model, const char* basePath, const char* defaultTexture) {
    if (!model || model->textureCount <= 0) return 0;
    
    int successCount = 0;
    printf("Loading %d textures for model\n", model->textureCount);
    
    // Load each texture
    for (int i = 0; i < model->textureCount; i++) {
        char texturePath[256];
        sprintf(texturePath, "%s/texture%d.tex", basePath, i);
        
        model->textures[i] = LoadTextureDTEX(texturePath);
        if (model->textures[i].id != 0) {
            printf("Loaded texture %d: %s\n", i, texturePath);
            successCount++;
        } else {
            printf("Failed to load texture %d: %s\n", i, texturePath);
            // Try fallback texture if provided
            if (defaultTexture) {
                model->textures[i] = LoadTextureDTEX(defaultTexture);
                if (model->textures[i].id != 0) {
                    printf("Loaded fallback texture for %d\n", i);
                    successCount++;
                }
            }
        }
    }
    
    return successCount;
}

// Update animation for a DMS model
void UpdateDMSModelAnimation(DMSModel* model, float deltaTime) {
    if (!model || !model->skeleton || model->skeleton->animCount == 0) return;

    DMSSkeleton* skeleton = model->skeleton;
    DMSAnimation* anim = &skeleton->animations[skeleton->currentAnim];
    
    // Update animation time
    skeleton->currentTime += deltaTime;
    
    // Loop animation
    while (skeleton->currentTime >= anim->duration) {
        skeleton->currentTime -= anim->duration;
    }

    if (anim->frameCount < 2) return;

    // Get current and next frame
    float timePerFrame = anim->duration / (float)anim->frameCount;
    int frame = (int)(skeleton->currentTime / timePerFrame);
    int nextFrame = (frame + 1) % anim->frameCount;
    float alpha = fmodf(skeleton->currentTime, timePerFrame) / timePerFrame;

    // Update bone transforms
    for (int i = 0; i < skeleton->boneCount; i++) {
        DMSBone* bone = &skeleton->bones[i];
        
        // Get current and next pose for interpolation
        DMSTransform* curr = &((DMSTransform*)anim->framePoses)[frame * skeleton->boneCount + i];
        DMSTransform* next = &((DMSTransform*)anim->framePoses)[nextFrame * skeleton->boneCount + i];

        // Interpolate between frames
        bone->localPose.translation = Vector3Lerp(curr->translation, next->translation, alpha);
        bone->localPose.rotation = QuaternionSlerp(curr->rotation, next->rotation, alpha);
        bone->localPose.scale = Vector3Lerp(curr->scale, next->scale, alpha);

        // Build local transformation matrix
        Matrix S = MatrixScale(bone->localPose.scale.x, bone->localPose.scale.y, bone->localPose.scale.z);
        Matrix R = QuaternionToMatrix(bone->localPose.rotation);
        Matrix T = MatrixTranslate(bone->localPose.translation.x, bone->localPose.translation.y, bone->localPose.translation.z);
        
        // Combine transformations
        Matrix localTransform = MatrixMultiply(MatrixMultiply(S, R), T);

        // Apply parent transform if any
        if (bone->parent >= 0) {
            bone->worldPose = MatrixMultiply(localTransform, skeleton->bones[bone->parent].worldPose);
        } else {
            bone->worldPose = localTransform;
        }
    }
}

// Update vertex positions for an animated mesh
void UpdateDMSMeshAnimation(DMSMesh* mesh, const DMSSkeleton* skeleton) {
    if (!mesh || !skeleton || !mesh->animatedVertices) return;

    for (int i = 0; i < mesh->vertexCount; i++) {
        // Start with bind pose
        mesh->animatedVertices[i] = mesh->vertices[i];
        
        // If vertex has bone influence
        if (mesh->vertices[i].boneWeight > 0.0f) {
            uint8_t boneId = mesh->vertices[i].boneId;
            if (boneId < skeleton->boneCount) {
                // Get bone transformation matrices
                Matrix invBind = skeleton->bones[boneId].inverseBindMatrix;
                Matrix world = skeleton->bones[boneId].worldPose;
                
                // Transform vertex position
                Vector3 pos = {mesh->vertices[i].x, mesh->vertices[i].y, mesh->vertices[i].z};
                Vector3 boneSpace = Vector3Transform(pos, invBind);
                Vector3 transformed = Vector3Transform(boneSpace, world);
                
                // Update animated position
                mesh->animatedVertices[i].x = transformed.x;
                mesh->animatedVertices[i].y = transformed.y;
                mesh->animatedVertices[i].z = transformed.z;
            }
        }
    }
}


void RenderDMSModel(DMSModel* dmsModel, Vector3 position, float scale, Color tint) {
    if (!dmsModel) return;
    
    // Disable lighting since  not using normals
    glDisable(GL_LIGHTING);
        glDisable(GL_CULL_FACE);

    glPushMatrix();
    
    // Apply model transform
    glTranslatef(position.x, position.y, position.z);
    glScalef(scale, scale, scale);
    
    // Enable client states once at the beginning
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    // glEnableClientState(GL_NORMAL_ARRAY);  // Enable if using normals
    
    // For each mesh
    for (int m = 0; m < dmsModel->meshCount; m++) {
        DMSMesh* mesh = &dmsModel->meshes[m];
        
        if (!mesh->vertices || mesh->indexCount == 0) continue;
        
        // Bind texture if available
        if (mesh->textureId >= 0 && mesh->textureId < dmsModel->textureCount && 
            dmsModel->textures[mesh->textureId].id > 0) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, dmsModel->textures[mesh->textureId].id);
        } else {
            glDisable(GL_TEXTURE_2D);
        }
        
        // Set color
        glColor4ub(tint.r, tint.g, tint.b, tint.a);
        
        // Select vertex buffer based on animation
        DMSVertex* vertexBuffer = dmsModel->skeleton ? mesh->animatedVertices : mesh->vertices;
        
        // Point to our vertex data
        glVertexPointer(3, GL_FLOAT, sizeof(DMSVertex), &vertexBuffer[0].x);
        glTexCoordPointer(2, GL_FLOAT, sizeof(DMSVertex), &vertexBuffer[0].u);
        // glNormalPointer(GL_BYTE, sizeof(DMSVertex), &vertexBuffer[0].nx); 
        
        // Process all triangle strips first
        int i = 0;
        while (i < mesh->indexCount) {
            uint32_t rawIndex = mesh->indices[i];
            
            if ((rawIndex & 0x80000000) == 0) {
                // Skip individual triangles for now
                i++;
                continue;
            }
            
            uint32_t stripId = (rawIndex >> 24) & 0x7F;
            int stripStart = i;
            int stripLength = 0;
            
            // Count strip length
            while (i < mesh->indexCount) {
                rawIndex = mesh->indices[i];
                if (!(rawIndex & 0x80000000) || ((rawIndex >> 24) & 0x7F) != stripId)
                    break;
                stripLength++;
                i++;
            }
            
            if (stripLength >= 3) {
                // Create a temporary index buffer for this strip
                unsigned int* stripIndices = (unsigned int*)alloca(stripLength * sizeof(unsigned int));
                
                for (int j = 0; j < stripLength; j++) {
                    stripIndices[j] = mesh->indices[stripStart + j] & 0x00FFFFFF;
                }
                
                // Draw the strip with one call
                glDrawElements(GL_TRIANGLE_STRIP, stripLength, GL_UNSIGNED_INT, stripIndices);
            }
        }
        
        // Now collect all individual triangles into one batch
        int triangleCount = 0;
        i = 0;
        
        // First count triangles
        while (i < mesh->indexCount) {
            if ((mesh->indices[i] & 0x80000000) == 0) {
                if (i + 2 < mesh->indexCount) {
                    triangleCount++;
                    i += 3;
                } else {
                    i++;
                }
            } else {
                // Skip strips
                uint32_t stripId = (mesh->indices[i] >> 24) & 0x7F;
                while (i < mesh->indexCount && 
                       (mesh->indices[i] & 0x80000000) && 
                       ((mesh->indices[i] >> 24) & 0x7F) == stripId) {
                    i++;
                }
            }
        }
        
        if (triangleCount > 0) {
            // Allocate temporary buffer for triangle indices
            unsigned int* triIndices = (unsigned int*)alloca(triangleCount * 3 * sizeof(unsigned int));
            int indexPos = 0;
            
            // Collect all triangle indices
            i = 0;
            while (i < mesh->indexCount) {
                if ((mesh->indices[i] & 0x80000000) == 0) {
                    if (i + 2 < mesh->indexCount) {
                        for (int j = 0; j < 3; j++) {
                            triIndices[indexPos++] = mesh->indices[i + j] & 0x00FFFFFF;
                        }
                        i += 3;
                    } else {
                        i++;
                    }
                } else {
                    // Skip strips
                    uint32_t stripId = (mesh->indices[i] >> 24) & 0x7F;
                    while (i < mesh->indexCount && 
                           (mesh->indices[i] & 0x80000000) && 
                           ((mesh->indices[i] >> 24) & 0x7F) == stripId) {
                        i++;
                    }
                }
            }
            
            // Draw all triangles at once
            glDrawElements(GL_TRIANGLES, triangleCount * 3, GL_UNSIGNED_INT, triIndices);
        }
    }
    
    // Disable client states at the end
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    // glDisableClientState(GL_NORMAL_ARRAY);  // Disable if using normals
    
    // Reset texture state
    glDisable(GL_TEXTURE_2D);
    
    glPopMatrix();
}

// Free DMS model resources
void UnloadDMSModel(DMSModel* model) {
    if (!model) return;
    
    // Free meshes
    for (int i = 0; i < model->meshCount; i++) {
        if (model->meshes[i].vertices) free(model->meshes[i].vertices);
        if (model->meshes[i].animatedVertices) free(model->meshes[i].animatedVertices);
        if (model->meshes[i].indices) free(model->meshes[i].indices);
    }
    if (model->meshes) free(model->meshes);
    
    // Free skeleton
    if (model->skeleton) {
        if (model->skeleton->bones) free(model->skeleton->bones);
        
        // Free animations
        if (model->skeleton->animations) {
            for (int i = 0; i < model->skeleton->animCount; i++) {
                if (model->skeleton->animations[i].framePoses) 
                    free(model->skeleton->animations[i].framePoses);
            }
            free(model->skeleton->animations);
        }
        
        free(model->skeleton);
    }
    
    if (model->textures) free(model->textures);
    
    free(model);
}

// Set current animation for a DMS model
int SetDMSModelAnimation(DMSModel* model, int animIndex) {
    if (!model || !model->skeleton) return 0;
    
    if (animIndex >= 0 && animIndex < model->skeleton->animCount) {
        model->skeleton->currentAnim = animIndex;
        model->skeleton->currentTime = 0.0f;
        return 1;
    }
    
    return 0;
}

// Get the number of animations in a DMS model
int GetDMSModelAnimationCount(DMSModel* model) {
    if (!model || !model->skeleton) return 0;
    return model->skeleton->animCount;
}

// Get the name of a specific animation
const char* GetDMSModelAnimationName(DMSModel* model, int animIndex) {
    if (!model || !model->skeleton) return NULL;
    
    if (animIndex >= 0 && animIndex < model->skeleton->animCount) {
        return model->skeleton->animations[animIndex].name;
    }
    
    return NULL;
}

// Get the current animation playing on a DMS model
int GetDMSModelCurrentAnimation(DMSModel* model) {
    if (!model || !model->skeleton) return -1;
    return model->skeleton->currentAnim;
}