#include "framebuffer.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static void check_float_support(void) {
    static int checked = 0;
    if (checked) return;
    checked = 1;
    if (!glGetString(GL_EXTENSIONS)) return;
    /* WebGL2: RGBA16F/32F as a color attachment needs EXT_color_buffer_float.
       Linear filtering of float textures needs OES_texture_float_linear.
       Half-float linear filtering is core in WebGL2. */
    const char* ext = (const char*)glGetString(GL_EXTENSIONS);
    if (!ext) return;
    if (!strstr(ext, "EXT_color_buffer_float")) {
        fprintf(stderr, "[fb] WARNING: EXT_color_buffer_float not advertised\n");
    }
}

#include <string.h>

static GLenum to_gl_format(FBFormat f, GLenum* internal, GLenum* type) {
    switch (f) {
        case FB_RGBA8:   *internal = GL_RGBA8;   *type = GL_UNSIGNED_BYTE;  return GL_RGBA;
        case FB_RGBA16F: *internal = GL_RGBA16F; *type = GL_HALF_FLOAT;     return GL_RGBA;
        case FB_RGBA32F: *internal = GL_RGBA32F; *type = GL_FLOAT;          return GL_RGBA;
        case FB_R8:      *internal = GL_R8;      *type = GL_UNSIGNED_BYTE;  return GL_RED;
    }
    *internal = GL_RGBA8; *type = GL_UNSIGNED_BYTE; return GL_RGBA;
}

Framebuffer fb_create(int width, int height, FBFormat fmt) {
    check_float_support();
    Framebuffer fb = {0};
    fb.width = width;
    fb.height = height;
    fb.format = fmt;

    GLenum internal, type, format;
    format = to_gl_format(fmt, &internal, &type);

    glGenTextures(1, &fb.color);
    glBindTexture(GL_TEXTURE_2D, fb.color);
    glTexImage2D(GL_TEXTURE_2D, 0, internal, width, height, 0, format, type, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenRenderbuffers(1, &fb.depth);
    glBindRenderbuffer(GL_RENDERBUFFER, fb.depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

    glGenFramebuffers(1, &fb.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fb.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fb.color, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, fb.depth);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "[fb] incomplete framebuffer (0x%04X) %dx%d fmt=%d\n",
                status, width, height, (int)fmt);
        abort();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    return fb;
}

void fb_destroy(Framebuffer* fb) {
    if (fb->color) glDeleteTextures(1, &fb->color);
    if (fb->depth) glDeleteRenderbuffers(1, &fb->depth);
    if (fb->fbo)   glDeleteFramebuffers(1, &fb->fbo);
    fb->color = fb->depth = fb->fbo = 0;
}

void fb_bind(Framebuffer* fb) {
    glBindFramebuffer(GL_FRAMEBUFFER, fb->fbo);
    glViewport(0, 0, fb->width, fb->height);
}

void fb_bind_read(Framebuffer* fb, int unit) {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, fb->color);
}

void fb_unbind(int screen_w, int screen_h) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, screen_w, screen_h);
}


Framebuffer fb_create_with_depth_tex(int width, int height, FBFormat fmt) {
    check_float_support();
    Framebuffer fb = {0};
    fb.width = width;
    fb.height = height;
    fb.format = fmt;

    GLenum internal, type, format;
    format = to_gl_format(fmt, &internal, &type);

    glGenTextures(1, &fb.color);
    glBindTexture(GL_TEXTURE_2D, fb.color);
    glTexImage2D(GL_TEXTURE_2D, 0, internal, width, height, 0, format, type, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    /* Depth texture instead of renderbuffer */
    glGenTextures(1, &fb.depth);
    glBindTexture(GL_TEXTURE_2D, fb.depth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0,
                 GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &fb.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fb.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fb.color, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, fb.depth, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "[fb] incomplete framebuffer (0x%04X) %dx%d fmt=%d\n",
                status, width, height, (int)fmt);
        abort();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    return fb;
}
