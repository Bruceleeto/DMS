#include <kos.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glkos.h>
#include <stdio.h>
#include "dms.h"


 #define NUM_BALLS 10

 typedef struct {
    float x, y, z;         // Position
    float rx, ry, rz;      // Rotation angles
    float drx, dry, drz;   // Rotation speeds
} BallInfo;

int main(int argc, char **argv) {
    maple_device_t *cont;
    cont_state_t *state;
    float z = -15.0f;
    BallInfo balls[NUM_BALLS];
    
    uint32 last_time = 0;
    uint32 current_time = 0;
    uint32 fps_counter = 0;
    uint32 fps_display_timer = 0;
    float fps = 0.0f;
    uint32 total_polys = 0;   
    float pps = 0.0f;

    // Initialize balls in a grid pattern
    int idx = 0;
    for (int y = 0; y < 5; y++) {
        for (int x = 0; x < 4; x++) {
            if (idx < NUM_BALLS) {
                // Position
                balls[idx].x = (x * 4.0f) - 6.0f;
                balls[idx].y = (y * 4.0f) - 8.0f;
                balls[idx].z = 0.0f;
                
                // Rotation and speeds
                balls[idx].rx = 0.0f;
                balls[idx].ry = 0.0f;
                balls[idx].rz = 0.0f;
                balls[idx].drx = 0.5f + (idx & 3) * 0.2f;  
                balls[idx].dry = 0.7f + (idx & 1) * 0.2f;  
                balls[idx].drz = 0.0f;
                
                idx++;
            }
        }
    }

    glKosInit();
    
    // Set up projection matrix
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, 640.0f / 480.0f, 0.1f, 100.0f);
    glMatrixMode(GL_MODELVIEW);
    
    // Enable texturing and depth testing
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    
    // Load model and calculate triangle count
    DMSModel* model = LoadDMSModel("/rd/ball.dms");
    if (!model) {
        printf("Failed to load model\n");
        return 1;
    }
    
    // Count actual triangles in the model
    for (int m = 0; m < model->meshCount; m++) {
        DMSMesh* mesh = &model->meshes[m];
        int i = 0;
        
        // Count triangles in strips
        while (i < mesh->indexCount) {
            uint32_t rawIndex = mesh->indices[i];
            
            if ((rawIndex & 0x80000000) != 0) {
                // Triangle strip
                uint32_t stripId = (rawIndex >> 24) & 0x7F;
                int stripLength = 0;
                
                while (i < mesh->indexCount) {
                    rawIndex = mesh->indices[i];
                    if (!(rawIndex & 0x80000000) || ((rawIndex >> 24) & 0x7F) != stripId)
                        break;
                    stripLength++;
                    i++;
                }
                
                if (stripLength >= 3) {
                    // A strip of length N has N-2 triangles
                    mesh->triangleCount += stripLength - 2;
                }
            } else {
                // Individual triangle
                if (i + 2 < mesh->indexCount) {
                    mesh->triangleCount++;
                    i += 3;
                } else {
                    i++;
                }
            }
        }
        
        // Add to total
        total_polys += mesh->triangleCount;
    }
    
    // Multiply by number of instances
    total_polys *= NUM_BALLS;
    
    printf("Model loaded: %lu triangles per model, %lu total triangles\n", 
        total_polys / NUM_BALLS, total_polys);
    
    // Load textures
    LoadDMSTextures(model, "/rd", "/rd/ball0.tex");
    
    // Clear color
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    
    // Initialize timing
    last_time = timer_ms_gettime64();
    fps_display_timer = last_time;
    
    // Main loop
    while(1) {
        current_time = timer_ms_gettime64();
        last_time = current_time;
        
        fps_counter++;
        
        if (current_time - fps_display_timer >= 1000) {
            fps = fps_counter * 1000.0f / (float)(current_time - fps_display_timer);
            pps = fps * total_polys;
            
            printf("FPS: %.2f, Triangles: %lu, PPS: %.0f\n", 
                fps, total_polys, pps);
                   
            fps_counter = 0;
            fps_display_timer = current_time;
        }
        
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        cont = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        if(cont) {
            state = (cont_state_t *)maple_dev_status(cont);
            if(state && (state->buttons & CONT_START)) {
                break;   
            }
        }
        
        // Update and draw each ball
        for (int i = 0; i < NUM_BALLS; i++) {
            // Update rotation
            balls[i].rx += balls[i].drx;
            balls[i].ry += balls[i].dry;
            
            // Keep rotations within 0-360 range  
            if (balls[i].rx >= 360.0f) balls[i].rx -= 360.0f;
            if (balls[i].ry >= 360.0f) balls[i].ry -= 360.0f;
            
            // Draw the model
            glLoadIdentity();
            glTranslatef(0.0f, 0.0f, z);
            glTranslatef(balls[i].x, balls[i].y, balls[i].z);
            glRotatef(balls[i].rx, 1.0f, 0.0f, 0.0f);
            glRotatef(balls[i].ry, 0.0f, 1.0f, 0.0f);
            
            // Render model
            RenderDMSModel(model, (Vector3){0.0f, 0.0f, 0.0f}, 1.0f, WHITE);
        }

        // Finish the frame
        glKosSwapBuffers();
    }
    
    // Final stats report
    printf("Final stats - Average FPS: %.2f, PPS: %.0f\n", fps, pps);
    
    UnloadDMSModel(model);
    
    return 0;
}