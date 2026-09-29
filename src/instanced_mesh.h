#ifndef INSTANCED_MESH_H
#define INSTANCED_MESH_H

#include <GLES3/gl3.h>
#include "math3d.h"

/* An instanced mesh wraps a single source Mesh and a buffer of N instance
   transforms. Drawn with glDrawElementsInstanced in one call. */
typedef struct {
    GLuint vao;
    GLuint instance_vbo;
    GLuint index_count;      /* with bit 31 set if 16-bit indices */
    int    instance_count;
} InstancedMesh;

/* Create an instanced mesh from base geometry (VBO/EBO already uploaded by
   the source mesh) and an array of N model matrices. */
InstancedMesh instanced_mesh_create(GLuint base_vao,
                                    GLuint base_vbo,
                                    GLuint base_ebo,
                                    GLuint index_count,
                                    const mat4* transforms,
                                    int instance_count);

void instanced_mesh_destroy(InstancedMesh* im);

/* Draw — caller sets up program + uniforms. */
void instanced_mesh_draw(InstancedMesh* im);

/* Update instance transforms in-place (for dynamic scenes). */
void instanced_mesh_update(InstancedMesh* im, const mat4* transforms, int count);

#endif
