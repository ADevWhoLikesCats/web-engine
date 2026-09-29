#include "pass.h"
#include "asset.h"
#include <stdio.h>
#include <stdlib.h>

static GLuint compile(GLenum type, const char* src, const char* path) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(s, sizeof(log), NULL, log);
        fprintf(stderr, "pass shader compile failed [%s]:\n%s\n", path, log);
        abort();
    }
    return s;
}

static GLuint load_shader(GLenum type, const char* path) {
    size_t len;
    char* src = asset_load(path, &len);
    if (!src) { fprintf(stderr, "pass missing shader: %s\n", path); abort(); }
    GLuint s = compile(type, src, path);
    free(src);
    return s;
}

Pass pass_create(const char* vert_path, const char* frag_path) {
    Pass p = {0};
    GLuint vs = load_shader(GL_VERTEX_SHADER,   vert_path);
    GLuint fs = load_shader(GL_FRAGMENT_SHADER, frag_path);

    p.prog = glCreateProgram();
    glAttachShader(p.prog, vs);
    glAttachShader(p.prog, fs);
    glLinkProgram(p.prog);
    GLint ok = 0;
    glGetProgramiv(p.prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetProgramInfoLog(p.prog, sizeof(log), NULL, log);
        fprintf(stderr, "pass link failed [%s + %s]:\n%s\n", vert_path, frag_path, log);
        abort();
    }
    glDeleteShader(vs);
    glDeleteShader(fs);

    glGenVertexArrays(1, &p.vao);
    return p;
}

void pass_destroy(Pass* p) {
    if (p->prog) glDeleteProgram(p->prog);
    if (p->vao)  glDeleteVertexArrays(1, &p->vao);
    p->prog = p->vao = 0;
}

void pass_use(Pass* p) {
    glUseProgram(p->prog);
    glBindVertexArray(p->vao);
}

void pass_draw(Pass* p, int width, int height) {
    (void)width; (void)height;
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void pass_set_i32 (Pass* p, const char* name, int v)   { glUniform1i(glGetUniformLocation(p->prog, name), v); }
void pass_set_f32 (Pass* p, const char* name, float v) { glUniform1f(glGetUniformLocation(p->prog, name), v); }
void pass_set_vec2(Pass* p, const char* name, vec2 v)  { glUniform2f(glGetUniformLocation(p->prog, name), v.x, v.y); }
void pass_set_vec3(Pass* p, const char* name, vec3 v)  { glUniform3f(glGetUniformLocation(p->prog, name), v.x, v.y, v.z); }
void pass_set_vec4(Pass* p, const char* name, vec4 v)  { glUniform4f(glGetUniformLocation(p->prog, name), v.x, v.y, v.z, v.w); }
void pass_set_mat4(Pass* p, const char* name, mat4 m)  { glUniformMatrix4fv(glGetUniformLocation(p->prog, name), 1, GL_FALSE, m.m); }
void pass_set_tex (Pass* p, const char* name, int unit){ glUniform1i(glGetUniformLocation(p->prog, name), unit); }
