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
    GLuint   color;      /* attachment 0 */
    GLuint   color1;     /* attachment 1 (G-buffer albedo) */
    GLuint   color2;     /* attachment 2 (G-buffer normal+rough) */
    GLuint   color3;     /* attachment 3 (G-buffer emissive) */
    GLuint   depth;
    int      width;
    int      height;
    FBFormat format;
    int      attachment_count;   /* 1 for legacy, 4 for G-buffer */
} Framebuffer;

Framebuffer fb_create(int width, int height, FBFormat fmt);
Framebuffer fb_create_with_depth_tex(int width, int height, FBFormat fmt);

/* Create a 4-attachment G-buffer (lit, albedo, normal+rough, emissive).
   If depth_tex != 0, borrow that depth texture instead of allocating one.
   Ownership is not taken of borrowed depth — fb_destroy will not delete it. */
Framebuffer fb_create_gbuffer(int width, int height, GLuint depth_tex);
void        fb_destroy(Framebuffer* fb);
void        fb_bind(Framebuffer* fb);
void        fb_bind_read(Framebuffer* fb, int unit);
void        fb_unbind(int screen_w, int screen_h);

#endif
