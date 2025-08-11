#include <kos.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glkos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dms.h"

#include <toy_common.h>
#include <toy_value.h>
#include <toy_vm.h>
#include <toy_lexer.h>
#include <toy_parser.h>
#include <toy_module_compiler.h>
#include <toy_bucket.h>
#include <toy_string.h>
#include <toy_scope.h>
#include <toy_print.h>

static void printCallback(const char* msg) {
    printf("%s\n", msg);
}

static char* loadFile(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) return NULL;
    
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    
    char* content = malloc(size + 1);
    fread(content, 1, size, file);
    content[size] = '\0';
    fclose(file);
    return content;
}

static int runToyScript(Toy_VM* vm, Toy_Bucket** bucket, const char* script, int preserveScope) {
    Toy_Lexer lexer;
    Toy_Parser parser;
    
    Toy_bindLexer(&lexer, script);
    Toy_bindParser(&parser, &lexer);
    Toy_configureParser(&parser, false);
    
    Toy_Ast* ast = Toy_scanParser(bucket, &parser);
    if (parser.error) return 0;
    
    unsigned char* buffer = Toy_compileModule(ast);
    Toy_Module module = Toy_parseModule(buffer);
    
    Toy_bindVM(vm, &module, preserveScope);
    Toy_runVM(vm);
    Toy_resetVM(vm, true);
    free(buffer);
    
    return 1;
}

static float getToyFloat(Toy_VM* vm, Toy_Bucket** bucket, const char* name) {
    Toy_String* key = Toy_createNameStringLength(bucket, name, strlen(name), TOY_VALUE_ANY, false);
    Toy_Value* val = Toy_accessScopeAsPointer(vm->scope, key);
    float result = 0.0f;
    if (val) {
        if (TOY_VALUE_IS_FLOAT(*val)) result = TOY_VALUE_AS_FLOAT(*val);
        else if (TOY_VALUE_IS_INTEGER(*val)) result = (float)TOY_VALUE_AS_INTEGER(*val);
    }
    Toy_freeString(key);
    return result;
}

int main(int argc, char **argv) {
    Toy_setPrintCallback(printCallback);
    Toy_Bucket* bucket = Toy_allocateBucket(TOY_BUCKET_IDEAL);
    Toy_VM vm;
    Toy_initVM(&vm);
    
    char* initScript = loadFile("/rd/game.toy");
    char* updateScript = loadFile("/rd/update.toy");
    
    if (!initScript || !updateScript) {
        printf("ERROR: game.toy and update.toy are required!\n");
        free(initScript);
        free(updateScript);
        return 1;
    }
    
      if (!runToyScript(&vm, &bucket, initScript, 0)) {
        printf("Failed to parse game.toy\n");
        free(initScript);
        free(updateScript);
        return 1;
    }
    free(initScript);
    
    glKosInit();
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, 640.0f / 480.0f, 0.1f, 100.0f);
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    
    DMSModel* model = LoadDMSModel("/rd/toy_box.dms");
    if (model) LoadDMSTextures(model, "/rd", "/rd/toylogo0.tex");
    
    maple_device_t *cont;
    cont_state_t *state;
    
    while(1) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        cont = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        if(cont) {
            state = (cont_state_t *)maple_dev_status(cont);
            if(state && (state->buttons & CONT_START)) break;
        }
        
        runToyScript(&vm, &bucket, updateScript, 1);
        
        float x = getToyFloat(&vm, &bucket, "x");
        float y = getToyFloat(&vm, &bucket, "y");
        float rot = getToyFloat(&vm, &bucket, "rot");
        
        glLoadIdentity();
        glTranslatef(0.0f, 0.0f, -15.0f);
        glTranslatef(x, y, 0.0f);
        glRotatef(rot, 1.0f, 1.0f, 0.0f);
        
        if (model) {
            RenderDMSModel(model, (Vector3){0.0f, 0.0f, 0.0f}, 0.5f, WHITE);
        }
        
        glKosSwapBuffers();
    }
    
    if (model) UnloadDMSModel(model);
    Toy_freeVM(&vm);
    free(updateScript);
    Toy_freeBucket(&bucket);
    
    return 0;
}