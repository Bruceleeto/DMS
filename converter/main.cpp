#include "main.h"
#define CGLTF_IMPLEMENTATION
#include "include/cgltf.h"

// Transform data
typedef struct Transform {
    Vector3 translation;
    Quaternion rotation;
    Vector3 scale;
} Transform;

typedef struct {
    char name[64];
    int parent;
    Transform bindPose;
    Transform localPose;
    Matrix worldPose;        
    Matrix inverseBindMatrix;
} Bone;

typedef struct {
    char name[32];
    int boneCount;
    int frameCount;
    float duration;
    Transform** framePoses;  
} Animation;


typedef struct {
    Bone* bones;
    int boneCount;
    Animation* animations;
    int animCount;
    int currentAnim;
    float currentTime;
} Skeleton;


typedef struct {
    float x, y, z;          // Position (12 bytes)
    int8_t nx, ny, nz;      // Packed normals (3 bytes)
    uint8_t pad;            // Padding (1 byte)
    float u, v;             // Texture coordinates (8 bytes)
    uint8_t boneId;         // Bone index (1 byte)
    uint8_t pad2[3];        // Padding (3 bytes)
    float boneWeight;       // Bone weight (4 bytes)
} Vertex;                   // Total: 32 bytes


typedef struct {
    Vertex* vertices;       // Dynamic vertex array
    Vertex* originalVertices; // Store the original vertex positions  
    Vertex* animatedVertices;// Working buffer for animations

    unsigned int* indices;  // Dynamic index array
    int vertexCount;
    int indexCount;
    int textureId;          // NEW: Texture ID reference

 } Mesh;

typedef struct {
    Mesh* meshes;         // Array of meshes
    int meshCount;
    Skeleton* skeleton;   // Pointer to skeleton data
} Model;

typedef struct {
    float x, y, z;          // Position
    float nx, ny, nz;       // Normal
    float u, v;             // Texture coordinates
} StaticVertex;

struct Vertex_Tristripped {
    float position[3];
    float normal[3];
    float texcoord[2];
    uint32_t color;
    uint32_t vertexId;
    uint32_t normalId;
};

struct Triangle {
    Vertex_Tristripped vertices[3];
    uint32_t materialId;
};

std::vector<Triangle> triangles;
std::vector<std::vector<size_t>> join_strips(const triangle_stripper::primitive_vector& originalStrips);


bool LoadGLTF(const char* filename);
void Cleanup(void);
void optimize_mesh();
bool can_join_strips(const std::vector<size_t>& strip1, const std::vector<size_t>& strip2);
void ExportTristrippedModel(const Model* model, const char* filename);

#define POSITION_THRESHOLD 0.001f  // 0.1mm of movement
#define ROTATION_THRESHOLD 0.001f  // ~0.0006 degrees
#define SCALE_THRESHOLD    0.001f  // 0.001% scale change

// Globals
Skeleton skeleton = { 0 };
Model model = { 0 };
Model tristrippedModel = { 0 }; // Second model for tristripped version
 




// Find joint index in skeleton
static int GetNodeBoneIndex(const cgltf_node* node, const cgltf_skin* skin) {
    if (!node || !skin) return -1;
    
    for (size_t i = 0; i < skin->joints_count; i++) {
        if (skin->joints[i] == node) return (int)i;
    }
    return -1;
}

float QuaternionDotProduct(Quaternion a, Quaternion b) {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}


// Get transform from node
static Transform GetNodeTransform(const cgltf_node* node)
{
    Transform transform = {
        .translation = { 0 },
        .rotation    = QuaternionIdentity(),
        .scale       = { 1.0f, 1.0f, 1.0f }
    };

    if (node->has_matrix)
    {
        // Lay out glTF's row-major matrix (node->matrix[16]) into
        // a raylib Matrix (column-major), the same way cgltf_node_transform_world() does.
        // NO extra transpose step!
        Matrix mat = {
            node->matrix[0],  node->matrix[4],  node->matrix[8],  node->matrix[12],
            node->matrix[1],  node->matrix[5],  node->matrix[9],  node->matrix[13],
            node->matrix[2],  node->matrix[6],  node->matrix[10], node->matrix[14],
            node->matrix[3],  node->matrix[7],  node->matrix[11], node->matrix[15]
        };

        // Extract T, R, S from that matrix directly
        Vector3 t, s;
        Quaternion r;
        MatrixDecompose(mat, &t, &r, &s);

        transform.translation = t;
        transform.rotation    = r;
        transform.scale       = s;
    }
    else
    {
        // If no matrix, fall back to node->translation / node->rotation / node->scale
        if (node->has_translation)
        {
            transform.translation = (Vector3){
                node->translation[0],
                node->translation[1],
                node->translation[2]
            };
        }
        if (node->has_rotation)
        {
            transform.rotation = (Quaternion){
                node->rotation[0],
                node->rotation[1],
                node->rotation[2],
                node->rotation[3]
            };
        }
        if (node->has_scale)
        {
            transform.scale = (Vector3){
                node->scale[0],
                node->scale[1],
                node->scale[2]
            };
        }
    }

    return transform;
}

struct StripInfo {
    std::vector<uint32_t> indices;  // Indices making up this strip
    uint32_t stripId;               // ID of this strip
};

struct MeshTriStrips {
    std::vector<Vertex> vertices;           // Optimized vertex buffer
    std::vector<StripInfo> strips;          // List of strips
    std::vector<uint32_t> looseTriangles;   // Non-stripped triangles
    std::map<uint32_t, uint32_t> vertexMap; // Original to optimized vertex mapping
    int textureId;                          //   Texture ID for this mesh

};




