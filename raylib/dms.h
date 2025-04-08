#ifndef DMS_H
#define DMS_H

#include <raylib.h>
#include <raymath.h>
#include <GL/gl.h>
#include <stdint.h>

// DMS File Format Magic Number ("DMST" in hex)
#define DMS_MAGIC_NUMBER 0x54534D44

// DMS Transform structure
typedef struct {
    Vector3 translation;
    Quaternion rotation;
    Vector3 scale;
} DMSTransform;

// DMS Bone structure
typedef struct {
    char name[64];
    int parent;
    DMSTransform bindPose;
    DMSTransform localPose;
    Matrix worldPose;
    Matrix inverseBindMatrix;
} DMSBone;

// DMS Animation structure
typedef struct {
    char name[32];
    int boneCount;
    int frameCount;
    float duration;
    DMSTransform** framePoses;
} DMSAnimation;

// DMS Skeleton structure
typedef struct {
    DMSBone* bones;
    int boneCount;
    DMSAnimation* animations;
    int animCount;
    int currentAnim;
    float currentTime;
} DMSSkeleton;

// DMS Vertex structure (32 bytes total)
 typedef struct {
    float x, y, z;          // Position (12 bytes)
    int8_t nx, ny, nz;      // Packed normals (3 bytes)
    uint8_t pad;            // Padding (1 byte)
    float u, v;             // Texture coordinates (8 bytes)
    uint8_t boneId;         // Bone index (1 byte)
    uint8_t pad2[3];        // Padding (3 bytes)
    float boneWeight;       // Bone weight (4 bytes)
} DMSVertex;

// DMS Mesh structure
typedef struct {
    DMSVertex* vertices;       // Bind-pose data
    DMSVertex* animatedVertices; // CPU-skinned results
    unsigned int* indices;
    int vertexCount;
    int indexCount;
    unsigned int triangleCount;
    int textureId;
} DMSMesh;

// DMS Model structure
typedef struct {
    DMSMesh* meshes;
    int meshCount;
    DMSSkeleton* skeleton;
    Texture2D* textures;
    int textureCount;
} DMSModel;

// Function prototypes

/**
 * Load a DMS model from file
 * @param filename Path to the DMS file
 * @return Pointer to loaded DMS model or NULL if loading failed
 */
DMSModel* LoadDMSModel(const char* filename);

/**
 * Load textures for a DMS model
 * @param model Pointer to the DMS model
 * @param basePath Base path for textures
 * @param defaultTexture Path to default texture if specific textures can't be loaded
 * @return Number of textures loaded successfully
 */
int LoadDMSTextures(DMSModel* model, const char* basePath, const char* defaultTexture);

/**
 * Update animation for a DMS model
 * @param model Pointer to the DMS model
 * @param deltaTime Time elapsed since last update (in seconds)
 */
void UpdateDMSModelAnimation(DMSModel* model, float deltaTime);

/**
 * Update vertex positions for an animated mesh
 * @param mesh Pointer to the DMS mesh
 * @param skeleton Pointer to the DMS skeleton
 */
void UpdateDMSMeshAnimation(DMSMesh* mesh, const DMSSkeleton* skeleton);

/**
 * Render a DMS model
 * @param dmsModel Pointer to the DMS model
 * @param position Position of the model
 * @param scale Scale of the model
 * @param tint Color tint to apply
 */
void RenderDMSModel(DMSModel* dmsModel, Vector3 position, float scale, Color tint);

/**
 * Free resources used by a DMS model
 * @param model Pointer to the DMS model to unload
 */
void UnloadDMSModel(DMSModel* model);

/**
 * Set current animation for a DMS model
 * @param model Pointer to the DMS model
 * @param animIndex Index of the animation to play
 * @return 1 if animation was set successfully, 0 otherwise
 */
int SetDMSModelAnimation(DMSModel* model, int animIndex);

/**
 * Get the number of animations in a DMS model
 * @param model Pointer to the DMS model
 * @return Number of animations, or 0 if model has no skeleton/animations
 */
int GetDMSModelAnimationCount(DMSModel* model);

/**
 * Get the name of a specific animation
 * @param model Pointer to the DMS model
 * @param animIndex Index of the animation
 * @return Name of the animation, or NULL if animation not found
 */
const char* GetDMSModelAnimationName(DMSModel* model, int animIndex);

/**
 * Get the current animation playing on a DMS model
 * @param model Pointer to the DMS model
 * @return Index of the current animation, or -1 if no animation playing
 */
int GetDMSModelCurrentAnimation(DMSModel* model);

#endif // DMS_H