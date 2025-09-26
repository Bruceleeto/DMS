#include <kos.h>
#include <raylib.h>
#include <stdio.h>
#include <math.h>
#include "dms.h"

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

// Player animation indices
#define ANIM_IDLE 0
#define ANIM_ATTACK 1
#define ANIM_WALK 2

bool running = true;

// Player variables
Vector3 playerPos = { 0.0f, 1.0f, 0.0f };
float playerYaw = 0.0f;
float playerSpeed = 3.0f;
bool isMoving = false;
bool isAttacking = false;
float attackTimer = 0.0f;
int currentAnimIndex = ANIM_IDLE;

// Spider enemy variables
Vector3 spiderPos = { -2.4f, 1.0f, 30.0f };
float spiderSpeed = 2.0f;
float spiderYaw = -90.0f;  // Spider's rotation
bool spiderGoingForward = true;
bool spiderAnimSet = false;  

// Fixed camera variables
float cameraDistance = 4.0f;
float cameraHeight = 8.0f;

void updateController(float dt, DMSModel* playerModel) {
    if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT)) 
        running = false;
    
    isMoving = false;
    
    // Don't change direction if attacking
    if (!isAttacking) {
        // Movement with D-pad arrows
        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_UP)) {
            playerPos.z -= playerSpeed * dt;
            playerYaw = -90.0f;  
            isMoving = true;
        }
        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN)) {
            playerPos.z += playerSpeed * dt;
            playerYaw = 90.0f;  
            isMoving = true;
        }
        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT)) {
            playerPos.x -= playerSpeed * dt;
            playerYaw = 0.0f;  
            isMoving = true;
        }
        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) {
            playerPos.x += playerSpeed * dt;
            playerYaw = 180.0f;  
            isMoving = true;
        }
    }
    
    // Attack with A button
    if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) {
        if (!isAttacking) {
            isAttacking = true;
            SetDMSModelAnimation(playerModel, ANIM_ATTACK);
            currentAnimIndex = ANIM_ATTACK;
            
            // Get the actual duration of the attack animation
            if (playerModel->skeleton && playerModel->skeleton->animCount > ANIM_ATTACK) {
                attackTimer = playerModel->skeleton->animations[ANIM_ATTACK].duration;
            } else {
                attackTimer = 1.0f; // fallback
            }
        }
    }
    
    // Handle attack timer
    if (isAttacking) {
        attackTimer -= dt;
        if (attackTimer <= 0.0f) {
            isAttacking = false;
            // After attack ends, set appropriate animation
            if (isMoving) {
                SetDMSModelAnimation(playerModel, ANIM_WALK);
                currentAnimIndex = ANIM_WALK;
            } else {
                SetDMSModelAnimation(playerModel, ANIM_IDLE);
                currentAnimIndex = ANIM_IDLE;
            }
        }
    }
    
    // Update animation based on state (only if not attacking)
    if (!isAttacking) {
        if (isMoving && currentAnimIndex != ANIM_WALK) {
            SetDMSModelAnimation(playerModel, ANIM_WALK);
            currentAnimIndex = ANIM_WALK;
        } else if (!isMoving && currentAnimIndex != ANIM_IDLE) {
            SetDMSModelAnimation(playerModel, ANIM_IDLE);
            currentAnimIndex = ANIM_IDLE;
        }
    }
}

void updateSpider(float dt, DMSModel* spiderModel) {
    // Set animation to walk once at the beginning
    if (!spiderAnimSet) {
        printf("Setting spider to animation 4 (walk)\n");
        SetDMSModelAnimation(spiderModel, 4);  // Walk is index 4
        spiderAnimSet = true;
        
        if (spiderModel->skeleton) {
            printf("Spider currentAnim after setting: %d\n", spiderModel->skeleton->currentAnim);
        }
    }
    
    // Simple back and forth movement
    if (spiderGoingForward) {
        spiderPos.z -= spiderSpeed * dt;
        spiderYaw = 180.0f;  // Try 180 for forward
        if (spiderPos.z <= 14.0f) {
            spiderGoingForward = false;
        }
    } else {
        spiderPos.z += spiderSpeed * dt;
        spiderYaw = 0.0f;  // Try 0 for backward
        if (spiderPos.z >= 30.0f) {
            spiderGoingForward = true;
        }
    }
    
    UpdateDMSModelAnimation(spiderModel, dt);
    
    if (spiderModel->skeleton) {
        for (int m = 0; m < spiderModel->meshCount; m++) {
            UpdateDMSMeshAnimation(&spiderModel->meshes[m], spiderModel->skeleton);
        }
    }
}