bool isKeyframeNeeded(const Transform& current, const Transform& last) {
    // Check translation change
    Vector3 posDelta = Vector3Subtract(current.translation, last.translation);
    if (Vector3Length(posDelta) > POSITION_THRESHOLD) {
        return true;
    }

    // Check rotation change using dot product
    float rotDelta = 1.0f - fabsf(
        QuaternionDotProduct(current.rotation, last.rotation)
    );
    if (rotDelta > ROTATION_THRESHOLD) {
        return true;
    }

    // Check scale change
    Vector3 scaleDelta = Vector3Subtract(current.scale, last.scale);
    if (Vector3Length(scaleDelta) > SCALE_THRESHOLD) {
        return true;
    }

    return false;
}

// Returns how many keyframes were actually needed
int reduceKeyframes(Transform* poses, int frameCount, int boneCount) {
    std::vector<Transform> optimizedPoses;
    optimizedPoses.reserve(frameCount * boneCount);
    
    // Always keep first frame
    for(int i = 0; i < boneCount; i++) {
        optimizedPoses.push_back(poses[i]);
    }

    // Check each subsequent frame
    for(int frame = 1; frame < frameCount; frame++) {
        bool frameNeeded = false;
        int poseOffset = frame * boneCount;
        
        // Check if any bone has significant change
        for(int bone = 0; bone < boneCount; bone++) {
            Transform& current = poses[poseOffset + bone];
            Transform& last = optimizedPoses[optimizedPoses.size() - boneCount + bone];
            
            if(isKeyframeNeeded(current, last)) {
                frameNeeded = true;
                break;
            }
        }
        
        // If frame needed, copy all bone transforms
        if(frameNeeded) {
            for(int bone = 0; bone < boneCount; bone++) {
                optimizedPoses.push_back(poses[poseOffset + bone]);
            }
        }
    }
    
    // Always keep last frame if it wasn't already kept
    int lastFrame = (frameCount - 1) * boneCount;
    if(optimizedPoses.size() < (lastFrame + boneCount)) {
        for(int bone = 0; bone < boneCount; bone++) {
            optimizedPoses.push_back(poses[lastFrame + bone]);
        }
    }

    // Copy back to original array
    memcpy(poses, optimizedPoses.data(), 
           optimizedPoses.size() * sizeof(Transform));
           
    return optimizedPoses.size() / boneCount;
}






