#include "mesh_merge.h"
#include "math3d.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Material signature: identifies meshes that share a visual material. */
typedef struct {
    GLuint tex_basecolor;
    GLuint tex_mr;
    GLuint tex_normal;
    GLuint tex_emissive;
    GLuint tex_occlusion;
    float  basecolor[3];
    float  metallic;
    float  roughness;
    int    has_bc, has_mr, has_nm;
} MatSig;

static MatSig mat_signature(const Material* m) {
    MatSig s;
    memset(&s, 0, sizeof(s));
    s.tex_basecolor = m->has_basecolor ? m->tex_basecolor : 0;
    s.tex_mr        = m->has_mr        ? m->tex_mr        : 0;
    s.tex_normal    = m->has_normal    ? m->tex_normal    : 0;
    s.tex_emissive  = m->tex_emissive;
    s.tex_occlusion = m->tex_occlusion;
    s.basecolor[0]  = m->basecolor_factor.x;
    s.basecolor[1]  = m->basecolor_factor.y;
    s.basecolor[2]  = m->basecolor_factor.z;
    s.metallic      = m->metallic_factor;
    s.roughness     = m->roughness_factor;
    s.has_bc = m->has_basecolor;
    s.has_mr = m->has_mr;
    s.has_nm = m->has_normal;
    return s;
}

static int mat_equal(const MatSig* a, const MatSig* b) {
    if (a->tex_basecolor != b->tex_basecolor) return 0;
    if (a->tex_mr        != b->tex_mr)        return 0;
    if (a->tex_normal    != b->tex_normal)    return 0;
    if (a->tex_emissive  != b->tex_emissive)  return 0;
    if (a->tex_occlusion != b->tex_occlusion) return 0;
    if (a->has_bc != b->has_bc) return 0;
    if (a->has_mr != b->has_mr) return 0;
    if (a->has_nm != b->has_nm) return 0;
    for (int i = 0; i < 3; ++i) {
        if (fabsf(a->basecolor[i] - b->basecolor[i]) > 1e-4f) return 0;
    }
    if (fabsf(a->metallic  - b->metallic)  > 1e-4f) return 0;
    if (fabsf(a->roughness - b->roughness) > 1e-4f) return 0;
    return 1;
}

/* Read a mesh's vertex data back from the GPU. Not possible directly —
   instead, we re-read from the source and re-pack. But we don't have the
   source data anymore.

   Better approach: capture the vertex data at load time. Since we don't
   have that, we need to either store it in Mesh or make this function
   accept the raw data.

   For this implementation, we assume the caller has retained the raw
   vertex data. Since our current Mesh doesn't, this merge will need a
   small change to mesh.c to store the raw arrays.

   Simplification: skip readback, and instead merge at load time inside
   model_load_glb. This merge function is a placeholder that returns the
   input unmodified until we move the merge into the loader. */

Model model_merge(Model* src) {
    /* Not implemented as a standalone function yet — see model_load_glb
       which will do the merge inline. */
    Model out = *src;
    return out;
}
