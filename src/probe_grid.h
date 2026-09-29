#ifndef PROBE_GRID_H
#define PROBE_GRID_H

#include <GLES3/gl3.h>
#include "math3d.h"
#include "framebuffer.h"
#include "pass.h"
#include "tri_grid.h"

#define PROBE_COUNT_X 8
#define PROBE_COUNT_Y 4
#define PROBE_COUNT_Z 8
#define PROBE_COUNT   (PROBE_COUNT_X * PROBE_COUNT_Y * PROBE_COUNT_Z)
#define PROBE_SG_COUNT 32

typedef struct {
    Framebuffer fb;         /* (2*PROBE_SG_COUNT) x PROBE_COUNT, RGBA32F */
    Pass        bake_pass;  /* bakes all probes in one draw */
    vec3        grid_min;
    vec3        grid_max;
    int         valid;
} ProbeGrid;

ProbeGrid probe_grid_create(vec3 grid_min, vec3 grid_max);
void      probe_grid_destroy(ProbeGrid* pg);
void      probe_grid_bake_with_tris(ProbeGrid* pg, TriGrid* tg, vec3 car_grid_min, vec3 car_grid_max, float ground_y, vec3 ground_albedo, vec3 sky_horizon_color, vec3 sky_zenith_color, vec3 light_dir, vec3 light_color, float light_intensity);

void      probe_grid_bake(ProbeGrid* pg,
                          vec3  car_center, vec3 car_half_extent, vec3 car_albedo,
                          float ground_y,   vec3 ground_albedo,
                          vec3  sky_horizon_color, vec3 sky_zenith_color,
                          vec3  light_dir, vec3 light_color, float light_intensity);

#endif
