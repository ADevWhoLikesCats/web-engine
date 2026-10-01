#include "mesh.h"
#include "asset.h"
#include "texture.h"

#include "cgltf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Global CPU triangle data (defined here, declared extern in mesh.h) */
float* g_mesh_cpu_tris = NULL;
float* g_mesh_cpu_albedos = NULL;
int    g_mesh_cpu_tri_count = 0;


static Material default_material(void) {
    Material m = {0};
    m.tex_basecolor    = texture_solid(200, 200, 200, 255, 1);
    m.tex_normal       = texture_solid(128, 128, 255, 255, 0);
    m.tex_mr           = texture_solid(255, 200, 0, 255, 0);  /* G=rough 0.78, B=metal 0 */
    m.tex_emissive     = texture_solid(0, 0, 0, 255, 1);
    m.tex_occlusion    = texture_solid(255, 255, 255, 255, 0);
    m.basecolor_factor = v3(1.0f, 1.0f, 1.0f);
    m.metallic_factor  = 0.0f;
    m.roughness_factor = 1.0f;
    m.normal_scale     = 1.0f;
    m.occlusion_strength = 1.0f;
    m.emissive_strength = 1.0f;
    m.emissive_factor   = v3(0.0f, 0.0f, 0.0f);
    return m;
}

static GLuint load_texture_from_gltf(cgltf_texture_view* view, int srgb) {
    if (!view || !view->texture) return 0;
    cgltf_texture* tex = view->texture;
    if (!tex->image) return 0;

    const cgltf_buffer_view* bv = tex->image->buffer_view;
    if (!bv) {
        fprintf(stderr, "texture: image has no buffer_view (external URIs not supported)\n");
        return 0;
    }
    const unsigned char* base = (const unsigned char*)bv->buffer->data;
    if (!base) {
        fprintf(stderr, "texture: buffer data NULL\n");
        return 0;
    }
    const unsigned char* bytes = base + bv->offset + tex->image->buffer_view->offset * 0; /* offset handled below */
    bytes = base + bv->offset;

    return texture_from_memory(bytes, (int)bv->size, srgb);
}

static Material material_from_gltf(cgltf_material* gm) {
    Material m = default_material();
    if (!gm) return m;

    if (gm->has_pbr_metallic_roughness) {
        cgltf_pbr_metallic_roughness* p = &gm->pbr_metallic_roughness;

        m.basecolor_factor = v3(p->base_color_factor[0], p->base_color_factor[1], p->base_color_factor[2]);
        m.metallic_factor  = p->metallic_factor;
        m.roughness_factor = p->roughness_factor;

        GLuint bc = load_texture_from_gltf(&p->base_color_texture, 1);
        if (bc) { m.tex_basecolor = bc; m.has_basecolor = 1; }

        GLuint mr = load_texture_from_gltf(&p->metallic_roughness_texture, 0);
        if (mr) { m.tex_mr = mr; m.has_mr = 1; }
    }

    GLuint nm = load_texture_from_gltf(&gm->normal_texture, 0);
    if (nm) { m.tex_normal = nm; m.has_normal = 1; m.normal_scale = gm->normal_texture.scale; }

    /* Emissive factor — RGB multiplier for emissive texture, or the
       standalone emissive color if no texture is present. */
    m.emissive_factor = v3(gm->emissive_factor[0],
                           gm->emissive_factor[1],
                           gm->emissive_factor[2]);

    /* KHR_materials_emissive_strength — enables HDR emissive (>1.0).
       Headlights in glTF files typically use emissive_factor 1.0 with
       emissive_strength 5.0 or higher to get a bright glow. */
    if (gm->has_emissive_strength) {
        m.emissive_strength = gm->emissive_strength.emissive_strength;
    }

    /* Load the emissive texture if there is one. */
    GLuint em = load_texture_from_gltf(&gm->emissive_texture, 1);
    if (em) { m.tex_emissive = em; m.has_emissive = 1; }

    /* Mark as emissive if either the texture exists OR the factor is
       nonzero (or the strength was explicitly set above). */
    if (m.emissive_factor.x > 0.0f || m.emissive_factor.y > 0.0f ||
        m.emissive_factor.z > 0.0f || m.emissive_strength > 1.0f) {
        m.has_emissive = 1;
    }

    GLuint oc = load_texture_from_gltf(&gm->occlusion_texture, 0);
    if (oc) { m.tex_occlusion = oc; m.has_occlusion = 1; }

    m.double_sided = gm->double_sided ? 1 : 0;
    return m;
}

