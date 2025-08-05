#ifndef DMS_H
#define DMS_H

#include <kos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "raymath.h"

#define DMS_MAGIC_NUMBER 0x54534D44

// Texture structure for Dreamcast
typedef struct {
    pvr_ptr_t ptr;
    int w, h;
    uint32 fmt;
} kos_texture_t;


// Transform structure (position, rotation, scale)
typedef struct {
    Vector3 translation;
    Quaternion rotation;
    Vector3 scale;
} Transform;

// Bone structure for skeletal animation
typedef struct {
    char name[64];
    int parent;
    Transform bindPose;
    Transform localPose;
    Matrix __attribute__((aligned(32))) worldPose;
    Matrix __attribute__((aligned(32))) inverseBindMatrix;
} Bone;

// Animation structure
typedef struct {
    char name[32];
    int boneCount;
    int frameCount;
    float duration;
    Transform** framePoses;
} Animation;

// Skeleton structure
typedef struct {
    Bone* bones;
    int boneCount;
    Animation* animations;
    int animCount;
    int currentAnim;
    float currentTime;
} Skeleton;

// Vertex structure - 32 bytes total, aligned  
typedef struct __attribute__((packed, aligned(32))) {
    float x, y, z;          // Position (12 bytes)
    int8_t nx, ny, nz;      // Packed normals (3 bytes)
    uint8_t pad;            // Padding (1 byte)
    float u, v;             // Texture coordinates (8 bytes)
    uint8_t boneId;         // Bone index (1 byte)
    uint8_t pad2[3];        // Padding (3 bytes)
    float boneWeight;       // Bone weight (4 bytes)
} DMSVertex;                 // Total: 32 bytes

// Mesh structure
typedef struct {
    DMSVertex* vertices;         // Bind-pose data
    DMSVertex* animatedVertices; // CPU-skinned results
    unsigned int* indices;
    int vertexCount;
    int indexCount;
    unsigned int triangleCount;  // Precomputed number of triangles
    int textureId;               // Reference to texture in model
    
    // Bounding sphere data
    Vector3 boundingCenter;      // Center of bounding sphere
    float boundingRadius;        // Radius of bounding sphere
} DMSMesh;

// Model structure
typedef struct {
    DMSMesh* meshes;
    int meshCount;
    Skeleton* skeleton;
    kos_texture_t** textures;
    int textureCount;           // Number of textures
} DMSModel;


DMSModel* LoadDMSModel(const char* filename);



 
void UpdateDMSModelAnimation(DMSModel* model, float deltaTime);

void UpdateDMSMeshAnimation(DMSMesh* mesh, const Skeleton* skeleton);




void UnloadDMSModel(DMSModel* model);


void mat_mult(const Matrix* matrix1, const Matrix* matrix2, Matrix* dst);

#endif // DMS_H