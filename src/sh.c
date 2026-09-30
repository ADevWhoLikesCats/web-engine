#include "sh.h"
#include "asset.h"
#include <stdio.h>

extern GLuint sh_make_shader_program(const char* vsp, const char* fsp); /* forward decl */

SH sh_create(void) {
    SH sh = {0};
    /* 9x1 RGBA32F */
    sh.fb = fb_create(9, 1, FB_RGBA32F);
    sh.project = pass_create("/shaders/fullscreen.vert", "/shaders/sh_project.frag");
    sh.reconstruct = pass_create("/shaders/fullscreen.vert", "/shaders/sh_reconstruct.frag");
    return sh;
}

void sh_destroy(SH* sh) {
    fb_destroy(&sh->fb);
    pass_destroy(&sh->project);
    pass_destroy(&sh->reconstruct);
}

void sh_project(SH* sh, Framebuffer* octa_capture) {
    fb_bind(&sh->fb);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    pass_use(&sh->project);
    fb_bind_read(octa_capture, 0);
    pass_set_tex(&sh->project, "uOcta", 0);
    pass_set_f32(&sh->project, "uOctaSize", (float)octa_capture->width);
    pass_draw(&sh->project, sh->fb.width, sh->fb.height);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

void sh_reconstruct(SH* sh, int width, int height, float exposure) {
    pass_use(&sh->reconstruct);
    fb_bind_read(&sh->fb, 0);
    pass_set_tex(&sh->reconstruct, "uSH", 0);
    vec2 inv = { 1.0f / (float)width, 1.0f / (float)height };
    pass_set_vec2(&sh->reconstruct, "uInvResolution", inv);
    pass_set_f32 (&sh->reconstruct, "uExposure", exposure);
    pass_draw(&sh->reconstruct, width, height);
}
