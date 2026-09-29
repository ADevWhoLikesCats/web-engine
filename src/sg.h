#ifndef SG_H
#define SG_H

#include <GLES3/gl3.h>
#include "framebuffer.h"
#include "pass.h"

#define SG_COUNT 32
#define SG_SHARPNESS 4.0f

typedef struct {
    Framebuffer fb;
    Pass        fit;
    GLint       u_axis_sharp;
} SG;

SG   sg_create(void);
void sg_destroy(SG* sg);
void sg_fit(SG* sg, Framebuffer* octa_capture);

#endif
