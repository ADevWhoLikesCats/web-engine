#include "sg.h"
#include <stdio.h>
#include <math.h>

SG sg_create(void) {
    SG sg = {0};
    sg.fb = fb_create(2 * SG_COUNT, 1, FB_RGBA32F);
    sg.fit = pass_create("/shaders/fullscreen.vert", "/shaders/sg_fit.frag");
    sg.u_axis_sharp = glGetUniformLocation(sg.fit.prog, "uSGAxisSharp");
    return sg;
}

void sg_destroy(SG* sg) {
    fb_destroy(&sg->fb);
    pass_destroy(&sg->fit);
}

void sg_fit(SG* sg, Framebuffer* octa_capture) {
    fb_bind(&sg->fb);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    pass_use(&sg->fit);

    /* Fibonacci sphere placement: evenly distributed N directions. */
    float axis_sharp[SG_COUNT * 4];
    const float golden_angle = 3.14159265f * (1.0f + sqrtf(5.0f));
    for (int i = 0; i < SG_COUNT; ++i) {
        float phi   = acosf(1.0f - 2.0f * ((float)i + 0.5f) / (float)SG_COUNT);
        float theta = golden_angle * (float)i;
        axis_sharp[i * 4 + 0] = sinf(phi) * cosf(theta);
        axis_sharp[i * 4 + 1] = sinf(phi) * sinf(theta);
        axis_sharp[i * 4 + 2] = cosf(phi);
        axis_sharp[i * 4 + 3] = SG_SHARPNESS;
    }
    glUniform4fv(sg->u_axis_sharp, SG_COUNT, axis_sharp);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, octa_capture->color);
    pass_set_tex(&sg->fit, "uOcta", 0);
    pass_set_f32(&sg->fit, "uOctaSize", (float)octa_capture->width);

    glDrawArrays(GL_TRIANGLES, 0, 3);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}
