#ifndef TRI_GRID_H
#define TRI_GRID_H

#include <GLES3/gl3.h>
#include "math3d.h"

typedef struct {
    GLuint tri_tex;
    GLuint cell_tex;
    GLuint idx_tex;

    int    tri_count;
    int    grid_x, grid_y, grid_z;
    int    cell_count;
    int    total_entries;

    int    tri_tex_w, tri_tex_h, tri_per_row;
    int    idx_tex_w, idx_tex_h;

    vec3   grid_min;
    vec3   grid_max;
} TriGrid;

TriGrid tri_grid_build(const float* tri_positions,
                       const float* tri_albedos,
                       int count,
                       vec3 world_min, vec3 world_max);

void tri_grid_destroy(TriGrid* tg);

#endif