// Function to extract strip information from a mesh
MeshTriStrips ExtractTriStrips(const Mesh* srcMesh) {
    MeshTriStrips result;
    result.textureId = srcMesh->textureId;  //  Copy texture ID

    std::map<std::string, uint32_t> tempVertexMap;
    
    // Convert to triangles format
    triangles.clear();
    
    if (srcMesh->indexCount > 0 && srcMesh->indices) {
        // Indexed mesh - process triangles using indices
        for (int i = 0; i < srcMesh->indexCount; i += 3) {
            Triangle tri;
            tri.materialId = 0;
            
            for (int j = 0; j < 3; j++) {
                int idx = srcMesh->indices[i + j];
                const Vertex& v = srcMesh->vertices[idx];
                
                // Create vertex key - Updated for single bone
                char key[256];  
                snprintf(key, sizeof(key), 
                "%.6f,%.6f,%.6f|%.6f,%.6f|%d,%d,%d|%u,%f",
                v.x, v.y, v.z, v.u, v.v, (int)v.nx, (int)v.ny, (int)v.nz,
                (unsigned int)v.boneId, v.boneWeight);


                
                // Look up or add vertex
                auto it = tempVertexMap.find(key);
                uint32_t vertex_idx;
                if (it == tempVertexMap.end()) {
                    vertex_idx = result.vertices.size();
                    tempVertexMap[key] = vertex_idx;
                    result.vertices.push_back(v);
                    result.vertexMap[idx] = vertex_idx;
                } else {
                    vertex_idx = it->second;
                }
                
                // Setup triangle data
                Vertex_Tristripped tv;
                memcpy(tv.position, &v.x, sizeof(float) * 3);
                memcpy(tv.normal, &v.nx, sizeof(float) * 3);
                tv.texcoord[0] = v.u;
                tv.texcoord[1] = v.v;
                tv.color = 0xFFFFFFFF;
                tv.vertexId = vertex_idx;
                tv.normalId = vertex_idx;
                
                tri.vertices[j] = tv;
            }
            triangles.push_back(tri);
        }
    } else if (srcMesh->vertexCount > 0) {
        // Non-indexed mesh - process vertices directly as triangles
        for (int i = 0; i < srcMesh->vertexCount; i += 3) {
            Triangle tri;
            tri.materialId = 0;
            
            for (int j = 0; j < 3; j++) {
                const Vertex& v = srcMesh->vertices[i + j];
                
                // Create vertex key - Updated for single bone
                char key[256];  // Reduced size since we need less space now
                snprintf(key, sizeof(key), 
                "%.6f,%.6f,%.6f|%.6f,%.6f|%.6f,%.6f,%.6f|%u,%f",
                v.x, v.y, v.z, v.u, v.v, 
                (float)v.nx / 127.0f, (float)v.ny / 127.0f, (float)v.nz / 127.0f,
                (unsigned int)v.boneId, v.boneWeight);


                
                // Look up or add vertex
                auto it = tempVertexMap.find(key);
                uint32_t vertex_idx;
                if (it == tempVertexMap.end()) {
                    vertex_idx = result.vertices.size();
                    tempVertexMap[key] = vertex_idx;
                    result.vertices.push_back(v);
                    result.vertexMap[i + j] = vertex_idx;
                } else {
                    vertex_idx = it->second;
                }
                
                // Setup triangle data
                Vertex_Tristripped tv;
                memcpy(tv.position, &v.x, sizeof(float) * 3);
                memcpy(tv.normal, &v.nx, sizeof(float) * 3);
                tv.texcoord[0] = v.u;
                tv.texcoord[1] = v.v;
                tv.color = 0xFFFFFFFF;
                tv.vertexId = vertex_idx;
                tv.normalId = vertex_idx;
                
                tri.vertices[j] = tv;
            }
            triangles.push_back(tri);
        }
    }
    
    if (!triangles.empty()) {
        optimize_mesh();
        
        std::map<uint32_t, std::vector<Triangle>> stripMap;
        for (const auto& tri : triangles) {
            if (tri.materialId & 0x80000000) {
                uint32_t stripId = tri.materialId & 0x7FFFFFFF;
                stripMap[stripId].push_back(tri);
            } else {
                for (int j = 0; j < 3; j++) {
                    result.looseTriangles.push_back(tri.vertices[j].vertexId);
                }
            }
        }
        
        for (const auto& [stripId, stripTris] : stripMap) {
            StripInfo strip;
            strip.stripId = stripId;
            
            if (!stripTris.empty()) {
                for (int j = 0; j < 3; j++) {
                    strip.indices.push_back(stripTris[0].vertices[j].vertexId);
                }
                
                for (size_t i = 1; i < stripTris.size(); i++) {
                    if (i & 1) {
                        strip.indices.push_back(stripTris[i].vertices[1].vertexId);
                    } else {
                        strip.indices.push_back(stripTris[i].vertices[2].vertexId);
                    }
                }
            }
            
            result.strips.push_back(strip);
        }
    }
    
    return result;
}
bool LoadGLTF(const char* filename) {
    cgltf_options options = {};
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse_file(&options, filename, &data);

    if (result != cgltf_result_success) {
        printf("Failed to parse GLTF file: %s\n", filename);
        return false;
    }

    result = cgltf_load_buffers(&options, data, filename);
    if (result != cgltf_result_success) {
        printf("Failed to load GLTF buffers\n");
        cgltf_free(data);
        return false;
    }

    // Load skeleton
    if (data->skins_count > 0) {
        cgltf_skin* skin = &data->skins[0];
        skeleton.boneCount = (int)skin->joints_count;
        skeleton.bones = (Bone*)calloc(skeleton.boneCount, sizeof(Bone));
        // Load bone data
        for (int i = 0; i < skeleton.boneCount; i++) {
            cgltf_node* node = skin->joints[i];
            Bone* bone = &skeleton.bones[i];

            // Set name
            if (node->name) {
                strncpy(bone->name, node->name, sizeof(bone->name) - 1);
            } else {
                snprintf(bone->name, sizeof(bone->name), "bone_%d", i);
            }

            // Get parent index
            bone->parent = -1;
            if (node->parent) {
                bone->parent = GetNodeBoneIndex(node->parent, skin);
            }

            // Get bind pose transform
            bone->bindPose = GetNodeTransform(node);
            bone->localPose = bone->bindPose;

            // Get inverse bind matrix if available
            bone->inverseBindMatrix = MatrixIdentity();
            if (skin->inverse_bind_matrices) {
                cgltf_accessor* ibm = skin->inverse_bind_matrices;
                float* matrices = (float*)((char*)ibm->buffer_view->buffer->data +
                    ibm->buffer_view->offset + ibm->offset);
            if (skin->inverse_bind_matrices) {
                // Rename the accessor variable:
                cgltf_accessor* ibmAccessor = skin->inverse_bind_matrices;
                float* matrices = (float*)((char*)ibmAccessor->buffer_view->buffer->data +
                    ibmAccessor->buffer_view->offset + ibmAccessor->offset);

                // Reorder the floats from row-major to column-major:
                float *m4 = &matrices[i * 16];
                Matrix tempIBM = {
                    m4[0],  m4[4],  m4[8],  m4[12],
                    m4[1],  m4[5],  m4[9],  m4[13],
                    m4[2],  m4[6],  m4[10], m4[14],
                    m4[3],  m4[7],  m4[11], m4[15]
                };

                bone->inverseBindMatrix = tempIBM;
            }
                        }
        }

        // Initialize world poses in correct hierarchy order
        for (int i = 0; i < skeleton.boneCount; i++) {
            Bone* bone = &skeleton.bones[i];

            // Convert bind pose to matrix (Local transform)
            Matrix translation = MatrixTranslate(
                bone->bindPose.translation.x,
                bone->bindPose.translation.y,
                bone->bindPose.translation.z
            );
            Matrix rotation = QuaternionToMatrix(bone->bindPose.rotation);
            Matrix scale = MatrixScale(
                bone->bindPose.scale.x,
                bone->bindPose.scale.y,
                bone->bindPose.scale.z
            );

            // Local transform = Scale * Rotation * Translation
            Matrix localTransform = MatrixMultiply(MatrixMultiply(scale, rotation), translation);

            // For root bones, local transform is world transform
            if (bone->parent < 0) {
                bone->worldPose = localTransform;
            }
            // For child bones, multiply with parent's world transform
            else {
                bone->worldPose = MatrixMultiply(localTransform, skeleton.bones[bone->parent].worldPose);
            }
        }


        // Load animations if available
        if (data->animations_count > 0) {
            skeleton.animCount = (int)data->animations_count;
            skeleton.animations = (Animation*)calloc(skeleton.animCount, sizeof(Animation));
            for (int i = 0; i < skeleton.animCount; i++) {
                cgltf_animation* srcAnim = &data->animations[i];
                Animation* dstAnim = &skeleton.animations[i];

                // Set animation name
                if (srcAnim->name) {
                    strncpy(dstAnim->name, srcAnim->name, sizeof(dstAnim->name) - 1);
                } else {
                    snprintf(dstAnim->name, sizeof(dstAnim->name), "anim_%d", i);
                }

                // Find animation duration
                dstAnim->duration = 0.0f;
                for (size_t c = 0; c < srcAnim->channels_count; c++) {
                    cgltf_accessor* input = srcAnim->channels[c].sampler->input;
                    float* times = (float*)((char*)input->buffer_view->buffer->data +
                        input->buffer_view->offset + input->offset);
                    float lastTime = times[input->count - 1];
                    if (lastTime > dstAnim->duration) {
                        dstAnim->duration = lastTime;
                    }
                }

                // Calculate frame count (30 FPS)
                dstAnim->frameCount = (int)(dstAnim->duration * 30.0f) + 1;  // Look at this later.
                dstAnim->boneCount = skeleton.boneCount;

                // Allocate frame poses (as array of Transforms)
                Transform* poses = (Transform*)calloc(dstAnim->frameCount * skeleton.boneCount, sizeof(Transform));
                // Initialize all poses to bind pose
                for (int frame = 0; frame < dstAnim->frameCount; frame++) {
                    for (int bone = 0; bone < skeleton.boneCount; bone++) {
                        poses[frame * skeleton.boneCount + bone] = skeleton.bones[bone].bindPose;
                    }
                }

                // Sample animation channels
                for (size_t channelId = 0; channelId < srcAnim->channels_count; channelId++) {
                    cgltf_animation_channel* channel = &srcAnim->channels[channelId];
                    int boneIndex = GetNodeBoneIndex(channel->target_node, skin);
                    if (boneIndex < 0) continue;

                    cgltf_animation_sampler* sampler = channel->sampler;
                    float* times = (float*)((char*)sampler->input->buffer_view->buffer->data +
                        sampler->input->buffer_view->offset + sampler->input->offset);
                    float* values = (float*)((char*)sampler->output->buffer_view->buffer->data +
                        sampler->output->buffer_view->offset + sampler->output->offset);

                    // Sample this channel at each frame time
                    for (int frame = 0; frame < dstAnim->frameCount; frame++) {
                        float time = frame * (1.0f / 30.0f);

                        // Find surrounding keyframes
                        int prevKey = 0;
                        int nextKey = 0;
                        for (size_t k = 0; k < sampler->input->count - 1; k++) {
                            if (times[k] <= time && times[k + 1] >= time) {
                                prevKey = (int)k;
                                nextKey = (int)k + 1;
                                break;
                            }
                        }

                        // Get interpolation factor
                        float alpha = (time - times[prevKey]) / (times[nextKey] - times[prevKey]);
                        if (alpha < 0.0f) alpha = 0.0f;
                        if (alpha > 1.0f) alpha = 1.0f;

                        Transform* pose = &poses[frame * skeleton.boneCount + boneIndex];

                        // Apply channel data based on path
                        switch (channel->target_path) {
                            case cgltf_animation_path_type_translation: {
                                Vector3 prev = { values[prevKey * 3], values[prevKey * 3 + 1], values[prevKey * 3 + 2] };
                                Vector3 next = { values[nextKey * 3], values[nextKey * 3 + 1], values[nextKey * 3 + 2] };
                                pose->translation = Vector3Lerp(prev, next, alpha);
                            } break;
                            case cgltf_animation_path_type_rotation: {
                                Quaternion prev = {
                                    values[prevKey * 4], values[prevKey * 4 + 1],
                                    values[prevKey * 4 + 2], values[prevKey * 4 + 3]
                                };
                                Quaternion next = {
                                    values[nextKey * 4], values[nextKey * 4 + 1],
                                    values[nextKey * 4 + 2], values[nextKey * 4 + 3]
                                };
                                pose->rotation = QuaternionSlerp(prev, next, alpha);
                            } break;
                            case cgltf_animation_path_type_scale: {
                                Vector3 prev = { values[prevKey * 3], values[prevKey * 3 + 1], values[prevKey * 3 + 2] };
                                Vector3 next = { values[nextKey * 3], values[nextKey * 3 + 1], values[nextKey * 3 + 2] };
                                pose->scale = Vector3Lerp(prev, next, alpha);
                            } break;
                            default: break;
                        }
                    }
                }

                int reducedFrames = reduceKeyframes(poses, dstAnim->frameCount, dstAnim->boneCount);
                printf("Animation '%s': Reduced from %d to %d frames\n", dstAnim->name, dstAnim->frameCount, reducedFrames);
                dstAnim->frameCount = reducedFrames;  // Update the frame count

                dstAnim->framePoses = (Transform**)poses;

 
            }
        }
    } else {
        // Initialize empty animation data
        skeleton.animCount = 0;
        skeleton.animations = NULL;
    }

    // Free existing mesh data if it exists
    if (model.meshes) {
        for (int i = 0; i < model.meshCount; i++) {
            if (model.meshes[i].vertices) free(model.meshes[i].vertices);
            if (model.meshes[i].indices) free(model.meshes[i].indices);
        }
        free(model.meshes);
    }

    // Reset mesh count and other model data
    model.meshCount = 0;
    model.meshes = NULL;

    // Associate skeleton with model
    model.skeleton = &skeleton;

    // Load meshes
       if (data->meshes_count > 0) {
        model.meshCount = (int)data->meshes_count;
        model.meshes = (Mesh*)calloc(model.meshCount, sizeof(Mesh));





        for (size_t m = 0; m < data->meshes_count; m++) {
            cgltf_mesh* srcMesh = &data->meshes[m];
            Mesh* dstMesh = &model.meshes[m];
            dstMesh->textureId = -1;

            // First, count total vertices and indices across all primitives
            int totalVertices = 0;
            int totalIndices = 0;
            for (size_t p = 0; p < srcMesh->primitives_count; p++) {
                cgltf_primitive* primitive = &srcMesh->primitives[p];
                
                // If this primitive has a material with a texture
                if (primitive->material && primitive->material->has_pbr_metallic_roughness) {
                    cgltf_pbr_metallic_roughness* pbr = &primitive->material->pbr_metallic_roughness;
                    
                    // If there's a base color texture, use its index as texture ID
                    if (pbr->base_color_texture.texture) {
                        dstMesh->textureId = pbr->base_color_texture.texture->image - data->images;
                        printf("Mesh %zu using texture ID: %d\n", m, dstMesh->textureId);
                    }
                }
                
                // Continue with existing vertex/index counting
                for (size_t a = 0; a < primitive->attributes_count; a++) {
                    if (primitive->attributes[a].type == cgltf_attribute_type_position) {
                        totalVertices += (int)primitive->attributes[a].data->count;
                        break;
                    }
                }
                if (primitive->indices) {
                    totalIndices += (int)primitive->indices->count;
                }
            }

            // Allocate combined buffers
             dstMesh->vertices = (Vertex*)calloc(totalVertices, sizeof(Vertex));
            dstMesh->originalVertices = (Vertex*)calloc(totalVertices, sizeof(Vertex));
            dstMesh->animatedVertices = (Vertex*)calloc(totalVertices, sizeof(Vertex));  
            dstMesh->indices = (unsigned int*)calloc(totalIndices, sizeof(unsigned int));
            
            // Keep track of current offsets as we combine primitives
            int vertexOffset = 0;
            int indexOffset = 0;

            // Load each primitive
            for (size_t p = 0; p < srcMesh->primitives_count; p++) {
                cgltf_primitive* primitive = &srcMesh->primitives[p];
                
                // Get vertex count for this primitive
                int primitiveVertexCount = 0;
                for (size_t a = 0; a < primitive->attributes_count; a++) {
                    if (primitive->attributes[a].type == cgltf_attribute_type_position) {
                        primitiveVertexCount = (int)primitive->attributes[a].data->count;
                        break;
                    }
                }

                // Load vertex attributes
                for (size_t a = 0; a < primitive->attributes_count; a++) {
                    cgltf_attribute* attr = &primitive->attributes[a];
                    cgltf_accessor* accessor = attr->data;

                    switch (attr->type) {
                        case cgltf_attribute_type_position: {
                            for (size_t v = 0; v < accessor->count; v++) {
                                cgltf_accessor_read_float(accessor, v, 
                                    &dstMesh->vertices[vertexOffset + v].x, 3);
                                // Copy to original vertices
                                dstMesh->originalVertices[vertexOffset + v].x = 
                                    dstMesh->vertices[vertexOffset + v].x;
                                dstMesh->originalVertices[vertexOffset + v].y = 
                                    dstMesh->vertices[vertexOffset + v].y;
                                dstMesh->originalVertices[vertexOffset + v].z = 
                                    dstMesh->vertices[vertexOffset + v].z;
                            }
                        } break;

                        case cgltf_attribute_type_normal: {
                            for (size_t v = 0; v < accessor->count; v++) {
                                // Create temporary buffer for float values
                                float temp[3];
                                cgltf_accessor_read_float(accessor, v, temp, 3);
                                
                                // Convert floats to bytes (normalized from -1.0...1.0 to -127...127)
                                dstMesh->vertices[vertexOffset + v].nx = (int8_t)(temp[0] * 127.0f);
                                dstMesh->vertices[vertexOffset + v].ny = (int8_t)(temp[1] * 127.0f);
                                dstMesh->vertices[vertexOffset + v].nz = (int8_t)(temp[2] * 127.0f);
                                
                                // Copy to original vertices
                                dstMesh->originalVertices[vertexOffset + v].nx = dstMesh->vertices[vertexOffset + v].nx;
                                dstMesh->originalVertices[vertexOffset + v].ny = dstMesh->vertices[vertexOffset + v].ny;
                                dstMesh->originalVertices[vertexOffset + v].nz = dstMesh->vertices[vertexOffset + v].nz;
                            }
                        } break;

                        case cgltf_attribute_type_texcoord: {
                            for (size_t v = 0; v < accessor->count; v++) {
                                cgltf_accessor_read_float(accessor, v, 
                                    &dstMesh->vertices[vertexOffset + v].u, 2);
                                dstMesh->originalVertices[vertexOffset + v].u = 
                                    dstMesh->vertices[vertexOffset + v].u;
                                dstMesh->originalVertices[vertexOffset + v].v = 
                                    dstMesh->vertices[vertexOffset + v].v;
                            }
                        } break;



                        case cgltf_attribute_type_joints: {
                            for (size_t v = 0; v < accessor->count; v++) {
                                cgltf_uint jointIds[4] = {0};
                                cgltf_accessor_read_uint(accessor, v, jointIds, 4);
                                
                                // Only store the first joint
                        if (jointIds[0] > 255) {
                            printf("Warning: Joint ID %u exceeds uint8_t range\n", jointIds[0]);
                            jointIds[0] = 255;
                        }
                        dstMesh->vertices[vertexOffset + v].boneId = (uint8_t)jointIds[0];
                        dstMesh->originalVertices[vertexOffset + v].boneId = (uint8_t)jointIds[0];

                            }
                        } break;


                        case cgltf_attribute_type_weights: {
                        for (size_t v = 0; v < accessor->count; v++) {
                            float weights[4] = {0};
                            cgltf_accessor_read_float(accessor, v, weights, 4);
                            
                            // Only store the first weight
                            dstMesh->vertices[vertexOffset + v].boneWeight = weights[0];
                            dstMesh->originalVertices[vertexOffset + v].boneWeight = weights[0];
                        }
                    } break;


                    }
                }

                // Load indices
                if (primitive->indices) {
                    for (size_t i = 0; i < primitive->indices->count; i++) {
                        dstMesh->indices[indexOffset + i] = 
                            (unsigned int)cgltf_accessor_read_index(primitive->indices, i) + vertexOffset;
                    }
                    indexOffset += (int)primitive->indices->count;
                }

                vertexOffset += primitiveVertexCount;
            }

            // Set final counts
            dstMesh->vertexCount = totalVertices;
            dstMesh->indexCount = totalIndices;

            printf("Loaded mesh %zu with %d vertices and %d indices\n", 
                  m, dstMesh->vertexCount, dstMesh->indexCount);
        }
    }

    cgltf_free(data);
    return true;
}

 

 

