#ifndef SHADOW_H
#define SHADOW_H

#include <GLES3/gl3.h>
#include "math3d.h"
#include "mesh.h"

typedef struct {
    GLuint fbo;
    GLuint depth_tex;
    int    size;
    mat4   light_vp;   /* updated each frame by shadow_update_matrix */
} ShadowMap;

ShadowMap shadow_create(int size);
void      shadow_destroy(ShadowMap* sm);

/* Compute a light-view-projection matrix that fits the scene bounding sphere. */
void shadow_update_matrix(ShadowMap* sm, vec3 scene_center, float scene_radius, vec3 light_dir);

/* Render depth from light's POV into the shadow map. */
void shadow_render(ShadowMap* sm, Mesh* meshes, int mesh_count);

#endif
