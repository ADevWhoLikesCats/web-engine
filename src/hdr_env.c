#include "hdr_env.h"
#include "asset.h"
#include "stb_image.h"
#include <stdio.h>
#include <stdlib.h>

HDREnv hdr_env_load(const char* path) {
    HDREnv env = {0};

    size_t len = 0;
    char* bytes = asset_load(path, &len);
    if (!bytes) {
        fprintf(stderr, "hdr_env: cannot load %s\n", path);
        return env;
    }

    int w = 0, h = 0, comp = 0;
    float* data = stbi_loadf_from_memory((const unsigned char*)bytes, (int)len,
                                          &w, &h, &comp, 4);
    free(bytes);

    if (!data) {
        fprintf(stderr, "hdr_env: stbi_loadf failed for %s\n", path);
        return env;
    }

    printf("hdr_env: loaded %s (%dx%d, %d channels)\n", path, w, h, comp);

    glGenTextures(1, &env.tex);
    glBindTexture(GL_TEXTURE_2D, env.tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, data);

    /* No mipmaps — keeps the texture always-complete and filterable */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    stbi_image_free(data);

    env.width = w;
    env.height = h;
    env.valid = 1;
    return env;
}

void hdr_env_destroy(HDREnv* h) {
    if (h->tex) glDeleteTextures(1, &h->tex);
    h->tex = 0;
    h->valid = 0;
}