void CreateTristrippedModel(const Model* sourceModel, Model* destModel)
{
    printf("Creating tristripped model...\n");
    
    // Copy skeleton pointer and allocate new mesh array
    destModel->skeleton = sourceModel->skeleton;
    destModel->meshCount = sourceModel->meshCount;
    destModel->meshes = (Mesh*)calloc(destModel->meshCount, sizeof(Mesh));

    // Process each mesh from the source model
    for (int m = 0; m < sourceModel->meshCount; m++)
    {
        const Mesh* srcMesh = &sourceModel->meshes[m];
        Mesh* dstMesh = &destModel->meshes[m];
                // Copy texture ID from source mesh
                dstMesh->textureId = srcMesh->textureId;  // NEW: Copy texture ID


        printf("Processing mesh %d of %d...\n", m + 1, sourceModel->meshCount);
        printf("Source mesh has %d vertices and %d indices\n", srcMesh->vertexCount, srcMesh->indexCount);

        // Use the new ExtractTriStrips function to get optimized data
        MeshTriStrips tristrips = ExtractTriStrips(srcMesh);

        // Allocate destination mesh memory
        dstMesh->vertexCount = tristrips.vertices.size();
        dstMesh->vertices = (Vertex*)calloc(dstMesh->vertexCount, sizeof(Vertex));
        dstMesh->animatedVertices = (Vertex*)calloc(dstMesh->vertexCount, sizeof(Vertex));

        // Copy optimized vertices to bind pose buffer (already done)
        memcpy(dstMesh->vertices, tristrips.vertices.data(), 
            dstMesh->vertexCount * sizeof(Vertex));




        // Calculate total indices needed
        size_t totalIndices = 0;
        for (const auto& strip : tristrips.strips) {
            totalIndices += strip.indices.size();
        }
        totalIndices += tristrips.looseTriangles.size();

        // Allocate and fill index buffer
        dstMesh->indexCount = totalIndices;
        dstMesh->indices = (unsigned int*)calloc(dstMesh->indexCount, sizeof(unsigned int));

        // Copy indices - first the strips
        size_t indexOffset = 0;
        for (const auto& strip : tristrips.strips) {
            for (size_t i = 0; i < strip.indices.size(); i++) {
                // Mark strip indices with the high bit and strip ID
                dstMesh->indices[indexOffset++] = 
                    (0x80000000 | (strip.stripId << 24)) | strip.indices[i];
            }
        }

        //   loose triangles
        for (uint32_t idx : tristrips.looseTriangles) {
            dstMesh->indices[indexOffset++] = idx;
        }

        printf("Mesh %d processed:\n", m + 1);
        printf("  Original vertices:    %d\n", srcMesh->vertexCount);
        printf("  Optimized vertices:   %d\n", dstMesh->vertexCount);
        printf("  Total strips:         %zu\n", tristrips.strips.size());
        printf("  Loose triangles:      %zu\n", tristrips.looseTriangles.size() / 3);
        printf("  Total indices:        %d\n", dstMesh->indexCount);
    }

    printf("Tristripped model creation complete!\n");
}

 


 


