#ifndef PASS_H
#define PASS_H

#include <GLES3/gl3.h>
#include "math3d.h"

typedef struct {
    GLuint prog;
    GLuint vao;    /* empty VAO — geometry comes from gl_VertexID */
} Pass;

Pass pass_create(const char* vert_path, const char* frag_path);
void pass_destroy(Pass* p);

void pass_use(Pass* p);
void pass_draw(Pass* p, int width, int height);

void pass_set_i32 (Pass* p, const char* name, int v);
void pass_set_f32 (Pass* p, const char* name, float v);
void pass_set_vec2(Pass* p, const char* name, vec2 v);
void pass_set_vec3(Pass* p, const char* name, vec3 v);
void pass_set_vec4(Pass* p, const char* name, vec4 v);
void pass_set_mat4(Pass* p, const char* name, mat4 m);
void pass_set_tex (Pass* p, const char* name, int unit);

#endif
