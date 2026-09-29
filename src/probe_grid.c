#include "probe_grid.h"
#include <stdio.h>
#include <math.h>

ProbeGrid probe_grid_create(vec3 grid_min, vec3 grid_max) {
    ProbeGrid pg = {0};
    pg.grid_min = grid_min;
    pg.grid_max = grid_max;
    pg.fb = fb_create(2 * PROBE_SG_COUNT, PROBE_COUNT, FB_RGBA32F);
    pg.bake_pass = pass_create("/shaders/fullscreen.vert", "/shaders/probe_bake.frag");
    pg.valid = 0;
    return pg;
}

void probe_grid_destroy(ProbeGrid* pg) {
    fb_destroy(&pg->fb);
    pass_destroy(&pg->bake_pass);
}

void probe_grid_bake(ProbeGrid* pg,
                     vec3  car_center, vec3 car_half_extent, vec3 car_albedo,
                     float ground_y,   vec3 ground_albedo,
                     vec3  sky_horizon_color, vec3 sky_zenith_color,
                     vec3  light_dir, vec3 light_color, float light_intensity)
{
    fb_bind(&pg->fb);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    pass_use(&pg->bake_pass);
    pass_set_vec3(&pg->bake_pass, "uGridMin",     pg->grid_min);
    pass_set_vec3(&pg->bake_pass, "uGridMax",     pg->grid_max);
    pass_set_vec3(&pg->bake_pass, "uCarCenter",   car_center);
    pass_set_vec3(&pg->bake_pass, "uCarHalf",     car_half_extent);
    pass_set_vec3(&pg->bake_pass, "uCarAlbedo",   car_albedo);
    pass_set_f32 (&pg->bake_pass, "uGroundY",     ground_y);
    pass_set_vec3(&pg->bake_pass, "uGroundAlbedo", ground_albedo);
    pass_set_vec3(&pg->bake_pass, "uSkyHorizon",  sky_horizon_color);
    pass_set_vec3(&pg->bake_pass, "uSkyZenith",   sky_zenith_color);
    pass_set_vec3(&pg->bake_pass, "uLightDir",    light_dir);
    pass_set_vec3(&pg->bake_pass, "uLightColor",  light_color);
    pass_set_f32 (&pg->bake_pass, "uLightIntensity", light_intensity);
    pass_set_i32 (&pg->bake_pass, "uProbeSGCount", PROBE_SG_COUNT);

    glDrawArrays(GL_TRIANGLES, 0, 3);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    pg->valid = 1;
    printf("probe_grid: baked %d probes, %d SGs each\n", PROBE_COUNT, PROBE_SG_COUNT);
}


void probe_grid_bake_with_tris(ProbeGrid* pg, TriGrid* tg,
                               vec3 car_grid_min, vec3 car_grid_max,
                               float ground_y, vec3 ground_albedo,
                               vec3 sky_horizon_color, vec3 sky_zenith_color,
                               vec3 light_dir, vec3 light_color, float light_intensity)
{
    fb_bind(&pg->fb);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    pass_use(&pg->bake_pass);
    pass_set_vec3(&pg->bake_pass, "uGridMin",     pg->grid_min);
    pass_set_vec3(&pg->bake_pass, "uGridMax",     pg->grid_max);
    pass_set_vec3(&pg->bake_pass, "uCarGridMin",  car_grid_min);
    pass_set_vec3(&pg->bake_pass, "uCarGridMax",  car_grid_max);
    pass_set_f32 (&pg->bake_pass, "uGroundY",     ground_y);
    pass_set_vec3(&pg->bake_pass, "uGroundAlbedo", ground_albedo);
    pass_set_vec3(&pg->bake_pass, "uSkyHorizon",  sky_horizon_color);
    pass_set_vec3(&pg->bake_pass, "uSkyZenith",   sky_zenith_color);
    pass_set_vec3(&pg->bake_pass, "uLightDir",    light_dir);
    pass_set_vec3(&pg->bake_pass, "uLightColor",  light_color);
    pass_set_f32 (&pg->bake_pass, "uLightIntensity", light_intensity);
    pass_set_i32 (&pg->bake_pass, "uTriPerRow",   tg->tri_per_row);
    pass_set_f32 (&pg->bake_pass, "uIdxTexW",     (float)tg->idx_tex_w);

    /* Bind textures: 0=tri, 1=cell, 2=idx */
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tg->tri_tex);
    pass_set_i32(&pg->bake_pass, "uTriTex", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, tg->cell_tex);
    pass_set_i32(&pg->bake_pass, "uCellTex", 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, tg->idx_tex);
    pass_set_i32(&pg->bake_pass, "uIdxTex", 2);

    glActiveTexture(GL_TEXTURE0);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    pg->valid = 1;
    printf("probe_grid: baked %d probes with triangle grid\n", PROBE_COUNT);
}
