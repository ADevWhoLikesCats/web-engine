#include "ground.h"
#include "texture.h"
#include <stdlib.h>

Mesh ground_create(float y_level, float half_size) {
    Mesh m = {0};

    float y = y_level;
    float s = half_size;

    /* Interleaved: pos3, nrm3, uv2, tan4 = 12 floats */
    float verts[] = {
        /* x     y   z      nx  ny  nz     u     v     tx  ty  tz  tw */
        -s, y, -s,   0, 1, 0,   0, 0,   1, 0, 0, 1,
         s, y, -s,   0, 1, 0,   1, 0,   1, 0, 0, 1,
         s, y,  s,   0, 1, 0,   1, 1,   1, 0, 0, 1,
        -s, y,  s,   0, 1, 0,   0, 1,   1, 0, 0, 1,
    };
    unsigned short indices[] = { 0, 1, 2, 0, 2, 3 };

    glGenVertexArrays(1, &m.vao);
    glBindVertexArray(m.vao);

    glGenBuffers(1, &m.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12*sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 12*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 12*sizeof(float), (void*)(6*sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 12*sizeof(float), (void*)(8*sizeof(float)));

    glGenBuffers(1, &m.ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glBindVertexArray(0);

    m.index_count = 6 | 0x80000000;   /* 16-bit indices */
    m.model = m4_identity();

    /* Material */
    m.material = (Material){0};
    m.material.basecolor_factor = v3(0.10f, 0.10f, 0.12f);
    m.material.metallic_factor  = 0.0f;
    m.material.roughness_factor = 0.9;
    m.material.normal_scale     = 1.0f;
    m.material.occlusion_strength = 1.0f;
    m.material.emissive_strength  = 1.0f;
    m.material.tex_basecolor = texture_solid(255, 255, 255, 255, 1);
    m.material.tex_normal    = texture_solid(128, 128, 255, 255, 0);
    m.material.tex_mr        = texture_solid(255, 140, 0, 255, 0);   /* G=rough 0.55, B=metal 0 */
    m.material.tex_emissive  = texture_solid(0, 0, 0, 255, 1);
    m.material.tex_occlusion = texture_solid(255, 255, 255, 255, 0);
    m.material.has_basecolor = 0;   /* use the factor, no texture */
    m.material.has_mr        = 0;   /* use the factor */
    m.material.has_normal    = 0;
    return m;
}
