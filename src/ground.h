#ifndef GROUND_H
#define GROUND_H

#include "mesh.h"
#include "math3d.h"

/* Create a large ground plane mesh, positioned at y = y_level.
   Material is a mid-gray slightly glossy floor. */
Mesh ground_create(float y_level, float half_size);

#endif