/* Read an accessor as float3 (or float2/4 depending on type) into a tightly packed buffer. */
static float* accessor_to_float(cgltf_accessor* acc, int components) {
    if (!acc) return NULL;
    float* out = malloc(sizeof(float) * acc->count * components);
    if (!out) return NULL;
    for (cgltf_size i = 0; i < acc->count; ++i) {
        cgltf_accessor_read_float(acc, i, out + i * components, components);
    }
    return out;
}

static void mat4_from_cgltf(cgltf_float src[16], mat4* out) {
    /* cgltf gives column-major, same as our mat4. */
    memcpy(out->m, src, sizeof(float) * 16);
}


typedef struct {
    GLuint tex_basecolor, tex_mr, tex_normal, tex_emissive, tex_occlusion;
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
    s.has_bc = m->has_basecolor; s.has_mr = m->has_mr; s.has_nm = m->has_normal;
    return s;
}

static int mat_equal(const MatSig* a, const MatSig* b) {
    if (a->tex_basecolor != b->tex_basecolor) return 0;
    if (a->tex_mr != b->tex_mr) return 0;
    if (a->tex_normal != b->tex_normal) return 0;
    if (a->has_bc != b->has_bc || a->has_mr != b->has_mr || a->has_nm != b->has_nm) return 0;
    for (int i = 0; i < 3; ++i) if (fabsf(a->basecolor[i] - b->basecolor[i]) > 1e-4f) return 0;
    if (fabsf(a->metallic - b->metallic) > 1e-4f) return 0;
    if (fabsf(a->roughness - b->roughness) > 1e-4f) return 0;
    return 1;
}