int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <gltf_file>\n", argv[0]);
        return 1;
    }

    const char* inputFilename = argv[1];
    
    // Load the GLTF/GLB file
    if (!LoadGLTF(inputFilename)) {
        printf("Failed to load file: %s\n", inputFilename);
        return 1;
    }
    
    // Create the tristripped model
    CreateTristrippedModel(&model, &tristrippedModel);
    
    //   conversion stats
    printf("Original model: %d meshes\n", model.meshCount);
    if (model.meshCount > 0) {
        printf("First mesh: %d verts, %d indices\n", 
               model.meshes[0].vertexCount, model.meshes[0].indexCount);
    }
    
    printf("Tristripped model: %d meshes\n", tristrippedModel.meshCount);
    if (tristrippedModel.meshCount > 0) {
        printf("First mesh: %d verts, %d indices\n",
               tristrippedModel.meshes[0].vertexCount, 
               tristrippedModel.meshes[0].indexCount);
    }
    
    // Generate output filename (same name but with .dms extension)
    char outputFilename[256] = {0};
    const char* extension = strrchr(inputFilename, '.');
    if (extension) {
        // Copy everything before the extension
        size_t baseNameLength = extension - inputFilename;
        strncpy(outputFilename, inputFilename, baseNameLength);
        // Add new extension
        strcat(outputFilename, ".dms");
    } else {
        // No extension in input, just append .dms
        strcpy(outputFilename, inputFilename);
        strcat(outputFilename, ".dms");
    }
    
    // Export the file
    if (tristrippedModel.meshCount > 0) {
        printf("Exporting to: %s\n", outputFilename);
        ExportTristrippedModel(&tristrippedModel, outputFilename);
     } else {
        printf("No meshes to export\n");
    }
    
    // Clean up resources
    if (model.meshes) {
        for (int i = 0; i < model.meshCount; i++) {
            if (model.meshes[i].vertices) free(model.meshes[i].vertices);
            if (model.meshes[i].originalVertices) free(model.meshes[i].originalVertices);
            if (model.meshes[i].animatedVertices) free(model.meshes[i].animatedVertices);
            if (model.meshes[i].indices) free(model.meshes[i].indices);
        }
        free(model.meshes);
    }
    
    if (tristrippedModel.meshes) {
        for (int i = 0; i < tristrippedModel.meshCount; i++) {
            if (tristrippedModel.meshes[i].vertices) free(tristrippedModel.meshes[i].vertices);
            if (tristrippedModel.meshes[i].animatedVertices) free(tristrippedModel.meshes[i].animatedVertices);
            if (tristrippedModel.meshes[i].indices) free(tristrippedModel.meshes[i].indices);
        }
        free(tristrippedModel.meshes);
    }
    
    if (skeleton.bones) {
        free(skeleton.bones);
    }
    
    if (skeleton.animations) {
        for (int i = 0; i < skeleton.animCount; i++) {
            if (skeleton.animations[i].framePoses) {
                free(skeleton.animations[i].framePoses);
            }
        }
        free(skeleton.animations);
    }
    
    return 0;
}




