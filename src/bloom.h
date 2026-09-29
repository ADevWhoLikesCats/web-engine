#ifndef BLOOM_H
#define BLOOM_H

#include <GLES3/gl3.h>
#include "framebuffer.h"
#include "pass.h"

#define BLOOM_MIPS 6

typedef struct {
    Framebuffer mips[BLOOM_MIPS];   /* half-resolution chain */
    Pass bright;
    Pass downsample;
    Pass upsample;
} Bloom;

Bloom bloom_create(int width, int height);
void  bloom_destroy(Bloom* b);
void  bloom_resize(Bloom* b, int width, int height);
void  bloom_run(Bloom* b, Framebuffer* scene_hdr, float threshold);

#endif
