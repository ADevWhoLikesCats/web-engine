#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <GLES3/gl3.h>

typedef enum {
    FB_RGBA8,
    FB_RGBA16F,
    FB_RGBA32F,
    FB_R8,
} FBFormat;

typedef struct {
    GLuint   fbo;
    GLuint   color;
    GLuint   depth;
    int      width;
    int      height;
    FBFormat format;
} Framebuffer;

Framebuffer fb_create(int width, int height, FBFormat fmt);
void        fb_destroy(Framebuffer* fb);
void        fb_bind(Framebuffer* fb);
void        fb_bind_read(Framebuffer* fb, int unit);
void        fb_unbind(int screen_w, int screen_h);

#endif
