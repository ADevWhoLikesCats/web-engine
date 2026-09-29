#ifndef MESH_H
#define MESH_H

#include <GLES3/gl3.h>
#include "math3d.h"

typedef struct {
    /* Textures */
    GLuint tex_basecolor;
    GLuint tex_normal;
    GLuint tex_mr;         /* G=roughness, B=metalness */
    GLuint tex_emissive;
    GLuint tex_occlusion;

    /* Factors from glTF */
    vec3  basecolor_factor;
    float metallic_factor;
    float roughness_factor;
    float normal_scale;
    float occlusion_strength;
    float emissive_strength;

    int has_basecolor;
    int has_normal;
    int has_mr;
    int has_emissive;
    int has_occlusion;
    int double_sided;
} Material;

typedef struct {
    GLuint vao;
    GLuint vbo;
    GLuint ebo;
    GLuint index_count;
    Material material;

    /* Model transform for this primitive (its node's world matrix) */
    mat4 model;
} Mesh;

typedef struct {
    Mesh*   meshes;
    int     count;
    /* Bounding info for auto-scaling */
    vec3    min_bb;
    vec3    max_bb;
} Model;

/* Load a .glb file from the virtual FS path. */
Model model_load_glb(const char* path);
void  model_destroy(Model* m);

/* Compute overall bounding box center and extent. */
vec3 model_center(Model* m);
vec3 model_extent(Model* m);

/* CPU-side triangle data populated by model_load_glb (for SG bake) */
extern float* g_mesh_cpu_tris;
extern float* g_mesh_cpu_albedos;
extern int    g_mesh_cpu_tri_count;

#endif
