#include "bloom.h"
#include <stdio.h>

Bloom bloom_create(int width, int height) {
    Bloom b = {0};
    b.bright     = pass_create("/shaders/fullscreen.vert", "/shaders/bloom_bright.frag");
    b.downsample = pass_create("/shaders/fullscreen.vert", "/shaders/bloom_downsample.frag");
    b.upsample   = pass_create("/shaders/fullscreen.vert", "/shaders/bloom_upsample.frag");
    bloom_resize(&b, width, height);
    return b;
}

void bloom_resize(Bloom* b, int width, int height) {
    int w = width / 2;
    int h = height / 2;
    for (int i = 0; i < BLOOM_MIPS; ++i) {
        if (b->mips[i].fbo) fb_destroy(&b->mips[i]);
        if (w < 1) w = 1;
        if (h < 1) h = 1;
        b->mips[i] = fb_create(w, h, FB_RGBA16F);
        w /= 2;
        h /= 2;
    }
}

void bloom_destroy(Bloom* b) {
    for (int i = 0; i < BLOOM_MIPS; ++i) fb_destroy(&b->mips[i]);
    pass_destroy(&b->bright);
    pass_destroy(&b->downsample);
    pass_destroy(&b->upsample);
}

void bloom_run(Bloom* b, Framebuffer* scene_hdr, float threshold) {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    /* 1. Bright pass -> mip 0 */
    fb_bind(&b->mips[0]);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    pass_use(&b->bright);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, scene_hdr->color);
    pass_set_i32(&b->bright, "uScene", 0);
    vec2 texel = { 1.0f / (float)scene_hdr->width, 1.0f / (float)scene_hdr->height };
    pass_set_vec2(&b->bright, "uTexel", texel);
    pass_set_f32(&b->bright, "uThreshold", threshold);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    /* 2. Downsample chain: mip 0 -> mip 1 -> ... -> mip N-1 */
    pass_use(&b->downsample);
    for (int i = 1; i < BLOOM_MIPS; ++i) {
        fb_bind(&b->mips[i]);
        glClear(GL_COLOR_BUFFER_BIT);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, b->mips[i-1].color);
        pass_set_i32(&b->downsample, "uSource", 0);
        vec2 t = { 1.0f / (float)b->mips[i-1].width, 1.0f / (float)b->mips[i-1].height };
        pass_set_vec2(&b->downsample, "uTexel", t);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    /* 3. Upsample chain: mip N-1 -> N-2 -> ... -> mip 1 */
    /* The largest mip (mip 0) is where we accumulate the final bloom */
    pass_use(&b->upsample);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    for (int i = BLOOM_MIPS - 1; i > 0; --i) {
        fb_bind(&b->mips[i-1]);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, b->mips[i].color);
        pass_set_i32(&b->upsample, "uSource", 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, b->mips[i-1].color);
        pass_set_i32(&b->upsample, "uTarget", 1);
        vec2 t = { 1.0f / (float)b->mips[i-1].width, 1.0f / (float)b->mips[i-1].height };
        pass_set_vec2(&b->upsample, "uTexel", t);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    glDisable(GL_BLEND);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}
