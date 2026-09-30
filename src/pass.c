#include "pass.h"
#include "asset.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Program creation                                                    */
/* ------------------------------------------------------------------ */

static GLuint compile_shader(GLenum type, const char* src, const char* path) {
    GLuint s = glCreateShader(type);
    if (!s) {
        fprintf(stderr, "pass: glCreateShader failed for %s\n", path);
        abort();
    }
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);

    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(s, sizeof(log), NULL, log);
        fprintf(stderr, "pass: shader compile failed [%s]:\n%s\n", path, log);
        abort();
    }
    return s;
}

static GLuint load_shader(GLenum type, const char* path) {
    size_t len = 0;
    char* src = asset_load(path, &len);
    if (!src) {
        fprintf(stderr, "pass: missing shader %s\n", path);
        abort();
    }
    GLuint s = compile_shader(type, src, path);
    free(src);
    return s;
}

/* Drain any GL errors that accumulated before we start. */
static void clear_gl_errors(void) {
    while (glGetError() != GL_NO_ERROR) {}
}

Pass pass_create(const char* vert_path, const char* frag_path) {
    /* Start with a clean error state — a stale error from earlier code
       would otherwise be reported as belonging to this pass. */
    clear_gl_errors();

    Pass p;
    memset(&p, 0, sizeof(p));

    GLuint vs = load_shader(GL_VERTEX_SHADER,   vert_path);
    GLuint fs = load_shader(GL_FRAGMENT_SHADER, frag_path);

    p.prog = glCreateProgram();
    if (!p.prog) {
        fprintf(stderr, "pass: glCreateProgram failed [%s + %s]\n",
                vert_path, frag_path);
        abort();
    }

    glAttachShader(p.prog, vs);
    glAttachShader(p.prog, fs);
    glLinkProgram(p.prog);

    /* Firefox + Intel HD Graphics defer the link. Query something that
       forces the driver to actually complete it before we check status. */
    GLint n_uniforms = 0;
    GLint n_attribs  = 0;
    glGetProgramiv(p.prog, GL_ACTIVE_UNIFORMS,   &n_uniforms);
    glGetProgramiv(p.prog, GL_ACTIVE_ATTRIBUTES, &n_attribs);

    GLint ok = 0;
    glGetProgramiv(p.prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetProgramInfoLog(p.prog, sizeof(log), NULL, log);
        fprintf(stderr, "pass: link failed [%s + %s]:\n%s\n",
                vert_path, frag_path, log);
        abort();
    }

    /* Don't delete the shaders. On some Intel drivers, deleting a shader
       that is attached to a program causes the program itself to become
       unusable. The memory cost is negligible (a few KB per pass). */
    (void)vs;
    (void)fs;

    /* Create the VAO. Some drivers require the VAO to be bound before
       any glVertexAttribPointer calls, but this engine uses the
       gl_VertexID-only pattern, so the VAO is empty and just needs to
       exist. Bind it once to verify it was created correctly. */
    glGenVertexArrays(1, &p.vao);
    if (!p.vao) {
        fprintf(stderr, "pass: glGenVertexArrays failed [%s + %s]\n",
                vert_path, frag_path);
        abort();
    }

    /* Intel HD Graphics 400/500 series require at least one enabled vertex
       attribute for a draw call to rasterize, even for gl_VertexID-only
       shaders. Bind a dummy 1-element position attribute. */
    glBindVertexArray(p.vao);

    GLuint dummy_vbo = 0;
    glGenBuffers(1, &dummy_vbo);
    static const float dummy_pos[3] = { 0.0f, 0.0f, 0.0f };
    glBindBuffer(GL_ARRAY_BUFFER, dummy_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(dummy_pos), dummy_pos, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

    /* The VBO is referenced by the VAO for the lifetime of the pass.
       Do not delete it — deleting would invalidate the VAO. */

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    /* Final verification: bind the program and the VAO and confirm the
       full state is valid. */
    glUseProgram(p.prog);
    glBindVertexArray(p.vao);
    glGetProgramiv(p.prog, GL_LINK_STATUS, &ok);
    glUseProgram(0);
    glBindVertexArray(0);

    if (!ok) {
        fprintf(stderr, "pass: post-link verification failed [%s + %s]\n",
                vert_path, frag_path);
        abort();
    }

    clear_gl_errors();
    fprintf(stderr, "[inside pass_create] p.prog=%u p.vao=%u\n", p.prog, p.vao);
    return p;
}

/* ------------------------------------------------------------------ */
/* Cleanup                                                             */
/* ------------------------------------------------------------------ */

void pass_destroy(Pass* p) {
    if (!p) return;
    if (p->prog) glDeleteProgram(p->prog);
    if (p->vao)  glDeleteVertexArrays(1, &p->vao);
    p->prog = 0;
    p->vao  = 0;
}

/* ------------------------------------------------------------------ */
/* Bind and draw                                                       */
/* ------------------------------------------------------------------ */

void pass_use(Pass* p) {
    glUseProgram(p->prog);
    glBindVertexArray(p->vao);
    GLenum e = glGetError();
    if (e != GL_NO_ERROR) {
        fprintf(stderr, "pass_use failed: prog=%u vao=%u err=0x%04X\n",
                p->prog, p->vao, e);
        while (glGetError() != GL_NO_ERROR) {}
    }
}

void pass_draw(Pass* p, int width, int height) {
    (void)p;
    (void)width;
    (void)height;
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

/* ------------------------------------------------------------------ */
/* Uniform setters                                                     */
/*                                                                     */
/* These assume the caller has already called pass_use(p) so that the */
/* program is bound. If it isn't, glUniform* silently no-ops (or emits */
/* an INVALID_OPERATION), so we guard by binding explicitly.          */
/* ------------------------------------------------------------------ */

void pass_set_i32(Pass* p, const char* name, int v) {
    glUseProgram(p->prog);
    GLint loc = glGetUniformLocation(p->prog, name);
    if (loc >= 0) glUniform1i(loc, v);
}

void pass_set_f32(Pass* p, const char* name, float v) {
    glUseProgram(p->prog);
    GLint loc = glGetUniformLocation(p->prog, name);
    if (loc >= 0) glUniform1f(loc, v);
}

void pass_set_vec2(Pass* p, const char* name, vec2 v) {
    glUseProgram(p->prog);
    GLint loc = glGetUniformLocation(p->prog, name);
    if (loc >= 0) glUniform2f(loc, v.x, v.y);
}

void pass_set_vec3(Pass* p, const char* name, vec3 v) {
    glUseProgram(p->prog);
    GLint loc = glGetUniformLocation(p->prog, name);
    if (loc >= 0) glUniform3f(loc, v.x, v.y, v.z);
}

void pass_set_vec4(Pass* p, const char* name, vec4 v) {
    glUseProgram(p->prog);
    GLint loc = glGetUniformLocation(p->prog, name);
    if (loc >= 0) glUniform4f(loc, v.x, v.y, v.z, v.w);
}

void pass_set_mat4(Pass* p, const char* name, mat4 m) {
    glUseProgram(p->prog);
    GLint loc = glGetUniformLocation(p->prog, name);
    if (loc >= 0) glUniformMatrix4fv(loc, 1, GL_FALSE, m.m);
}

void pass_set_tex(Pass* p, const char* name, int unit) {
    glUseProgram(p->prog);
    GLint loc = glGetUniformLocation(p->prog, name);
    if (loc >= 0) glUniform1i(loc, unit);
}
