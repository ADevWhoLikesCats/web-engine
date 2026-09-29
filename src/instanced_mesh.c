#include "instanced_mesh.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

InstancedMesh instanced_mesh_create(GLuint base_vao,
                                    GLuint base_vbo,
                                    GLuint base_ebo,
                                    GLuint index_count,
                                    const mat4* transforms,
                                    int instance_count)
{
    (void)base_vbo; (void)base_ebo;

    InstancedMesh im = {0};
    im.index_count = index_count;
    im.instance_count = instance_count;

    glGenVertexArrays(1, &im.vao);
    glBindVertexArray(im.vao);

    /* We need the source mesh's attribute layout. Rebind the source VBO
       and re-establish attribute pointers on this new VAO. This is
       cheaper than copying data, and both VAOs share the same buffers. */
    /* Note: caller is responsible for the VAO already having attributes
       bound to base_vbo. We rebind those here. */

    /* We can't introspect the source VAO's layout portably, so we assume
       the standard layout used throughout the engine:
         attrib 0: vec3 position
         attrib 1: vec3 normal
         attrib 2: vec2 uv
         attrib 3: vec4 tangent
       with stride = 12 floats. */
    glBindBuffer(GL_ARRAY_BUFFER, base_vbo);
    const GLsizei stride = 12 * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6*sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)(8*sizeof(float)));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, base_ebo);

    /* Per-instance matrix: 4 vec4 columns starting at attribute 4. */
    glGenBuffers(1, &im.instance_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, im.instance_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(float) * 16 * instance_count,
                 transforms,
                 GL_STATIC_DRAW);

    for (int i = 0; i < 4; ++i) {
        GLuint loc = 4 + i;
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE,
                              sizeof(float) * 16,
                              (void*)(sizeof(float) * 4 * i));
        glVertexAttribDivisor(loc, 1);
    }

    glBindVertexArray(0);
    return im;
}

void instanced_mesh_destroy(InstancedMesh* im) {
    if (im->instance_vbo) glDeleteBuffers(1, &im->instance_vbo);
    if (im->vao) glDeleteVertexArrays(1, &im->vao);
    im->instance_vbo = im->vao = 0;
}

void instanced_mesh_draw(InstancedMesh* im) {
    glBindVertexArray(im->vao);
    int use_16 = (im->index_count & 0x80000000) != 0;
    GLuint count = im->index_count & 0x7FFFFFFF;
    GLenum idx_type = use_16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
    glDrawElementsInstanced(GL_TRIANGLES, count, idx_type, 0, im->instance_count);
}

void instanced_mesh_update(InstancedMesh* im, const mat4* transforms, int count) {
    if (count > im->instance_count) count = im->instance_count;
    glBindBuffer(GL_ARRAY_BUFFER, im->instance_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    sizeof(float) * 16 * count, transforms);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}