void Cleanup(void) {
    if (skeleton.bones) {
        free(skeleton.bones);
    }
    
    if (skeleton.animations) {
        for (int i = 0; i < skeleton.animCount; i++) {
            if (skeleton.animations[i].framePoses) {
                free(skeleton.animations[i].framePoses);
            }
        }
        free(skeleton.animations);
    }
    
    if (model.meshes) {
        for (int i = 0; i < model.meshCount; i++) {
            if (model.meshes[i].vertices) free(model.meshes[i].vertices);
            if (model.meshes[i].originalVertices) free(model.meshes[i].originalVertices);
            if (model.meshes[i].animatedVertices) free(model.meshes[i].animatedVertices);
            if (model.meshes[i].indices) free(model.meshes[i].indices);
        }
        free(model.meshes);
    }
    
    if (tristrippedModel.meshes) {
        for (int i = 0; i < tristrippedModel.meshCount; i++) {
            if (tristrippedModel.meshes[i].vertices) free(tristrippedModel.meshes[i].vertices);
            if (tristrippedModel.meshes[i].animatedVertices) free(tristrippedModel.meshes[i].animatedVertices);
            if (tristrippedModel.meshes[i].indices) free(tristrippedModel.meshes[i].indices);
        }
        free(tristrippedModel.meshes);
    }
}


