#include "shadow.h"
#include "asset.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Simple depth-only program */
static GLuint g_depth_prog = 0;
static GLint  g_u_light_vp = -1;
static GLint  g_u_model = -1;

static GLuint compile(GLenum type, const char* src, const char* path) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(s, sizeof(log), NULL, log);
        fprintf(stderr, "shadow shader compile failed [%s]:\n%s\n", path, log);
        abort();
    }
    return s;
}
static GLuint load_shader(GLenum type, const char* path) {
    size_t len;
    char* src = asset_load(path, &len);
    if (!src) { fprintf(stderr, "shadow missing shader: %s\n", path); abort(); }
    GLuint s = compile(type, src, path);
    free(src);
    return s;
}

static void ensure_depth_program(void) {
    if (g_depth_prog) return;
    GLuint vs = load_shader(GL_VERTEX_SHADER,   "/shaders/shadow_depth.vert");
    GLuint fs = load_shader(GL_FRAGMENT_SHADER, "/shaders/shadow_depth.frag");
    g_depth_prog = glCreateProgram();
    glAttachShader(g_depth_prog, vs);
    glAttachShader(g_depth_prog, fs);
    glLinkProgram(g_depth_prog);
    GLint ok = 0;
    glGetProgramiv(g_depth_prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetProgramInfoLog(g_depth_prog, sizeof(log), NULL, log);
        fprintf(stderr, "shadow link failed:\n%s\n", log);
        abort();
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    g_u_light_vp = glGetUniformLocation(g_depth_prog, "uLightVP");
    g_u_model    = glGetUniformLocation(g_depth_prog, "uModel");
}

ShadowMap shadow_create(int size) {
    ensure_depth_program();

    ShadowMap sm = {0};
    sm.size = size;

    glGenTextures(1, &sm.depth_tex);
    glBindTexture(GL_TEXTURE_2D, sm.depth_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, size, size, 0,
                 GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    /* Depth textures in WebGL2 don't support LINEAR via comparison; use NEAREST.
       We'll do PCF manually in the shader. */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glGenFramebuffers(1, &sm.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, sm.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, sm.depth_tex, 0);
    glDrawBuffers(0, NULL);  /* No color attachments */
    glReadBuffer(GL_NONE);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "shadow framebuffer incomplete: 0x%04X\n", status);
        abort();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    sm.light_vp = m4_identity();
    return sm;
}

void shadow_destroy(ShadowMap* sm) {
    if (sm->depth_tex) glDeleteTextures(1, &sm->depth_tex);
    if (sm->fbo)       glDeleteFramebuffers(1, &sm->fbo);
    sm->depth_tex = sm->fbo = 0;
}

/* Orthographic projection, right-handed, mapping depth to [0,1] for storage
   and back to [-1,1] for sampling. Standard GL ortho produces [-1,1] depth,
   which WebGL remaps to [0,1] on write. The PBR shader will do the same. */
static mat4 ortho(float l, float r, float b, float t, float n, float f) {
    mat4 m = m4_identity();
    m.m[0]  = 2.0f / (r - l);
    m.m[5]  = 2.0f / (t - b);
    m.m[10] = -2.0f / (f - n);
    m.m[12] = -(r + l) / (r - l);
    m.m[13] = -(t + b) / (t - b);
    m.m[14] = -(f + n) / (f - n);
    return m;
}

void shadow_update_matrix(ShadowMap* sm, vec3 scene_center, float scene_radius, vec3 light_dir) {
    /* Light position: far back along the light direction */
    float d = scene_radius * 2.0f;
    vec3 light_pos = v3_add(scene_center, v3_scale(v3_norm(light_dir), -d));

    vec3 up = v3(0.0f, 1.0f, 0.0f);
    /* If light is nearly straight up/down, use a different up vector to avoid degenerate look-at */
    if (fabsf(v3_dot(v3_norm(light_dir), up)) > 0.99f) {
        up = v3(0.0f, 0.0f, 1.0f);
    }

    mat4 view = m4_look_at(light_pos, scene_center, up);
    float r = scene_radius * 1.2f;
    mat4 proj = ortho(-r, r, -r, r, 0.1f, d + r);
    sm->light_vp = m4_mul(proj, view);
}

void shadow_render(ShadowMap* sm, Mesh* meshes, int mesh_count) {
    glBindFramebuffer(GL_FRAMEBUFFER, sm->fbo);
    glViewport(0, 0, sm->size, sm->size);
    glClearDepthf(1.0f);
    glClear(GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);   /* Render back faces into shadow map to reduce peter-panning */

    glUseProgram(g_depth_prog);
    glUniformMatrix4fv(g_u_light_vp, 1, GL_FALSE, sm->light_vp.m);

    for (int i = 0; i < mesh_count; ++i) {
        Mesh* m = &meshes[i];
        glUniformMatrix4fv(g_u_model, 1, GL_FALSE, m->model.m);
        glBindVertexArray(m->vao);
        int use_16 = (m->index_count & 0x80000000) != 0;
        GLuint count = m->index_count & 0x7FFFFFFF;
        glDrawElements(GL_TRIANGLES, count, use_16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, 0);
    }

    glCullFace(GL_BACK);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    /* Viewport gets restored by the caller */
}
