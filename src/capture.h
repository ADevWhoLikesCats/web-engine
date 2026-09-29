#ifndef CAPTURE_H
#define CAPTURE_H

#include <GLES3/gl3.h>
#include "framebuffer.h"
#include "pass.h"
#include "math3d.h"

typedef struct {
    Framebuffer fb;     /* RGB16F octahedral target, size x size */
    Pass        pass;   /* octa_capture.frag */
    int         size;   /* square resolution */
} Capture;

Capture capture_create(int size);
void    capture_destroy(Capture* c);
void    capture_run(Capture* c,
                    vec3 probe_pos,
                    vec3 light_dir,
                    vec3 light_color,
                    float light_intensity,
                    vec3 cam_pos);

#endif
