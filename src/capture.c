#include "capture.h"
#include <stdio.h>

Capture capture_create(int size) {
    Capture c = {0};
    c.size = size;
    c.fb = fb_create(size, size, FB_RGBA16F);
    c.pass = pass_create("/shaders/fullscreen.vert", "/shaders/octa_capture.frag");
    return c;
}

void capture_destroy(Capture* c) {
    fb_destroy(&c->fb);
    pass_destroy(&c->pass);
}

void capture_run(Capture* c,
                 vec3 probe_pos,
                 vec3 light_dir,
                 vec3 light_color,
                 float light_intensity,
                 vec3 cam_pos)
{
    fb_bind(&c->fb);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    pass_use(&c->pass);
    pass_set_vec3(&c->pass, "uProbePos",       probe_pos);
    pass_set_vec3(&c->pass, "uLightDir",       light_dir);
    pass_set_vec3(&c->pass, "uLightColor",     light_color);
    pass_set_f32 (&c->pass, "uLightIntensity", light_intensity);
    pass_set_vec3(&c->pass, "uCamPos",         cam_pos);
    vec2 inv = { 1.0f / (float)c->size, 1.0f / (float)c->size };
    pass_set_vec2(&c->pass, "uInvResolution",  inv);
    pass_draw(&c->pass, c->size, c->size);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}