Model model_load_glb(const char* path) {
    Model model = {0};

    size_t file_len = 0;
    char* file_data = asset_load(path, &file_len);
    if (!file_data) {
        fprintf(stderr, "mesh: cannot load %s\n", path);
        return model;
    }

    cgltf_options opts = {0};
    cgltf_data* data = NULL;
    cgltf_result r = cgltf_parse(&opts, file_data, file_len, &data);
    if (r != cgltf_result_success) {
        fprintf(stderr, "mesh: cgltf_parse failed (%d)\n", r);
        free(file_data);
        return model;
    }

    r = cgltf_load_buffers(&opts, data, NULL);
    if (r != cgltf_result_success) {
        fprintf(stderr, "mesh: cgltf_load_buffers failed (%d)\n", r);
        cgltf_free(data);
        free(file_data);
        return model;
    }

    printf("mesh: loaded %s\n", path);
    printf("  meshes=%zu materials=%zu images=%zu nodes=%zu\n",
           data->meshes_count, data->materials_count, data->images_count, data->nodes_count);

    /* Count primitives */
    size_t total_prims = 0;
    for (size_t i = 0; i < data->meshes_count; ++i) {
        total_prims += data->meshes[i].primitives_count;
    }
    if (total_prims == 0) {
        fprintf(stderr, "mesh: no primitives found\n");
        cgltf_free(data);
        free(file_data);
        return model;
    }

    model.meshes = calloc(total_prims, sizeof(Mesh));
    model.min_bb = v3( 1e30f,  1e30f,  1e30f);
    model.max_bb = v3(-1e30f, -1e30f, -1e30f);

    int mesh_out = 0;

    /* ---- First pass: accumulate vertices/indices per material group ---- */

    /* ---- CPU-side triangle accumulation (for SG bake) ---- */
    static float* g_cpu_tris = NULL;
    static float* g_cpu_albedo = NULL;
    static int    g_cpu_tri_count = 0;
    static int    g_cpu_tri_cap = 0;
    #define CPU_TRI_ADD(px0,py0,pz0,px1,py1,pz1,px2,py2,pz2, ar,ag,ab) do { \
        if (g_cpu_tri_count >= g_cpu_tri_cap) { \
            g_cpu_tri_cap = g_cpu_tri_cap ? g_cpu_tri_cap * 2 : 4096; \
            g_cpu_tris = realloc(g_cpu_tris, sizeof(float) * g_cpu_tri_cap * 9); \
            g_cpu_albedo = realloc(g_cpu_albedo, sizeof(float) * g_cpu_tri_cap * 3); \
        } \
        float* tp = g_cpu_tris + g_cpu_tri_count * 9; \
        tp[0]=(px0); tp[1]=(py0); tp[2]=(pz0); \
        tp[3]=(px1); tp[4]=(py1); tp[5]=(pz1); \
        tp[6]=(px2); tp[7]=(py2); tp[8]=(pz2); \
        float* ap = g_cpu_albedo + g_cpu_tri_count * 3; \
        ap[0]=(ar); ap[1]=(ag); ap[2]=(ab); \
        g_cpu_tri_count++; \
    } while(0)


    /* We store raw vertex data in a growing buffer per material group */
    typedef struct {
        int         mat_index;
        float*      verts;      /* interleaved 12 floats per vertex */
        size_t      vert_count;
        size_t      vert_capacity;
        unsigned int* indices;
        size_t      index_count;
        size_t      index_capacity;
    } Group;

    Group* groups = NULL;
    int    group_count = 0;
    Material* group_mats = NULL;

    for (size_t mi = 0; mi < data->meshes_count; ++mi) {
        cgltf_mesh* gm = &data->meshes[mi];
        for (size_t pi = 0; pi < gm->primitives_count; ++pi) {
            cgltf_primitive* prim = &gm->primitives[pi];
            if (prim->type != cgltf_primitive_type_triangles) continue;

            cgltf_accessor *a_pos = NULL, *a_nrm = NULL, *a_uv = NULL, *a_tan = NULL;
            for (size_t ai = 0; ai < prim->attributes_count; ++ai) {
                cgltf_attribute* at = &prim->attributes[ai];
                if (at->type == cgltf_attribute_type_position)  a_pos = at->data;
                if (at->type == cgltf_attribute_type_normal)    a_nrm = at->data;
                if (at->type == cgltf_attribute_type_texcoord && at->index == 0) a_uv = at->data;
                if (at->type == cgltf_attribute_type_tangent)   a_tan = at->data;
            }
            if (!a_pos) continue;

            size_t vcount = a_pos->count;
            float* pos = accessor_to_float(a_pos, 3);
            float* nrm = a_nrm ? accessor_to_float(a_nrm, 3) : NULL;
            float* uv  = a_uv  ? accessor_to_float(a_uv, 2)  : NULL;
            float* tan = a_tan ? accessor_to_float(a_tan, 4) : NULL;

            /* Material for this primitive */
            Material pm = prim->material ? material_from_gltf(prim->material) : default_material();
            MatSig sig = mat_signature(&pm);

            /* Find existing group or create a new one */
            int g = -1;
            for (int k = 0; k < group_count; ++k) {
                if (mat_equal(&group_mats[k] == NULL ? NULL : &(MatSig){0}, &sig)) { g = k; break; }
            }
            /* Simpler: just check by material pointer identity + factor equality */
            g = -1;
            for (int k = 0; k < group_count; ++k) {
                Material* km = &group_mats[k];
                if (km->tex_basecolor == pm.tex_basecolor &&
                    km->tex_mr == pm.tex_mr &&
                    km->tex_normal == pm.tex_normal &&
                    km->has_basecolor == pm.has_basecolor &&
                    km->has_mr == pm.has_mr &&
                    km->has_normal == pm.has_normal &&
                    fabsf(km->basecolor_factor.x - pm.basecolor_factor.x) < 1e-4f &&
                    fabsf(km->basecolor_factor.y - pm.basecolor_factor.y) < 1e-4f &&
                    fabsf(km->basecolor_factor.z - pm.basecolor_factor.z) < 1e-4f &&
                    fabsf(km->metallic_factor - pm.metallic_factor) < 1e-4f &&
                    fabsf(km->roughness_factor - pm.roughness_factor) < 1e-4f) {
                    g = k; break;
                }
            }
            if (g < 0) {
                groups = realloc(groups, sizeof(Group) * (group_count + 1));
                group_mats = realloc(group_mats, sizeof(Material) * (group_count + 1));
                memset(&groups[group_count], 0, sizeof(Group));
                groups[group_count].mat_index = group_count;
                group_mats[group_count] = pm;
                g = group_count;
                group_count++;
            }

            /* Append vertices */
            size_t needed = groups[g].vert_count + vcount;
            if (needed > groups[g].vert_capacity) {
                size_t new_cap = needed * 2;
                groups[g].verts = realloc(groups[g].verts, sizeof(float) * 12 * new_cap);
                groups[g].vert_capacity = new_cap;
            }
            unsigned int base_vertex = (unsigned int)groups[g].vert_count;
            float* dst = groups[g].verts + groups[g].vert_count * 12;
            for (size_t v = 0; v < vcount; ++v) {
                dst[v*12+0] = pos[v*3+0];
                dst[v*12+1] = pos[v*3+1];
                dst[v*12+2] = pos[v*3+2];
                if (nrm) { dst[v*12+3] = nrm[v*3+0]; dst[v*12+4] = nrm[v*3+1]; dst[v*12+5] = nrm[v*3+2]; }
                else { dst[v*12+3]=0; dst[v*12+4]=0; dst[v*12+5]=1; }
                if (uv) { dst[v*12+6] = uv[v*2+0]; dst[v*12+7] = uv[v*2+1]; }
                else { dst[v*12+6]=0; dst[v*12+7]=0; }
                if (tan) { dst[v*12+8] = tan[v*4+0]; dst[v*12+9] = tan[v*4+1]; dst[v*12+10] = tan[v*4+2]; dst[v*12+11] = tan[v*4+3]; }
                else { dst[v*12+8]=1; dst[v*12+9]=0; dst[v*12+10]=0; dst[v*12+11]=1; }

                float x=dst[v*12+0], y=dst[v*12+1], z=dst[v*12+2];
                if (x < model.min_bb.x) model.min_bb.x = x;
                if (y < model.min_bb.y) model.min_bb.y = y;
                if (z < model.min_bb.z) model.min_bb.z = z;
                if (x > model.max_bb.x) model.max_bb.x = x;
                if (y > model.max_bb.y) model.max_bb.y = y;
                if (z > model.max_bb.z) model.max_bb.z = z;
            }
            groups[g].vert_count += vcount;

            /* Append indices */
            size_t icount = prim->indices ? prim->indices->count : vcount;
            if (groups[g].index_count + icount > groups[g].index_capacity) {
                size_t new_cap = (groups[g].index_count + icount) * 2;
                groups[g].indices = realloc(groups[g].indices, sizeof(unsigned int) * new_cap);
                groups[g].index_capacity = new_cap;
            }
            unsigned int* idst = groups[g].indices + groups[g].index_count;
            if (prim->indices) {
                for (size_t i = 0; i < icount; ++i) {
                    idst[i] = (unsigned int)cgltf_accessor_read_index(prim->indices, i) + base_vertex;
                }
            } else {
                for (size_t i = 0; i < icount; ++i) idst[i] = base_vertex + (unsigned int)i;
            }
            groups[g].index_count += icount;

            /* Emit triangles for the CPU-side bake data */
            {
                float ar = pm.basecolor_factor.x;
                float ag = pm.basecolor_factor.y;
                float ab = pm.basecolor_factor.z;
                size_t tri_n = icount / 3;
                for (size_t ti = 0; ti < tri_n; ++ti) {
                    unsigned int i0, i1, i2;
                    if (prim->indices) {
                        i0 = (unsigned int)cgltf_accessor_read_index(prim->indices, ti*3+0);
                        i1 = (unsigned int)cgltf_accessor_read_index(prim->indices, ti*3+1);
                        i2 = (unsigned int)cgltf_accessor_read_index(prim->indices, ti*3+2);
                    } else {
                        i0 = (unsigned int)(ti*3+0);
                        i1 = (unsigned int)(ti*3+1);
                        i2 = (unsigned int)(ti*3+2);
                    }
                    const float* vp = dst;
                    CPU_TRI_ADD(vp[i0*12+0], vp[i0*12+1], vp[i0*12+2],
                                vp[i1*12+0], vp[i1*12+1], vp[i1*12+2],
                                vp[i2*12+0], vp[i2*12+1], vp[i2*12+2],
                                ar, ag, ab);
                }
            }


            free(pos); free(nrm); free(uv); free(tan);
        }
    }

    /* ---- Second pass: create one VAO/VBO/EBO per material group ---- */
    model.meshes = calloc(group_count, sizeof(Mesh));
    model.count = group_count;
    for (int g = 0; g < group_count; ++g) {
        Mesh* out = &model.meshes[g];
        out->material = group_mats[g];
        out->model = m4_identity();

        unsigned int max_idx = 0;
        for (size_t i = 0; i < groups[g].index_count; ++i) {
            if (groups[g].indices[i] > max_idx) max_idx = groups[g].indices[i];
        }
        int use_16 = (max_idx < 65535);

        glGenVertexArrays(1, &out->vao);
        glBindVertexArray(out->vao);

        glGenBuffers(1, &out->vbo);
        glBindBuffer(GL_ARRAY_BUFFER, out->vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 12 * groups[g].vert_count,
                     groups[g].verts, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12*sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 12*sizeof(float), (void*)(3*sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 12*sizeof(float), (void*)(6*sizeof(float)));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 12*sizeof(float), (void*)(8*sizeof(float)));

        glGenBuffers(1, &out->ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, out->ebo);
        if (use_16) {
            unsigned short* idx16 = malloc(sizeof(unsigned short) * groups[g].index_count);
            for (size_t i = 0; i < groups[g].index_count; ++i) idx16[i] = (unsigned short)groups[g].indices[i];
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned short) * groups[g].index_count, idx16, GL_STATIC_DRAW);
            free(idx16);
            out->index_count = (GLuint)groups[g].index_count | 0x80000000;
        } else {
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * groups[g].index_count,
                         groups[g].indices, GL_STATIC_DRAW);
            out->index_count = (GLuint)groups[g].index_count;
        }

        glBindVertexArray(0);

        printf("  merged[%d]: verts=%zu indices=%zu (%s) bc=%d mr=%d nm=%d\n",
            g, groups[g].vert_count, groups[g].index_count, use_16 ? "u16" : "u32",
               out->material.has_basecolor, out->material.has_mr, out->material.has_normal);

        free(groups[g].verts);
        free(groups[g].indices);
    }
    free(groups);
    free(group_mats);

    model.count = group_count;
    printf("mesh: %d primitives, bb min=(%.2f %.2f %.2f) max=(%.2f %.2f %.2f)\n",
           model.count,
           model.min_bb.x, model.min_bb.y, model.min_bb.z,
           model.max_bb.x, model.max_bb.y, model.max_bb.z);

    g_mesh_cpu_tris = g_cpu_tris;
    g_mesh_cpu_albedos = g_cpu_albedo;
    g_mesh_cpu_tri_count = g_cpu_tri_count;
    printf("mesh: %d CPU triangles accumulated for SG bake\n", g_cpu_tri_count);

    cgltf_free(data);
    free(file_data);
    return model;
}

void model_destroy(Model* m) {
    if (!m->meshes) return;
    for (int i = 0; i < m->count; ++i) {
        Mesh* mesh = &m->meshes[i];
        if (mesh->vao) glDeleteVertexArrays(1, &mesh->vao);
        if (mesh->vbo) glDeleteBuffers(1, &mesh->vbo);
        if (mesh->ebo) glDeleteBuffers(1, &mesh->ebo);
    }
    free(m->meshes);
    m->meshes = NULL;
    m->count = 0;
}

vec3 model_center(Model* m) {
    return v3(
        (m->min_bb.x + m->max_bb.x) * 0.5f,
        (m->min_bb.y + m->max_bb.y) * 0.5f,
        (m->min_bb.z + m->max_bb.z) * 0.5f
    );
}

vec3 model_extent(Model* m) {
    return v3(
        m->max_bb.x - m->min_bb.x,
        m->max_bb.y - m->min_bb.y,
        m->max_bb.z - m->min_bb.z
    );
}
