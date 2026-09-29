#ifndef HDR_ENV_H
#define HDR_ENV_H

#include <GLES3/gl3.h>
#include "math3d.h"

typedef struct {
    GLuint tex;
    int    width;
    int    height;
    int    valid;
} HDREnv;

HDREnv hdr_env_load(const char* path);
void   hdr_env_destroy(HDREnv* h);

#endif