bool can_join_strips(const std::vector<size_t>& strip1, const std::vector<size_t>& strip2) {
    if (strip1.empty() || strip2.empty()) return false;
    return (strip1[strip1.size()-1] == strip2[1] && strip1[strip1.size()-2] == strip2[0]) ||
           (strip1[strip1.size()-1] == strip2[0] && strip1[strip1.size()-2] == strip2[1]);
}

std::vector<std::vector<size_t>> join_strips(const triangle_stripper::primitive_vector& originalStrips) {
    std::vector<std::vector<size_t>> result;
    std::vector<bool> used(originalStrips.size(), false);
    std::vector<std::vector<size_t>> strips;
    
     for (const auto& prim : originalStrips) {
        if (prim.Type == triangle_stripper::TRIANGLE_STRIP) {
            strips.push_back(std::vector<size_t>(prim.Indices.begin(), prim.Indices.end()));
        }
    }

    for (size_t i = 0; i < strips.size(); i++) {
        if (used[i]) continue;

        std::vector<size_t> current_strip = strips[i];
        used[i] = true;
        
        bool found_join;
        do {
            found_join = false;
            for (size_t j = 0; j < strips.size(); j++) {
                if (!used[j] && can_join_strips(current_strip, strips[j])) {
                    current_strip.insert(current_strip.end(), 
                                      strips[j].begin() + 2,
                                      strips[j].end());
                    used[j] = true;
                    found_join = true;
                    break;
                }
            }
        } while (found_join);

        result.push_back(current_strip);
    }
    return result;
}

void optimize_mesh() {
    if (triangles.empty()) {
        printf("Warning: No triangles to optimize. Optimization did not happen.\n");
        return;
    }


    using namespace triangle_stripper;
    
    printf("=== Strip Optimization ===\n");
    printf("Input triangles: %zu\n", triangles.size());

    // Create vertex index mapping
    std::map<std::string, uint32_t> vertex_map;
    std::vector<Vertex_Tristripped> unique_vertices;
    indices Indices;
    
    for(const auto& tri : triangles) {
        for(int i = 0; i < 3; i++) {
            char key[256];
            const auto& v = tri.vertices[i];
            snprintf(key, sizeof(key), "%.6f,%.6f,%.6f|%.6f,%.6f", 
                    v.position[0], v.position[1], v.position[2],
                    v.texcoord[0], v.texcoord[1]);
            
            auto it = vertex_map.find(key);
            if(it == vertex_map.end()) {
                it = vertex_map.insert({key, unique_vertices.size()}).first;
                unique_vertices.push_back(v);
            }
            Indices.push_back(it->second);
        }
    }

    // Generate strips
    primitive_vector primitives;
    tri_stripper stripper(Indices);
    stripper.SetMinStripSize(0);
    stripper.SetCacheSize(0);
    stripper.SetBackwardSearch(true);
    stripper.SetPushCacheHits(true);
    stripper.Strip(&primitives);

    // Join compatible strips
    auto joined_strips = join_strips(primitives);
    
    // Create optimized triangles
    std::vector<Triangle> optimized_tris;
    size_t strip_triangles = 0, list_triangles = 0;

    // Process non-strip triangles
    for(const auto& prim : primitives) {
        if(prim.Type == TRIANGLES) {
            for(size_t i = 0; i < prim.Indices.size(); i += 3) {
                Triangle tri;
                tri.materialId = 0;
                for(int j = 0; j < 3; j++) {
                    tri.vertices[j] = unique_vertices[prim.Indices[i + j]];
                }
                optimized_tris.push_back(tri);
                list_triangles++;
            }
        }
    }

    // Process strips
    for(size_t strip_idx = 0; strip_idx < joined_strips.size(); strip_idx++) {
        const auto& strip = joined_strips[strip_idx];
        for(size_t i = 0; i < strip.size() - 2; i++) {
            Triangle tri;
            tri.materialId = 0x80000000 | (strip_idx + 1);
            
            size_t indices[3] = {
                strip[i],
                strip[i + (i & 1 ? 2 : 1)],
                strip[i + (i & 1 ? 1 : 2)]
            };
            
            for(int j = 0; j < 3; j++) {
                if(indices[j] < unique_vertices.size()) {
                    tri.vertices[j] = unique_vertices[indices[j]];
                }
            }
            optimized_tris.push_back(tri);
            strip_triangles++;
        }
    }

     float strip_ratio = (float)strip_triangles / (float)(strip_triangles + list_triangles);
    
     size_t total_strip_vertices = 0;
    size_t max_strip_length = 0;
    for(const auto& strip : joined_strips) {
        total_strip_vertices += strip.size();
        max_strip_length = std::max(max_strip_length, strip.size());
    }
    
    float avg_strip_length = joined_strips.size() > 0 ? 
        (float)total_strip_vertices / (float)joined_strips.size() : 0;
    float vertex_reuse = joined_strips.size() > 0 ? 
        (float)strip_triangles * 3.0f / (float)total_strip_vertices : 0;

    printf("Optimization complete:\n");
    printf("- Strips: %zu\n", joined_strips.size());
    printf("- Strip triangles: %zu\n", strip_triangles);
    printf("- List triangles: %zu\n", list_triangles);
    printf("- Total triangles: %zu\n", optimized_tris.size());
    
    printf("\nEfficiency Metrics:\n");
    printf(" - Strip ratio: %.2f%%\n", strip_ratio * 100.0f);
    printf(" - Average strip length: %.2f vertices\n", avg_strip_length);
    printf(" - Longest strip: %zu vertices\n", max_strip_length);
    printf(" - Vertex reuse factor: %.2f\n", vertex_reuse);
    printf("=== End of Strip Optimization Statistics ===\n");

    triangles = optimized_tris;
}

