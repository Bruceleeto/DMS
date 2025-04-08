#include <kos.h>
#include <raylib.h>
#include <stdio.h>
#include "dms.h"

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

// Global variables
bool running = true;

// Model variables
Vector3 modelPosition = { 0.0f, 0.0f, 0.0f };

// Animation variables
int currentAnimIndex = 0;
float animationTimer = 0.0f;

void updateController(DMSModel* model) {
    // Exit game if START button is pressed
    if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT)) running = false;
    
    // Cycle animations with A button
    if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) {
        currentAnimIndex = (currentAnimIndex + 1) % GetDMSModelAnimationCount(model);
        SetDMSModelAnimation(model, currentAnimIndex);
    }
}

int main(int argc, char **argv) {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Animation Viewer");
    
    // Load fox model
    DMSModel* foxModel = LoadDMSModel("/rd/dragon.dms");
    
    if (!foxModel) {
        printf("Failed to load fox model\n");
        CloseWindow();
        return 1;
    }
    
    // Load texture
    LoadDMSTextures(foxModel, "/rd", "/rd/texture.tex");
    
    // Set up camera
    Camera3D camera = { 0 };
    camera.position = (Vector3){ 0.0f, 1.0f, -3.0f };
    camera.target = (Vector3){ 0.0f, 0.5f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    
    SetTargetFPS(60);
    Color backgroundColor = BLUE;
    
    // Initialize animation
    if (GetDMSModelAnimationCount(foxModel) > 0) {
        SetDMSModelAnimation(foxModel, currentAnimIndex);
    }
    
    // Main game loop
    while (running) {
        updateController(foxModel);
        float deltaTime = GetFrameTime();
        
        // Update animation
        UpdateDMSModelAnimation(foxModel, deltaTime);
        for (int i = 0; i < foxModel->meshCount; i++) {
            UpdateDMSMeshAnimation(&foxModel->meshes[i], foxModel->skeleton);
        }
        
        float rotationSpeed = 30.0f;  
        modelPosition.y += rotationSpeed * deltaTime;
        
        // Draw
        BeginDrawing();
        ClearBackground(backgroundColor);
        BeginMode3D(camera);
        
        // Draw with rotation
        glPushMatrix();
        glTranslatef(0.0f, 0.0f, 0.0f);
        glRotatef(modelPosition.y, 0.0f, 1.0f, 0.0f);
       // glScalef(1.0f, 1.0f, 1.0f);
        RenderDMSModel(foxModel, (Vector3){0.0f, 0.0f, 0.0f}, 1.0f, WHITE);
        glPopMatrix();
        
        EndMode3D();
        
        // UI elements
        DrawRectangle(0, 0, SCREEN_WIDTH, 40, Fade(RAYWHITE, 0.6f));
        DrawText("Animation Viewer", 10, 10, 20, DARKGRAY);
        
        if (GetDMSModelAnimationCount(foxModel) > 0) {
            const char* animName = GetDMSModelAnimationName(foxModel, currentAnimIndex);
            if (animName) {
                DrawText(TextFormat("Animation: %s (%d/%d)", 
                    animName, 
                    currentAnimIndex + 1, 
                    GetDMSModelAnimationCount(foxModel)), 
                    180, 10, 20, DARKGRAY);
            }
        }
        
        DrawText("Press A to cycle animations", 10, SCREEN_HEIGHT - 30, 20, WHITE);
        DrawFPS(SCREEN_WIDTH - 70, 10);
        
        EndDrawing();
    }
    
    // Cleanup
    for (int i = 0; i < foxModel->textureCount; i++) {
        if (foxModel->textures[i].id != 0) UnloadTexture(foxModel->textures[i]);
    }
    
    UnloadDMSModel(foxModel);
    CloseWindow();
    
    return 0;
}