#include <kos.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glkos.h>
#include <stdio.h>
#include "dms.h"

int main(int argc, char **argv) {
    maple_device_t *cont;
    cont_state_t *state;
    float rotation = 0.0f;
    float dr = 1.0f;  // Rotation speed
    int currentAnim = 0;
    
    // FPS tracking variables
    uint32 last_time = 0;
    uint32 current_time = 0;
    uint32 fps_counter = 0;
    uint32 fps_display_timer = 0;
    float fps = 0.0f;
    
    // Animation cycle debounce variables
    uint32 last_anim_change_time = 0;
    uint32 anim_change_cooldown = 500;  

    glKosInit();
    
    // Set up projection matrix
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, 640.0f / 480.0f, 0.1f, 100.0f);
    glMatrixMode(GL_MODELVIEW);
    
    // Enable texturing and depth testing
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    
    // Load model
    DMSModel* model = LoadDMSModel("/rd/dragon.dms");
    if (!model) {
        printf("Failed to load model\n");
        return 1;
    }
    
    // Load textures
    LoadDMSTextures(model, "/rd", "/rd/texture0.tex");
    
    // Initialize animation if available
    if (GetDMSModelAnimationCount(model) > 0) {
        SetDMSModelAnimation(model, currentAnim);
        printf("Model has %d animations\n", GetDMSModelAnimationCount(model));
    }
    
    // Clear color
    glClearColor(0.3f, 0.4f, 0.5f, 1.0f);
    
    // Initialize timing
    last_time = timer_ms_gettime64();
    fps_display_timer = last_time;
    
    // Main loop
    while(1) {
        current_time = timer_ms_gettime64();
        uint32 frame_time = current_time - last_time;
        last_time = current_time;
        
        float deltaTime = frame_time / 1000.0f;
        fps_counter++;
        
        if (current_time - fps_display_timer >= 1000) {
            fps = fps_counter * 1000.0f / (float)(current_time - fps_display_timer);
            printf("FPS: %.2f, Frame Time: %.2f ms\n", fps, 1000.0f / fps);
            fps_counter = 0;
            fps_display_timer = current_time;
        }
        
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Check controller input
        cont = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        if(cont) {
            state = (cont_state_t *)maple_dev_status(cont);
            if(!state) {
                printf("Error reading controller\n");
            } else {
                // Exit if START is pressed
                if(state->buttons & CONT_START)
                    break;
                
                // Rotation controls
                if(state->buttons & CONT_DPAD_LEFT) {
                    dr = 0.0f;   
                    rotation -= 2.0f;
                }
                if(state->buttons & CONT_DPAD_RIGHT) {
                    dr = 0.0f;
                    rotation += 2.0f;
                }
                
                // Animation controls with debounce
                if((state->buttons & CONT_A) && 
                   (current_time - last_anim_change_time >= anim_change_cooldown)) {
                    // Cycle through animations
                    int animCount = GetDMSModelAnimationCount(model);
                    if(animCount > 0) {
                        currentAnim = (currentAnim + 1) % animCount;
                        SetDMSModelAnimation(model, currentAnim);
                        printf("Changed to animation: %s\n", 
                               GetDMSModelAnimationName(model, currentAnim));
                        
                        // Update last animation change time
                        last_anim_change_time = current_time;
                    }
                }
            }
        }
        
        // Update rotation
        rotation += dr;
        
        if (model->skeleton && model->skeleton->animCount > 0) {
            UpdateDMSModelAnimation(model, deltaTime);
            
            // Update all meshes with the new animation state
            for (int i = 0; i < model->meshCount; i++) {
                UpdateDMSMeshAnimation(&model->meshes[i], model->skeleton);
            }
        }
        
        // Draw the model
        glLoadIdentity();
        glTranslatef(0.0f, 0.0f, -2.0f);  // Fixed camera distance
        glRotatef(rotation, 0.0f, 1.0f, 0.0f);
        
        // Set color to white
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        
        // Render model
        RenderDMSModel(model, (Vector3){0.0f, 0.0f, 0.0f}, 1.0f, WHITE);

        // Finish the frame
        glKosSwapBuffers();
    }
    
    // Final FPS report
    printf("Final stats - Average FPS: %.2f\n", fps);
    
    UnloadDMSModel(model);
    
    return 0;
}