void ExportTristrippedModel(const Model* model, const char* filename) {
    FILE* file = fopen(filename, "wb");
    if (!file) {
        printf("Failed to open file for writing: %s\n", filename);
        return;
    }

    //   header
    uint32_t magic = 0x54534D44;  // "DMST" in hex
    uint32_t version = 1;
    uint32_t meshCount = model->meshCount;
    uint32_t boneCount = model->skeleton ? model->skeleton->boneCount : 0;
    bool isAnimated = (boneCount > 0);
    
    fwrite(&magic, sizeof(uint32_t), 1, file);
    fwrite(&version, sizeof(uint32_t), 1, file);
    fwrite(&meshCount, sizeof(uint32_t), 1, file);
    fwrite(&boneCount, sizeof(uint32_t), 1, file);

    printf("Writing file header at %ld\n", ftell(file));

    // Write skeleton data if it exists
    if (isAnimated && model->skeleton) {
        printf("Writing %d bones at %ld\n", boneCount, ftell(file));
        
        for (uint32_t i = 0; i < boneCount; i++) {
            const Bone& bone = model->skeleton->bones[i];
            fwrite(bone.name, sizeof(char), 64, file);
            fwrite(&bone.parent, sizeof(int), 1, file);
            fwrite(&bone.bindPose, sizeof(Transform), 1, file);
            fwrite(&bone.inverseBindMatrix, sizeof(Matrix), 1, file);
        }

        // Write animation data
        uint32_t animCount = model->skeleton->animCount;
        fwrite(&animCount, sizeof(uint32_t), 1, file);
        
        printf("Writing %d animations at %ld\n", animCount, ftell(file));

        for (uint32_t i = 0; i < animCount; i++) {
            const Animation* anim = &model->skeleton->animations[i];
            
            // Write animation header
            fwrite(anim->name, sizeof(char), 32, file);
            fwrite(&anim->boneCount, sizeof(int), 1, file);
            fwrite(&anim->frameCount, sizeof(int), 1, file);
            fwrite(&anim->duration, sizeof(float), 1, file);

            printf("Writing animation %d: %s (%d frames) at %ld\n", 
                   i, anim->name, anim->frameCount, ftell(file));

            // Write frame poses
            size_t totalPoses = anim->frameCount * anim->boneCount;
            for (size_t j = 0; j < totalPoses; j++) {
                Transform* pose = &((Transform*)anim->framePoses)[j];
                fwrite(pose, sizeof(Transform), 1, file);
            }
        }
    } else {
        // Write zero animations if no skeleton
        uint32_t animCount = 0;
        fwrite(&animCount, sizeof(uint32_t), 1, file);
    }

    // Write mesh data
    printf("Writing %d meshes at %ld\n", meshCount, ftell(file));
    
    for (uint32_t m = 0; m < meshCount; m++) {
        const Mesh* mesh = &model->meshes[m];
        
        // Debug print
        printf("  Writing mesh %d: %d vertices, %d indices\n", 
               m, mesh->vertexCount, mesh->indexCount);
        
        // Write mesh header
        uint32_t vertCount = mesh->vertexCount;
        uint32_t idxCount = mesh->indexCount;
        int textureId = mesh->textureId;  //  Get texture ID

        fwrite(&vertCount, sizeof(uint32_t), 1, file);
        fwrite(&idxCount, sizeof(uint32_t), 1, file);
        fwrite(&textureId, sizeof(int), 1, file);  //  Write texture ID

        // Write vertex data based on whether it's animated or static
        if (!isAnimated) {
            // Static mesh - write simplified vertex data
            for (uint32_t i = 0; i < mesh->vertexCount; i++) {
                StaticVertex sv;
                sv.x = mesh->vertices[i].x;
                sv.y = mesh->vertices[i].y;
                sv.z = mesh->vertices[i].z;
                sv.nx = mesh->vertices[i].nx;
                sv.ny = mesh->vertices[i].ny;
                sv.nz = mesh->vertices[i].nz;
                sv.u = mesh->vertices[i].u;
                sv.v = mesh->vertices[i].v;
                fwrite(&sv, sizeof(StaticVertex), 1, file);
            }
        } else {
            // Animated mesh - write full vertex data
            fwrite(mesh->vertices, sizeof(Vertex), mesh->vertexCount, file);
        }
        
        // Write index data
        if (mesh->indexCount > 0) {
            fwrite(mesh->indices, sizeof(uint32_t), mesh->indexCount, file);
        }
    }

    long pos = ftell(file);
    fclose(file);
    printf("File writing complete at %ld bytes\n", pos);
}

 
 
