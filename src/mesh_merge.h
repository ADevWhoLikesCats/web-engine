#ifndef MESH_MERGE_H
#define MESH_MERGE_H

#include "mesh.h"

/* Merge all meshes in the model that share the same material signature.
   Vertices are transformed into world space (using each mesh's model matrix)
   so the resulting meshes use identity model transforms.
   Returns a new Model; caller should destroy both when done. */
Model model_merge(Model* src);

#endif