int main(int argc, char **argv) {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "DMS Viewer");

    // Load level model
    DMSModel* levelModel = LoadDMSModel("/rd/level/level.dms");
    if (!levelModel) {
        printf("Failed to load level\n");
        CloseWindow();
        return 1;
    }
    LoadDMSTextures(levelModel, "/rd/level/textures", "/rd/level/textures/texture0.tex");

    // Load player model
    DMSModel* playerModel = LoadDMSModel("/rd/knight.dms");
    if (!playerModel) {
        printf("Failed to load player model\n");
        UnloadDMSModel(levelModel);
        CloseWindow();
        return 1;
    }
    LoadDMSTextures(playerModel, "/rd", "/rd/texture0.tex");

    // Load spider model
    DMSModel* spiderModel = LoadDMSModel("/rd/spider/spider.dms");
    if (!spiderModel) {
        printf("Failed to load spider model\n");
        UnloadDMSModel(levelModel);
        UnloadDMSModel(playerModel);
        CloseWindow();
        return 1;
    }
    LoadDMSTextures(spiderModel, "/rd/spider", "/rd/spider/texture0.tex");

    // Check spider animations
    int spiderAnimCount = GetDMSModelAnimationCount(spiderModel);
    printf("Spider model has %d animations\n", spiderAnimCount);
    
     
    SetDMSModelAnimation(playerModel, ANIM_IDLE);

    Camera3D camera = { 0 };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 60.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    SetTargetFPS(60);

    int frameCounter = 0;

    while (running) {
        float dt = GetFrameTime();
        frameCounter++;
        
        updateController(dt, playerModel);
        updateSpider(dt, spiderModel);

        // Update player animations
        UpdateDMSModelAnimation(playerModel, dt);
        
        // Update animated vertex positions for player
        if (playerModel->skeleton) {
            for (int m = 0; m < playerModel->meshCount; m++) {
                UpdateDMSMeshAnimation(&playerModel->meshes[m], playerModel->skeleton);
            }
        }
        
         if (frameCounter % 60 == 0 && spiderModel->skeleton) {
            printf("Frame %d: Spider anim = %d\n", frameCounter, spiderModel->skeleton->currentAnim);
        }

        camera.position = (Vector3){
            playerPos.x + cameraDistance,
            playerPos.y + cameraHeight,
            playerPos.z + cameraDistance
        };
        
        camera.target = (Vector3){
            playerPos.x,
            playerPos.y + 1.0f,
            playerPos.z
        };

        BeginDrawing();
        ClearBackground(SKYBLUE);
        BeginMode3D(camera);

        RenderDMSModel(levelModel, (Vector3){0, 0, 0}, 1.0f, WHITE);
        
        glPushMatrix();
        glTranslatef(playerPos.x, playerPos.y, playerPos.z);
        
        if (isAttacking) {
            glRotatef(playerYaw + -90.0f, 0, 1, 0);
        } else {
            glRotatef(playerYaw, 0, 1, 0);
        }
        
        glScalef(0.15f, 0.15f, 0.15f);
        glRotatef(90, 1, 0, 0);
        RenderDMSModel(playerModel, (Vector3){0, 0, 0}, 1.0f, WHITE);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(spiderPos.x, spiderPos.y, spiderPos.z);
        glRotatef(spiderYaw, 0, 1, 0);  
        glScalef(0.8f, 0.8f, 0.8f);
        RenderDMSModel(spiderModel, (Vector3){0, 0, 0}, 1.0f, WHITE);
        glPopMatrix();

        EndMode3D();

        DrawFPS(10, 10);
        DrawText("Arrow Keys: Move | A: Attack", 10, 30, 15, WHITE);
        EndDrawing();
    }

    for (int i = 0; i < levelModel->textureCount; i++) {
        if (levelModel->textures[i].id != 0) UnloadTexture(levelModel->textures[i]);
    }
    for (int i = 0; i < playerModel->textureCount; i++) {
        if (playerModel->textures[i].id != 0) UnloadTexture(playerModel->textures[i]);
    }
    for (int i = 0; i < spiderModel->textureCount; i++) {
        if (spiderModel->textures[i].id != 0) UnloadTexture(spiderModel->textures[i]);
    }

    UnloadDMSModel(levelModel);
    UnloadDMSModel(playerModel);
    UnloadDMSModel(spiderModel);
    CloseWindow();

    return 0;
}