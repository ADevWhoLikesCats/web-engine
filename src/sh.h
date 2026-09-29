#ifndef SH_H
#define SH_H

#include <GLES3/gl3.h>
#include "framebuffer.h"
#include "pass.h"

typedef struct {
    Framebuffer fb;        /* 9x1 RGBA32F, one texel per SH coefficient */
    Pass        project;   /* sh_project.frag */
    Pass        reconstruct;/* sh_reconstruct.frag */
    GLuint      dummy_vao; /* for the reconstruct pass to render 9x1 */
} SH;

SH   sh_create(void);
void sh_destroy(SH* sh);

/* Project an octahedral capture into 9 SH coefficients. */
void sh_project(SH* sh, Framebuffer* octa_capture);

/* Reconstruct radiance in direction space, writing to the currently-bound FB. */
void sh_reconstruct(SH* sh, int width, int height, float exposure);

#endif
