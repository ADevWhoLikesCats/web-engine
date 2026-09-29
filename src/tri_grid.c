#include "tri_grid.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define GRID_X 32
#define GRID_Y 16
#define GRID_Z 32

static int cell_index(int x, int y, int z) {
    return x + GRID_X * (y + GRID_Y * z);
}

TriGrid tri_grid_build(const float* tri_positions,
                       const float* tri_albedos,
                       int count,
                       vec3 world_min, vec3 world_max)
{
    TriGrid tg = {0};
    tg.tri_count = count;
    tg.grid_x = GRID_X;
    tg.grid_y = GRID_Y;
    tg.grid_z = GRID_Z;
    tg.cell_count = GRID_X * GRID_Y * GRID_Z;
    tg.grid_min = world_min;
    tg.grid_max = world_max;

    vec3 span = v3_sub(world_max, world_min);
    if (span.x < 1e-4f) span.x = 1e-4f;
    if (span.y < 1e-4f) span.y = 1e-4f;
    if (span.z < 1e-4f) span.z = 1e-4f;

    printf("tri_grid: building %dx%dx%d grid over %d triangles\n",
           GRID_X, GRID_Y, GRID_Z, count);

    int* cell_counts = calloc(tg.cell_count, sizeof(int));
    int** tri_cells = calloc(count, sizeof(int*));
    int* tri_cell_count = calloc(count, sizeof(int));

    int total_entries = 0;
    for (int i = 0; i < count; ++i) {
        const float* p = tri_positions + i * 9;
        float minx = p[0], miny = p[1], minz = p[2];
        float maxx = p[0], maxy = p[1], maxz = p[2];
        for (int v = 1; v < 3; ++v) {
            if (p[v*3+0] < minx) minx = p[v*3+0];
            if (p[v*3+1] < miny) miny = p[v*3+1];
            if (p[v*3+2] < minz) minz = p[v*3+2];
            if (p[v*3+0] > maxx) maxx = p[v*3+0];
            if (p[v*3+1] > maxy) maxy = p[v*3+1];
            if (p[v*3+2] > maxz) maxz = p[v*3+2];
        }
        int cx0 = (int)((minx - world_min.x) / span.x * GRID_X);
        int cy0 = (int)((miny - world_min.y) / span.y * GRID_Y);
        int cz0 = (int)((minz - world_min.z) / span.z * GRID_Z);
        int cx1 = (int)((maxx - world_min.x) / span.x * GRID_X);
        int cy1 = (int)((maxy - world_min.y) / span.y * GRID_Y);
        int cz1 = (int)((maxz - world_min.z) / span.z * GRID_Z);
        if (cx0 < 0) cx0 = 0; if (cy0 < 0) cy0 = 0; if (cz0 < 0) cz0 = 0;
        if (cx1 >= GRID_X) cx1 = GRID_X-1;
        if (cy1 >= GRID_Y) cy1 = GRID_Y-1;
        if (cz1 >= GRID_Z) cz1 = GRID_Z-1;

        int ncells = (cx1-cx0+1) * (cy1-cy0+1) * (cz1-cz0+1);
        tri_cells[i] = malloc(sizeof(int) * ncells);
        int nc = 0;
        for (int cz = cz0; cz <= cz1; ++cz)
            for (int cy = cy0; cy <= cy1; ++cy)
                for (int cx = cx0; cx <= cx1; ++cx)
                    tri_cells[i][nc++] = cell_index(cx, cy, cz);
        tri_cell_count[i] = nc;
        for (int k = 0; k < nc; ++k) cell_counts[tri_cells[i][k]]++;
        total_entries += nc;
    }
    tg.total_entries = total_entries;
    printf("tri_grid: %d total cell entries (avg %.1f per triangle)\n",
           total_entries, (float)total_entries / (float)count);

    int* cell_start = malloc(sizeof(int) * tg.cell_count);
    int running = 0;
    for (int c = 0; c < tg.cell_count; ++c) {
        cell_start[c] = running;
        running += cell_counts[c];
    }

    unsigned int* indices = malloc(sizeof(unsigned int) * total_entries);
    int* fill_cursor = malloc(sizeof(int) * tg.cell_count);
    memcpy(fill_cursor, cell_start, sizeof(int) * tg.cell_count);

    for (int i = 0; i < count; ++i) {
        for (int k = 0; k < tri_cell_count[i]; ++k) {
            int c = tri_cells[i][k];
            indices[fill_cursor[c]++] = (unsigned int)i;
        }
        free(tri_cells[i]);
    }
    free(tri_cells);
    free(tri_cell_count);
    free(fill_cursor);

    /* Triangle texture */
    int tris_per_row = 128;
    int tex_w = tris_per_row * 3;
    int tex_h = (count + tris_per_row - 1) / tris_per_row;
    tg.tri_tex_w = tex_w;
    tg.tri_tex_h = tex_h;
    tg.tri_per_row = tris_per_row;

    float* tri_data = calloc((size_t)tex_w * tex_h * 4, sizeof(float));
    for (int i = 0; i < count; ++i) {
        const float* p = tri_positions + i * 9;
        const float* alb = tri_albedos + i * 3;
        int row = i / tris_per_row;
        int col = (i % tris_per_row) * 3;
        int base = (row * tex_w + col) * 4;
        tri_data[base+0]=p[0]; tri_data[base+1]=p[1]; tri_data[base+2]=p[2]; tri_data[base+3]=alb[0];
        tri_data[base+4]=p[3]; tri_data[base+5]=p[4]; tri_data[base+6]=p[5]; tri_data[base+7]=alb[1];
        tri_data[base+8]=p[6]; tri_data[base+9]=p[7]; tri_data[base+10]=p[8]; tri_data[base+11]=alb[2];
    }
    glGenTextures(1, &tg.tri_tex);
    glBindTexture(GL_TEXTURE_2D, tg.tri_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, tex_w, tex_h, 0,
                 GL_RGBA, GL_FLOAT, tri_data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    free(tri_data);

    /* Cell texture */
    int cell_tex_w = GRID_X * GRID_Y;
    int cell_tex_h = GRID_Z;
    float* cell_data = calloc((size_t)cell_tex_w * cell_tex_h * 4, sizeof(float));
    for (int z = 0; z < GRID_Z; ++z) {
        for (int y = 0; y < GRID_Y; ++y) {
            for (int x = 0; x < GRID_X; ++x) {
                int c = cell_index(x, y, z);
                int col = y * GRID_X + x;
                int base = (z * cell_tex_w + col) * 4;
                cell_data[base+0] = (float)cell_start[c];
                cell_data[base+1] = (float)cell_counts[c];
            }
        }
    }
    glGenTextures(1, &tg.cell_tex);
    glBindTexture(GL_TEXTURE_2D, tg.cell_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, cell_tex_w, cell_tex_h, 0,
                 GL_RGBA, GL_FLOAT, cell_data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    free(cell_data);

    /* Index texture (R32UI) */
    int idx_w = 4096;
    int idx_h = (total_entries + idx_w - 1) / idx_w;
    tg.idx_tex_w = idx_w;
    tg.idx_tex_h = idx_h;
    unsigned int* idx_padded = calloc((size_t)idx_w * idx_h, sizeof(unsigned int));
    memcpy(idx_padded, indices, sizeof(unsigned int) * total_entries);
    glGenTextures(1, &tg.idx_tex);
    glBindTexture(GL_TEXTURE_2D, tg.idx_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, idx_w, idx_h, 0,
                 GL_RED_INTEGER, GL_UNSIGNED_INT, idx_padded);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    free(idx_padded);

    free(cell_start);
    free(cell_counts);
    free(indices);

    printf("tri_grid: done (indices: %d, %dx%d)\n", total_entries, idx_w, idx_h);
    return tg;
}

void tri_grid_destroy(TriGrid* tg) {
    if (tg->tri_tex)  glDeleteTextures(1, &tg->tri_tex);
    if (tg->cell_tex) glDeleteTextures(1, &tg->cell_tex);
    if (tg->idx_tex)  glDeleteTextures(1, &tg->idx_tex);
    tg->tri_tex = tg->cell_tex = tg->idx_tex = 0;
}
