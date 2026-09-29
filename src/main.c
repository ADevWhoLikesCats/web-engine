#include <GLES3/gl3.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include "asset.h"
#include "math3d.h"
#include "framebuffer.h"
#include "pass.h"
#include "capture.h"
#include "sh.h"
#include "sg.h"
#include "mesh.h"
#include "ground.h"
#include "shadow.h"
#include "probe_grid.h"
#include "tri_grid.h"
#include "bloom.h"
#include "hdr_env.h"

#define CAR_PATH "/assets/car.glb"

static GLuint prog;
static GLint  u_model, u_viewproj, u_normalmat;
static GLint  u_ssrcolor, u_ssrdepth, u_viewproj_mat, u_resolution;
static GLint  u_campos, u_lightdir, u_lightcolor, u_lightintensity;
static GLint  u_debugmode;
static GLint  u_sh, u_sg, u_sgcount;
static GLint  u_tex_basecolor, u_tex_mr, u_tex_normal, u_tex_emissive, u_tex_occlusion;
static GLint  u_basecolor_factor, u_metallic_factor, u_roughness_factor, u_normal_scale;
static GLint  u_has_basecolor, u_has_mr, u_has_normal;
static GLint  u_lightvp, u_shadowmap, u_shadowenabled;
static GLint  u_probes, u_scenegridmin, u_scenegridmax;

static Framebuffer g_scene_fb;
static Framebuffer g_sh_recon_fb;
static Framebuffer g_ssr_fb;
static Framebuffer g_taa_history[2];    /* ping-pong */
static Framebuffer g_dof_fb;
static Pass        g_dof_pass;
static float       g_focus_distance = 4.0f;
static float       g_focus_range = 6.0f;
static float       g_max_blur_radius = 12.0f;
static int         g_dof_enabled = 1;
static int         g_taa_ping = 0;
static Pass        g_taa_pass;
static mat4        g_prev_viewproj;
static int         g_frame_index = 0;
static int         g_taa_enabled = 1;
static Pass        g_ssr_pass;
static Pass        g_ssr_composite_pass;
static HDREnv      g_hdr_env;
#ifdef __cplusplus
extern "C" {
#endif
GLuint g_hdr_tex_global = 0;
int    g_hdr_valid_global = 0;
#ifdef __cplusplus
}
#endif
static Pass         g_sky_pass;
static Bloom       g_bloom;
static Pass        g_bloom_composite_pass;
static Pass        g_bloom_debug_pass;
static Framebuffer g_composite_fb;
static Pass        g_blit_pass;
static Pass        g_octa_debug_pass;
static Capture     g_capture;
static SH          g_sh;
static SG          g_sg;

static Model       g_car;
static ShadowMap   g_shadow;
static ProbeGrid   g_probes;
static TriGrid     g_tri_grid;
static vec3        g_car_grid_min, g_car_grid_max;
static Mesh        g_ground;
static mat4        g_car_model;

static float g_roughness_scale = 1.0f;
static int g_debug_mode = 0;
static int g_screen_w = 0, g_screen_h = 0;

/* Halton low-discrepancy sequence for TAA jitter */
static float halton_seq(int index, int base) {
    float f = 1.0f;
    float r = 0.0f;
    int i = index;
    while (i > 0) {
        f /= (float)base;
        r += f * (float)(i % base);
        i /= base;
    }
    return r;
}

/* 8-sample Halton(2,3) sequence, offset by 0.5 for [-0.5, 0.5] range */
static void taa_jitter_offset(int frame, float* out_x, float* out_y) {
    int idx = (frame % 8) + 1;
    *out_x = halton_seq(idx, 2) - 0.5f;
    *out_y = halton_seq(idx, 3) - 0.5f;
}

static GLuint compile(GLenum type, const char* src, const char* path) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(s, sizeof(log), NULL, log);
        fprintf(stderr, "shader compile failed [%s]:\n%s\n", path, log);
        abort();
    }
    return s;
}
static GLuint load_shader(GLenum type, const char* path) {
    size_t len;
    char* src = asset_load(path, &len);
    if (!src) { fprintf(stderr, "missing shader: %s\n", path); abort(); }
    GLuint s = compile(type, src, path);
    free(src);
    return s;
}
static GLuint link(GLuint vs, GLuint fs) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetProgramInfoLog(p, sizeof(log), NULL, log);
        fprintf(stderr, "link failed:\n%s\n", log);
        abort();
    }
    return p;
}

static EM_BOOL on_key(int event_type, const EmscriptenKeyboardEvent* e, void* user) {
    (void)user;
    if (event_type != EMSCRIPTEN_EVENT_KEYDOWN) return EM_FALSE;
    if (e->key[0] == 'm' || e->key[0] == 'M') {
        g_debug_mode = (g_debug_mode + 1) % 14;
        printf("debug mode -> %d\n", g_debug_mode);
    }
    if (e->key[0] == 't' || e->key[0] == 'T') {
        g_taa_enabled = !g_taa_enabled;
        printf("TAA -> %s\n", g_taa_enabled ? "on" : "off");
    }
    if (e->key[0] == 'o' || e->key[0] == 'O') {
        g_dof_enabled = !g_dof_enabled;
        printf("DOF -> %s\n", g_dof_enabled ? "on" : "off");
    }
    if (e->key[0] == '-') {
        g_focus_distance -= 0.5f;
        if (g_focus_distance < 0.5f) g_focus_distance = 0.5f;
        printf("focus distance -> %.2f\n", g_focus_distance);
    }
    if (e->key[0] == '=') {
        g_focus_distance += 0.5f;
        printf("focus distance -> %.2f\n", g_focus_distance);
    }
    if (e->key[0] == ';') {
        g_max_blur_radius -= 1.0f;
        if (g_max_blur_radius < 1.0f) g_max_blur_radius = 1.0f;
        printf("blur radius -> %.1f\n", g_max_blur_radius);
    }
    if (e->key[0] == 0x27) {
        g_max_blur_radius += 1.0f;
        if (g_max_blur_radius > 40.0f) g_max_blur_radius = 40.0f;
        printf("blur radius -> %.1f\n", g_max_blur_radius);
    }
    if (e->key[0] == '[') {
        g_roughness_scale *= 0.5f;
        if (g_roughness_scale < 0.01f) g_roughness_scale = 0.01f;
        printf("roughness scale -> %.4f\n", g_roughness_scale);
    }
    if (e->key[0] == ']') {
        g_roughness_scale *= 2.0f;
        if (g_roughness_scale > 10.0f) g_roughness_scale = 10.0f;
        printf("roughness scale -> %.4f\n", g_roughness_scale);
    }
    return EM_TRUE;
}

static void create_scene_fb(int w, int h) {
    if (g_scene_fb.fbo) fb_destroy(&g_scene_fb);
    g_scene_fb = fb_create_with_depth_tex(w, h, FB_RGBA16F);
}

static void sync_canvas_size(void) {
    int css_w = EM_ASM_INT({ return Module.canvas.clientWidth; });
    int css_h = EM_ASM_INT({ return Module.canvas.clientHeight; });
    if (css_w < 1) css_w = 640;
    if (css_h < 1) css_h = 480;
    int buf_w = 0, buf_h = 0;
    emscripten_get_canvas_element_size("#canvas", &buf_w, &buf_h);
    if (css_w != buf_w || css_h != buf_h) {
        emscripten_set_canvas_element_size("#canvas", css_w, css_h);
    }
    if (css_w != g_screen_w || css_h != g_screen_h) {
        g_screen_w = css_w;
        g_screen_h = css_h;
        create_scene_fb(css_w, css_h);
        if (g_ssr_fb.fbo) fb_destroy(&g_ssr_fb);
        g_ssr_fb = fb_create(css_w, css_h, FB_RGBA16F);
        if (g_composite_fb.fbo) fb_destroy(&g_composite_fb);
        g_composite_fb = fb_create(css_w, css_h, FB_RGBA16F);
        if (g_taa_history[0].fbo) fb_destroy(&g_taa_history[0]);
        if (g_taa_history[1].fbo) fb_destroy(&g_taa_history[1]);
        g_taa_history[0] = fb_create(css_w, css_h, FB_RGBA16F);
        g_taa_history[1] = fb_create(css_w, css_h, FB_RGBA16F);
        if (g_dof_fb.fbo) fb_destroy(&g_dof_fb);
        g_dof_fb = fb_create(css_w, css_h, FB_RGBA16F);
        if (g_bloom.mips[0].fbo) bloom_resize(&g_bloom, css_w, css_h);
    }
}

static void init(void) {
    GLuint vs = load_shader(GL_VERTEX_SHADER,   "/shaders/pbr.vert");
    GLuint fs = load_shader(GL_FRAGMENT_SHADER, "/shaders/pbr.frag");
    prog = link(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    u_model          = glGetUniformLocation(prog, "uModel");
    u_viewproj       = glGetUniformLocation(prog, "uViewProj");
    u_normalmat      = glGetUniformLocation(prog, "uNormalMat");
    u_campos         = glGetUniformLocation(prog, "uCamPos");
    u_lightdir       = glGetUniformLocation(prog, "uLightDir");
    u_lightcolor     = glGetUniformLocation(prog, "uLightColor");
    u_lightintensity = glGetUniformLocation(prog, "uLightIntensity");
    u_debugmode      = glGetUniformLocation(prog, "uDebugMode");
    u_sh             = glGetUniformLocation(prog, "uSH");
    u_sg             = glGetUniformLocation(prog, "uSG");
    u_sgcount        = glGetUniformLocation(prog, "uSGCount");
    u_tex_basecolor  = glGetUniformLocation(prog, "uTexBasecolor");
    u_tex_mr         = glGetUniformLocation(prog, "uTexMR");
    u_tex_normal     = glGetUniformLocation(prog, "uTexNormal");
    u_tex_emissive   = glGetUniformLocation(prog, "uTexEmissive");
    u_tex_occlusion  = glGetUniformLocation(prog, "uTexOcclusion");
    u_basecolor_factor = glGetUniformLocation(prog, "uBasecolorFactor");
    u_metallic_factor  = glGetUniformLocation(prog, "uMetallicFactor");
    u_roughness_factor = glGetUniformLocation(prog, "uRoughnessFactor");
    u_normal_scale     = glGetUniformLocation(prog, "uNormalScale");
    u_has_basecolor    = glGetUniformLocation(prog, "uHasBasecolor");
    u_has_mr           = glGetUniformLocation(prog, "uHasMR");
    u_has_normal       = glGetUniformLocation(prog, "uHasNormal");
    u_lightvp          = glGetUniformLocation(prog, "uLightVP");
    u_shadowmap        = glGetUniformLocation(prog, "uShadowMap");
    u_shadowenabled    = glGetUniformLocation(prog, "uShadowEnabled");
    u_probes           = glGetUniformLocation(prog, "uProbeSGs");
    u_scenegridmin     = glGetUniformLocation(prog, "uSceneGridMin");
    u_scenegridmax     = glGetUniformLocation(prog, "uSceneGridMax");
    u_ssrcolor         = glGetUniformLocation(prog, "uSSRColor");
    u_ssrdepth         = glGetUniformLocation(prog, "uSSRDepth");
    u_viewproj_mat     = glGetUniformLocation(prog, "uViewProj");
    u_resolution       = glGetUniformLocation(prog, "uResolution");

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    g_blit_pass       = pass_create("/shaders/fullscreen.vert", "/shaders/blit_tonemap.frag");
    g_octa_debug_pass = pass_create("/shaders/fullscreen.vert", "/shaders/octa_debug.frag");
    g_capture         = capture_create(128);
    g_sh              = sh_create();
    g_sg              = sg_create();
    g_sh_recon_fb     = fb_create(200, 200, FB_RGBA16F);
    g_ssr_pass        = pass_create("/shaders/fullscreen.vert", "/shaders/ssr.frag");
    g_ssr_composite_pass = pass_create("/shaders/fullscreen.vert", "/shaders/ssr_composite.frag");
    g_bloom_composite_pass = pass_create("/shaders/fullscreen.vert", "/shaders/bloom_composite.frag");
    g_bloom_debug_pass = pass_create("/shaders/fullscreen.vert", "/shaders/bloom_debug.frag");
    g_taa_pass = pass_create("/shaders/fullscreen.vert", "/shaders/taa_resolve.frag");
    g_dof_pass = pass_create("/shaders/fullscreen.vert", "/shaders/dof.frag");

    printf("Loading %s...\n", CAR_PATH);
    g_car = model_load_glb(CAR_PATH);

    /* Auto-center and scale the car so its longest dimension is ~3 units. */
    vec3 c = model_center(&g_car);
    vec3 e = model_extent(&g_car);
    float longest = e.x; if (e.y > longest) longest = e.y; if (e.z > longest) longest = e.z;
    float scale = (longest > 1e-6f) ? (3.0f / longest) : 1.0f;

    /* Scale, then translate so the model's center is at origin */
    mat4 S = m4_scale(v3(scale, scale, scale));
    mat4 T = m4_translate(v3(-c.x * scale, -c.y * scale, -c.z * scale));
    g_car_model = m4_mul(T, S);

    /* Apply the world transform to each mesh by baking it into the model matrix */
    for (int i = 0; i < g_car.count; ++i) {
        g_car.meshes[i].model = g_car_model;
    }

    /* Ground plane at the car's bottom Y */
    float ground_y = g_car.min_bb.y * scale - c.y * scale - 0.02f;
    g_ground = ground_create(ground_y, 15.0f);

    g_shadow = shadow_create(2048);
    g_hdr_env = hdr_env_load("/assets/sky.hdr");
    g_sky_pass = pass_create("/shaders/fullscreen.vert", "/shaders/sky_background.frag");

    /* Compute scene bounds for the probe grid */
    g_probes = probe_grid_create(v3(-6.0f, -1.0f, -6.0f), v3(6.0f, 5.0f, 6.0f));

    /* Build the car triangle grid for ray tracing */
    {
        extern float* g_mesh_cpu_tris;
        extern float* g_mesh_cpu_albedos;
        extern int    g_mesh_cpu_tri_count;

        /* Car bounds in world space: after auto-scale, the car is ~3 units long.
           Use the merged model bounds directly. */
        vec3 bb_min = g_car.min_bb;
        vec3 bb_max = g_car.max_bb;

        /* Compute auto-scale factor (same as in init) */
        vec3 ext0 = v3_sub(bb_max, bb_min);
        float longest0 = ext0.x;
        if (ext0.y > longest0) longest0 = ext0.y;
        if (ext0.z > longest0) longest0 = ext0.z;
        float sc = (longest0 > 1e-6f) ? (3.0f / longest0) : 1.0f;

        /* Apply the same world transform to each CPU triangle */
        int tri_n = g_mesh_cpu_tri_count;
        float* world_tris = malloc(sizeof(float) * 9 * tri_n);
        float* world_albs = malloc(sizeof(float) * 3 * tri_n);
        vec3 center = v3((bb_min.x + bb_max.x) * 0.5f,
                         (bb_min.y + bb_max.y) * 0.5f,
                         (bb_min.z + bb_max.z) * 0.5f);
        for (int i = 0; i < tri_n; ++i) {
            for (int v = 0; v < 3; ++v) {
                float x = g_mesh_cpu_tris[i*9 + v*3 + 0];
                float y = g_mesh_cpu_tris[i*9 + v*3 + 1];
                float z = g_mesh_cpu_tris[i*9 + v*3 + 2];
                world_tris[i*9 + v*3 + 0] = (x - center.x) * sc;
                world_tris[i*9 + v*3 + 1] = (y - center.y) * sc;
                world_tris[i*9 + v*3 + 2] = (z - center.z) * sc;
            }
            world_albs[i*3+0] = g_mesh_cpu_albedos[i*3+0];
            world_albs[i*3+1] = g_mesh_cpu_albedos[i*3+1];
            world_albs[i*3+2] = g_mesh_cpu_albedos[i*3+2];
        }

        /* Compute the world-space AABB of the transformed car */
        vec3 cmin = v3( 1e30f,  1e30f,  1e30f);
        vec3 cmax = v3(-1e30f, -1e30f, -1e30f);
        for (int i = 0; i < tri_n * 3; ++i) {
            float x = world_tris[i*3+0], y = world_tris[i*3+1], z = world_tris[i*3+2];
            if (x < cmin.x) cmin.x = x;
            if (y < cmin.y) cmin.y = y;
            if (z < cmin.z) cmin.z = z;
            if (x > cmax.x) cmax.x = x;
            if (y > cmax.y) cmax.y = y;
            if (z > cmax.z) cmax.z = z;
        }
        /* Pad the AABB a bit */
        cmin = v3_sub(cmin, v3(0.1f, 0.1f, 0.1f));
        cmax = v3_add(cmax, v3(0.1f, 0.1f, 0.1f));
        g_car_grid_min = cmin;
        g_car_grid_max = cmax;
        printf("car grid bounds: (%.2f %.2f %.2f) to (%.2f %.2f %.2f)\n",
            
               cmin.x, cmin.y, cmin.z, cmax.x, cmax.y, cmax.z);

        double _t_build = emscripten_get_now();
        g_tri_grid = tri_grid_build(world_tris, world_albs, tri_n, cmin, cmax);
        printf("[bake] tri_grid build: %.0f ms\n", emscripten_get_now() - _t_build);

        free(world_tris);
        free(world_albs);

        /* Bake SGs using the triangle grid */
        vec3 ground_albedo = v3(0.35f, 0.36f, 0.38f);
        vec3 sky_horizon = v3(0.45f, 0.55f, 0.7f);
        vec3 sky_zenith  = v3(0.15f, 0.25f, 0.5f);
        vec3 light_dir_bake = v3_norm(v3(-1.5f, -0.6f, -0.3f));
        vec3 light_color = v3(1.0f, 0.98f, 0.95f);

        double _t_bake = emscripten_get_now();
        extern GLuint g_hdr_tex_global;
        extern int g_hdr_valid_global;
        g_hdr_tex_global = g_hdr_env.tex;
        g_hdr_valid_global = g_hdr_env.valid;

        probe_grid_bake_with_tris(&g_probes, &g_tri_grid,
                                  g_car_grid_min, g_car_grid_max,
                                  ground_y, ground_albedo,
                                  sky_horizon, sky_zenith,
                                  light_dir_bake, light_color, 3.0f);
        printf("[bake] SG bake pass: %.0f ms\n", emscripten_get_now() - _t_bake);
    }

    sync_canvas_size();

    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, EM_TRUE, on_key);
    printf("Press M for debug modes. Press [ / ] to change roughness scale.\n");
}

static void draw_car_into_fb(int w, int h, vec3 cam) {
    vec3 target = v3(0.0f, 0.0f, 0.0f);
    vec3 up     = v3(0.0f, 1.0f, 0.0f);
    mat4 view = m4_look_at(cam, target, up);
    mat4 proj = m4_perspective(50.0f * 3.14159265f / 180.0f,
                               (float)w / (float)h, 0.1f, 200.0f);
    if (g_taa_enabled) {
        float jx, jy;
        taa_jitter_offset(g_frame_index, &jx, &jy);
        proj.m[8] += jx * 2.0f / (float)w;
        proj.m[9] += jy * 2.0f / (float)h;
    }
    mat4 viewproj = m4_mul(proj, view);

    glUseProgram(prog);
    glUniformMatrix4fv(u_viewproj, 1, GL_FALSE, viewproj.m);
    glUniform3f(u_campos, cam.x, cam.y, cam.z);

    glUniformMatrix4fv(u_lightvp, 1, GL_FALSE, g_shadow.light_vp.m);
    glActiveTexture(GL_TEXTURE8);
    glBindTexture(GL_TEXTURE_2D, g_shadow.depth_tex);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(u_shadowmap, 8);
    glUniform1i(u_shadowenabled, 1);

    glUniform3f(u_scenegridmin, g_probes.grid_min.x, g_probes.grid_min.y, g_probes.grid_min.z);
    glUniform3f(u_scenegridmax, g_probes.grid_max.x, g_probes.grid_max.y, g_probes.grid_max.z);

    glActiveTexture(GL_TEXTURE9);
    glBindTexture(GL_TEXTURE_2D, g_probes.fb.color);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(u_probes, 9);
    glActiveTexture(GL_TEXTURE10);
    glBindTexture(GL_TEXTURE_2D, g_ssr_fb.color);
    glActiveTexture(GL_TEXTURE11);
    glBindTexture(GL_TEXTURE_2D, g_scene_fb.depth);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(u_ssrcolor, 10);
    glUniform1i(u_ssrdepth, 11);
    {
        vec2 res = { (float)g_scene_fb.width, (float)g_scene_fb.height };
        glUniform2f(u_resolution, res.x, res.y);
        glUniformMatrix4fv(u_viewproj_mat, 1, GL_FALSE, viewproj.m);
    }

    vec3 light_dir = v3_norm(v3(-0.4f, -1.0f, -0.5f));
    glUniform3f(u_lightdir, light_dir.x, light_dir.y, light_dir.z);
    glUniform3f(u_lightcolor, 1.0f, 0.98f, 0.95f);
    glUniform1f(u_lightintensity, 3.0f);
    glUniform1i(u_debugmode, g_debug_mode);

    /* SH / SG */
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, g_sh.fb.color);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, g_sg.fb.color);
    glUniform1i(u_sh, 1);
    glUniform1i(u_sg, 2);
    glUniform1i(u_sgcount, SG_COUNT);

    /* Texture units for material maps */
    glUniform1i(u_tex_basecolor, 3);
    glUniform1i(u_tex_mr,        4);
    glUniform1i(u_tex_normal,    5);
    glUniform1i(u_tex_emissive,  6);
    glUniform1i(u_tex_occlusion, 7);

    for (int i = 0; i < g_car.count; ++i) {
        Mesh* m = &g_car.meshes[i];

        float nrm[9];
        m4_to_mat3_normal(m->model, nrm);
        glUniformMatrix4fv(u_model, 1, GL_FALSE, m->model.m);
        glUniformMatrix3fv(u_normalmat, 1, GL_FALSE, nrm);

        glUniform3f(u_basecolor_factor, m->material.basecolor_factor.x,
                                        m->material.basecolor_factor.y,
                                        m->material.basecolor_factor.z);
        glUniform1f(u_metallic_factor,  m->material.metallic_factor);
        glUniform1f(u_roughness_factor, m->material.roughness_factor * g_roughness_scale);
        glUniform1f(u_normal_scale,     m->material.normal_scale);
        glUniform1i(u_has_basecolor,    m->material.has_basecolor);
        glUniform1i(u_has_mr,           m->material.has_mr);
        glUniform1i(u_has_normal,       m->material.has_normal);

        glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, m->material.tex_basecolor);
        glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, m->material.tex_mr);
        glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D, m->material.tex_normal);
        glActiveTexture(GL_TEXTURE6); glBindTexture(GL_TEXTURE_2D, m->material.tex_emissive);
        glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D, m->material.tex_occlusion);
        glActiveTexture(GL_TEXTURE0);

        if (m->material.double_sided) glDisable(GL_CULL_FACE);
        else glEnable(GL_CULL_FACE);

        glBindVertexArray(m->vao);
        int use_16 = (m->index_count & 0x80000000) != 0;
        GLuint count = m->index_count & 0x7FFFFFFF;
        GLenum idx_type = use_16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
        glDrawElements(GL_TRIANGLES, count, idx_type, 0);
    }

    /* Ground plane (uses same shader) */
    {
        Mesh* gm = &g_ground;
        float nrm[9];
        m4_to_mat3_normal(gm->model, nrm);
        glUniformMatrix4fv(u_model, 1, GL_FALSE, gm->model.m);
        glUniformMatrix3fv(u_normalmat, 1, GL_FALSE, nrm);

        glUniform3f(u_basecolor_factor,
                    gm->material.basecolor_factor.x,
                    gm->material.basecolor_factor.y,
                    gm->material.basecolor_factor.z);
        glUniform1f(u_metallic_factor,  gm->material.metallic_factor);
        glUniform1f(u_roughness_factor, gm->material.roughness_factor * g_roughness_scale);
        glUniform1f(u_normal_scale,     gm->material.normal_scale);
        glUniform1i(u_has_basecolor,    gm->material.has_basecolor);
        glUniform1i(u_has_mr,           gm->material.has_mr);
        glUniform1i(u_has_normal,       gm->material.has_normal);

        glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, gm->material.tex_basecolor);
        glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, gm->material.tex_mr);
        glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D, gm->material.tex_normal);
        glActiveTexture(GL_TEXTURE6); glBindTexture(GL_TEXTURE_2D, gm->material.tex_emissive);
        glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D, gm->material.tex_occlusion);
        glActiveTexture(GL_TEXTURE0);

        glDisable(GL_CULL_FACE);   /* plane is single-sided; be lenient */
        glBindVertexArray(gm->vao);
        int use_16 = (gm->index_count & 0x80000000) != 0;
        GLuint count = gm->index_count & 0x7FFFFFFF;
        glDrawElements(GL_TRIANGLES, count, use_16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, 0);
        glEnable(GL_CULL_FACE);
    }

    glEnable(GL_CULL_FACE);
}

static void check_gl_errors(const char* label) {
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        printf("[GL error after %s] 0x%04X\n", label, err);
        while (glGetError() != GL_NO_ERROR) {}
    }
}

static void frame(void) {
    sync_canvas_size();

    double t = emscripten_get_now() * 0.001;
    /* Orbit camera */
    float ang = (float)t * 0.3f;
    vec3 cam = v3(sinf(ang) * 4.0f, 1.5f, cosf(ang) * 4.0f);
    vec3 light_dir = v3_norm(v3(-0.4f, -1.0f, -0.5f));
    vec3 light_color = v3(1.0f, 0.98f, 0.95f);
    float light_intensity = 3.0f;

    /* Capture */
    capture_run(&g_capture,
                v3(0.0f, 1.0f, 0.0f),
                light_dir, light_color, light_intensity, cam);

    /* SH + SG */
    sh_project(&g_sh, &g_capture.fb);
    /*     sg_fit(&g_sg, &g_capture.fb); (disabled — replaced by tri_grid bake) */

    /* SH reconstruct for debug */
    fb_bind(&g_sh_recon_fb);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    sh_reconstruct(&g_sh, g_sh_recon_fb.width, g_sh_recon_fb.height, 1.0f);

    /* Sky background pass */
    if (g_hdr_env.valid && g_sky_pass.prog) {
        fb_bind(&g_scene_fb);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        pass_use(&g_sky_pass);
    check_gl_errors("after pass_use");
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_hdr_env.tex);
        pass_set_i32(&g_sky_pass, "uHDRI", 0);

        vec3 sky_target = v3(0,0,0);
        vec3 sky_up = v3(0,1,0);
        mat4 sky_view = m4_look_at(cam, sky_target, sky_up);
        mat4 sky_proj = m4_perspective(50.0f * 3.14159265f / 180.0f,
                                       (float)g_scene_fb.width / (float)g_scene_fb.height,
                                       0.1f, 200.0f);
        mat4 sky_vp = m4_mul(sky_proj, sky_view);

        float sky_inv_vp[16];
        {
            const float* m = sky_vp.m;
            float inv[16];
            inv[0] =  m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15] + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
            inv[4] = -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15] - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
            inv[8] =  m[4]*m[9]*m[15] - m[4]*m[11]*m[13] - m[8]*m[5]*m[15] + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
            inv[12] = -m[4]*m[9]*m[14] + m[4]*m[10]*m[13] + m[8]*m[5]*m[14] - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];
            inv[1] = -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15] - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
            inv[5] =  m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15] + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
            inv[9] = -m[0]*m[9]*m[15] + m[0]*m[11]*m[13] + m[8]*m[1]*m[15] - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
            inv[13] = m[0]*m[9]*m[14] - m[0]*m[10]*m[13] - m[8]*m[1]*m[14] + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];
            inv[2] =  m[1]*m[6]*m[15] - m[1]*m[7]*m[14] - m[5]*m[2]*m[15] + m[5]*m[3]*m[14] + m[13]*m[2]*m[7] - m[13]*m[3]*m[6];
            inv[6] = -m[0]*m[6]*m[15] + m[0]*m[7]*m[14] + m[4]*m[2]*m[15] - m[4]*m[3]*m[14] - m[12]*m[2]*m[7] + m[12]*m[3]*m[6];
            inv[10] = m[0]*m[5]*m[15] - m[0]*m[7]*m[13] - m[4]*m[1]*m[15] + m[4]*m[3]*m[13] + m[12]*m[1]*m[7] - m[12]*m[3]*m[5];
            inv[14] = -m[0]*m[5]*m[14] + m[0]*m[6]*m[13] + m[4]*m[1]*m[14] - m[4]*m[2]*m[13] - m[12]*m[1]*m[6] + m[12]*m[2]*m[5];
            inv[3] = -m[1]*m[6]*m[11] + m[1]*m[7]*m[10] + m[5]*m[2]*m[11] - m[5]*m[3]*m[10] - m[9]*m[2]*m[7] + m[9]*m[3]*m[6];
            inv[7] =  m[0]*m[6]*m[11] - m[0]*m[7]*m[10] - m[4]*m[2]*m[11] + m[4]*m[3]*m[10] + m[8]*m[2]*m[7] - m[8]*m[3]*m[6];
            inv[11] = -m[0]*m[5]*m[11] + m[0]*m[7]*m[9] + m[4]*m[1]*m[11] - m[4]*m[3]*m[9] - m[8]*m[1]*m[7] + m[8]*m[3]*m[5];
            inv[15] = m[0]*m[5]*m[10] - m[0]*m[6]*m[9] - m[4]*m[1]*m[10] + m[4]*m[2]*m[9] + m[8]*m[1]*m[6] - m[8]*m[2]*m[5];
            float det = m[0]*inv[0] + m[1]*inv[4] + m[2]*inv[8] + m[3]*inv[12];
            float invdet = 1.0f / det;
            for (int i = 0; i < 16; ++i) sky_inv_vp[i] = inv[i] * invdet;
        }

        GLint loc = glGetUniformLocation(g_sky_pass.prog, "uInvViewProj");
        glUniformMatrix4fv(loc, 1, GL_FALSE, sky_inv_vp);

        vec2 sky_inv_res = { 1.0f / (float)g_scene_fb.width, 1.0f / (float)g_scene_fb.height };
        pass_set_vec2(&g_sky_pass, "uInvResolution", sky_inv_res);

        glDrawArrays(GL_TRIANGLES, 0, 3);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
    }
    check_gl_errors("sky pass");

    /* Shadow pass */
    {
        vec3 sc = model_center(&g_car);
        vec3 ext = model_extent(&g_car);
        float r = ext.x; if (ext.y > r) r = ext.y; if (ext.z > r) r = ext.z;
        r *= 0.55f;
        sc = v3(sc.x * 1.0f, sc.y * 1.0f, sc.z * 1.0f);
        /* Center in world: apply the car's auto-scale/centering */
        vec3 centerWorld = v3(0.0f, 0.0f, 0.0f);
        /* Radius in world units after scaling ~3 units long => use fixed 3 */
        float radiusWorld = 3.0f;
        shadow_update_matrix(&g_shadow, centerWorld, radiusWorld, light_dir);
        shadow_render(&g_shadow, g_car.meshes, g_car.count);
    }

    /* Scene */
    fb_bind(&g_scene_fb);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glClearColor(0.06f, 0.06f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    draw_car_into_fb(g_scene_fb.width, g_scene_fb.height, cam);
    check_gl_errors("scene render");

    /* SSR pass: reads scene color + depth, writes SSR reflections */
    {
        fb_bind(&g_ssr_fb);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        pass_use(&g_ssr_pass);
    check_gl_errors("after pass_use");

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_scene_fb.color);
        pass_set_i32(&g_ssr_pass, "uSceneColor", 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, g_scene_fb.depth);
        pass_set_i32(&g_ssr_pass, "uSceneDepth", 1);

        /* Compute viewProj locally (same as in draw_car_into_fb) */
        vec3 ssr_target = v3(0,0,0);
        vec3 ssr_up = v3(0,1,0);
        mat4 ssr_view = m4_look_at(cam, ssr_target, ssr_up);
        mat4 ssr_proj = m4_perspective(50.0f * 3.14159265f / 180.0f,
                                       (float)g_scene_fb.width / (float)g_scene_fb.height,
                                       0.1f, 200.0f);
        mat4 ssr_viewproj = m4_mul(ssr_proj, ssr_view);

        /* Compute inverse view-projection */
        float inv_vp[16];
        {
            /* Inline 4x4 inverse */
            const float* m = ssr_viewproj.m;
            float inv[16];
            inv[0] =  m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15] + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
            inv[4] = -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15] - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
            inv[8] =  m[4]*m[9]*m[15] - m[4]*m[11]*m[13] - m[8]*m[5]*m[15] + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
            inv[12] = -m[4]*m[9]*m[14] + m[4]*m[10]*m[13] + m[8]*m[5]*m[14] - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];
            inv[1] = -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15] - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
            inv[5] =  m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15] + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
            inv[9] = -m[0]*m[9]*m[15] + m[0]*m[11]*m[13] + m[8]*m[1]*m[15] - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
            inv[13] = m[0]*m[9]*m[14] - m[0]*m[10]*m[13] - m[8]*m[1]*m[14] + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];
            inv[2] =  m[1]*m[6]*m[15] - m[1]*m[7]*m[14] - m[5]*m[2]*m[15] + m[5]*m[3]*m[14] + m[13]*m[2]*m[7] - m[13]*m[3]*m[6];
            inv[6] = -m[0]*m[6]*m[15] + m[0]*m[7]*m[14] + m[4]*m[2]*m[15] - m[4]*m[3]*m[14] - m[12]*m[2]*m[7] + m[12]*m[3]*m[6];
            inv[10] = m[0]*m[5]*m[15] - m[0]*m[7]*m[13] - m[4]*m[1]*m[15] + m[4]*m[3]*m[13] + m[12]*m[1]*m[7] - m[12]*m[3]*m[5];
            inv[14] = -m[0]*m[5]*m[14] + m[0]*m[6]*m[13] + m[4]*m[1]*m[14] - m[4]*m[2]*m[13] - m[12]*m[1]*m[6] + m[12]*m[2]*m[5];
            inv[3] = -m[1]*m[6]*m[11] + m[1]*m[7]*m[10] + m[5]*m[2]*m[11] - m[5]*m[3]*m[10] - m[9]*m[2]*m[7] + m[9]*m[3]*m[6];
            inv[7] =  m[0]*m[6]*m[11] - m[0]*m[7]*m[10] - m[4]*m[2]*m[11] + m[4]*m[3]*m[10] + m[8]*m[2]*m[7] - m[8]*m[3]*m[6];
            inv[11] = -m[0]*m[5]*m[11] + m[0]*m[7]*m[9] + m[4]*m[1]*m[11] - m[4]*m[3]*m[9] - m[8]*m[1]*m[7] + m[8]*m[3]*m[5];
            inv[15] = m[0]*m[5]*m[10] - m[0]*m[6]*m[9] - m[4]*m[1]*m[10] + m[4]*m[2]*m[9] + m[8]*m[1]*m[6] - m[8]*m[2]*m[5];
            float det = m[0]*inv[0] + m[1]*inv[4] + m[2]*inv[8] + m[3]*inv[12];
            float invdet = 1.0f / det;
            for (int i = 0; i < 16; ++i) inv_vp[i] = inv[i] * invdet;
        }
        pass_set_mat4(&g_ssr_pass, "uInvViewProj", (mat4){0});  /* will set below */
        GLint loc = glGetUniformLocation(g_ssr_pass.prog, "uInvViewProj");
        glUniformMatrix4fv(loc, 1, GL_FALSE, inv_vp);

        loc = glGetUniformLocation(g_ssr_pass.prog, "uViewProj");
        glUniformMatrix4fv(loc, 1, GL_FALSE, ssr_viewproj.m);

        pass_set_vec3(&g_ssr_pass, "uCamPos", cam);
        vec2 res = { (float)g_scene_fb.width, (float)g_scene_fb.height };
        pass_set_vec2(&g_ssr_pass, "uResolution", res);

        glDrawArrays(GL_TRIANGLES, 0, 3);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
    }

    /* Composite pass: scene + SSR -> final HDR */
    {
        fb_bind(&g_composite_fb);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        pass_use(&g_ssr_composite_pass);
    check_gl_errors("after pass_use");

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_scene_fb.color);
        pass_set_i32(&g_ssr_composite_pass, "uSceneColor", 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, g_ssr_fb.color);
        pass_set_i32(&g_ssr_composite_pass, "uSSRColor", 1);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, g_scene_fb.depth);
        pass_set_i32(&g_ssr_composite_pass, "uSceneDepth", 2);

        /* Recompute ssr_viewproj (same as in SSR block) */
        vec3 ssr_target = v3(0,0,0);
        vec3 ssr_up = v3(0,1,0);
        mat4 ssr_view = m4_look_at(cam, ssr_target, ssr_up);
        mat4 ssr_proj = m4_perspective(50.0f * 3.14159265f / 180.0f,
                                       (float)g_scene_fb.width / (float)g_scene_fb.height,
                                       0.1f, 200.0f);
        mat4 ssr_vp = m4_mul(ssr_proj, ssr_view);

        /* Compute inverse for depth reconstruction */
        float inv_vp[16];
        {
            const float* m = ssr_vp.m;
            float inv[16];
            inv[0] =  m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15] + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
            inv[4] = -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15] - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
            inv[8] =  m[4]*m[9]*m[15] - m[4]*m[11]*m[13] - m[8]*m[5]*m[15] + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
            inv[12] = -m[4]*m[9]*m[14] + m[4]*m[10]*m[13] + m[8]*m[5]*m[14] - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];
            inv[1] = -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15] - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
            inv[5] =  m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15] + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
            inv[9] = -m[0]*m[9]*m[15] + m[0]*m[11]*m[13] + m[8]*m[1]*m[15] - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
            inv[13] = m[0]*m[9]*m[14] - m[0]*m[10]*m[13] - m[8]*m[1]*m[14] + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];
            inv[2] =  m[1]*m[6]*m[15] - m[1]*m[7]*m[14] - m[5]*m[2]*m[15] + m[5]*m[3]*m[14] + m[13]*m[2]*m[7] - m[13]*m[3]*m[6];
            inv[6] = -m[0]*m[6]*m[15] + m[0]*m[7]*m[14] + m[4]*m[2]*m[15] - m[4]*m[3]*m[14] - m[12]*m[2]*m[7] + m[12]*m[3]*m[6];
            inv[10] = m[0]*m[5]*m[15] - m[0]*m[7]*m[13] - m[4]*m[1]*m[15] + m[4]*m[3]*m[13] + m[12]*m[1]*m[7] - m[12]*m[3]*m[5];
            inv[14] = -m[0]*m[5]*m[14] + m[0]*m[6]*m[13] + m[4]*m[1]*m[14] - m[4]*m[2]*m[13] - m[12]*m[1]*m[6] + m[12]*m[2]*m[5];
            inv[3] = -m[1]*m[6]*m[11] + m[1]*m[7]*m[10] + m[5]*m[2]*m[11] - m[5]*m[3]*m[10] - m[9]*m[2]*m[7] + m[9]*m[3]*m[6];
            inv[7] =  m[0]*m[6]*m[11] - m[0]*m[7]*m[10] - m[4]*m[2]*m[11] + m[4]*m[3]*m[10] + m[8]*m[2]*m[7] - m[8]*m[3]*m[6];
            inv[11] = -m[0]*m[5]*m[11] + m[0]*m[7]*m[9] + m[4]*m[1]*m[11] - m[4]*m[3]*m[9] - m[8]*m[1]*m[7] + m[8]*m[3]*m[5];
            inv[15] = m[0]*m[5]*m[10] - m[0]*m[6]*m[9] - m[4]*m[1]*m[10] + m[4]*m[2]*m[9] + m[8]*m[1]*m[6] - m[8]*m[2]*m[5];
            float det = m[0]*inv[0] + m[1]*inv[4] + m[2]*inv[8] + m[3]*inv[12];
            float invdet = 1.0f / det;
            for (int i = 0; i < 16; ++i) inv_vp[i] = inv[i] * invdet;
        }

        GLint loc = glGetUniformLocation(g_ssr_composite_pass.prog, "uInvViewProj");
        glUniformMatrix4fv(loc, 1, GL_FALSE, inv_vp);

        pass_set_vec3(&g_ssr_composite_pass, "uCamPos", cam);
        vec2 res = { (float)g_scene_fb.width, (float)g_scene_fb.height };
        pass_set_vec2(&g_ssr_composite_pass, "uResolution", res);

        glDrawArrays(GL_TRIANGLES, 0, 3);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
    }

    /* TAA resolve: composite -> TAA history */
    if (g_taa_enabled) {
        int curr = g_taa_ping;
        int prev = 1 - g_taa_ping;

        /* First frame: initialize history with current frame */
        if (g_frame_index == 0) {
            fb_bind(&g_taa_history[prev]);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            glClear(GL_COLOR_BUFFER_BIT);
            pass_use(&g_blit_pass);
    check_gl_errors("after pass_use");
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, g_composite_fb.color);
            pass_set_tex(&g_blit_pass, "uScene", 0);
            vec2 inv0 = { 1.0f / (float)g_composite_fb.width, 1.0f / (float)g_composite_fb.height };
            pass_set_vec2(&g_blit_pass, "uInvSize", inv0);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }

        fb_bind(&g_taa_history[curr]);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        pass_use(&g_taa_pass);
    check_gl_errors("after pass_use");

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_composite_fb.color);
        pass_set_i32(&g_taa_pass, "uCurrentColor", 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, g_taa_history[prev].color);
        pass_set_i32(&g_taa_pass, "uHistoryColor", 1);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, g_scene_fb.depth);
        pass_set_i32(&g_taa_pass, "uCurrentDepth", 2);

        /* Current viewProj and its inverse — same formulas as before */
        vec3 taa_target = v3(0,0,0);
        vec3 taa_up = v3(0,1,0);
        mat4 taa_view = m4_look_at(cam, taa_target, taa_up);
        mat4 taa_proj = m4_perspective(50.0f * 3.14159265f / 180.0f,
                                       (float)g_scene_fb.width / (float)g_scene_fb.height,
                                       0.1f, 200.0f);
        mat4 taa_vp = m4_mul(taa_proj, taa_view);

        float taa_inv_vp[16];
        {
            const float* m = taa_vp.m;
            float inv[16];
            inv[0] =  m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15] + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
            inv[4] = -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15] - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
            inv[8] =  m[4]*m[9]*m[15] - m[4]*m[11]*m[13] - m[8]*m[5]*m[15] + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
            inv[12] = -m[4]*m[9]*m[14] + m[4]*m[10]*m[13] + m[8]*m[5]*m[14] - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];
            inv[1] = -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15] - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
            inv[5] =  m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15] + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
            inv[9] = -m[0]*m[9]*m[15] + m[0]*m[11]*m[13] + m[8]*m[1]*m[15] - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
            inv[13] = m[0]*m[9]*m[14] - m[0]*m[10]*m[13] - m[8]*m[1]*m[14] + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];
            inv[2] =  m[1]*m[6]*m[15] - m[1]*m[7]*m[14] - m[5]*m[2]*m[15] + m[5]*m[3]*m[14] + m[13]*m[2]*m[7] - m[13]*m[3]*m[6];
            inv[6] = -m[0]*m[6]*m[15] + m[0]*m[7]*m[14] + m[4]*m[2]*m[15] - m[4]*m[3]*m[14] - m[12]*m[2]*m[7] + m[12]*m[3]*m[6];
            inv[10] = m[0]*m[5]*m[15] - m[0]*m[7]*m[13] - m[4]*m[1]*m[15] + m[4]*m[3]*m[13] + m[12]*m[1]*m[7] - m[12]*m[3]*m[5];
            inv[14] = -m[0]*m[5]*m[14] + m[0]*m[6]*m[13] + m[4]*m[1]*m[14] - m[4]*m[2]*m[13] - m[12]*m[1]*m[6] + m[12]*m[2]*m[5];
            inv[3] = -m[1]*m[6]*m[11] + m[1]*m[7]*m[10] + m[5]*m[2]*m[11] - m[5]*m[3]*m[10] - m[9]*m[2]*m[7] + m[9]*m[3]*m[6];
            inv[7] =  m[0]*m[6]*m[11] - m[0]*m[7]*m[10] - m[4]*m[2]*m[11] + m[4]*m[3]*m[10] + m[8]*m[2]*m[7] - m[8]*m[3]*m[6];
            inv[11] = -m[0]*m[5]*m[11] + m[0]*m[7]*m[9] + m[4]*m[1]*m[11] - m[4]*m[3]*m[9] - m[8]*m[1]*m[7] + m[8]*m[3]*m[5];
            inv[15] = m[0]*m[5]*m[10] - m[0]*m[6]*m[9] - m[4]*m[1]*m[10] + m[4]*m[2]*m[9] + m[8]*m[1]*m[6] - m[8]*m[2]*m[5];
            float det = m[0]*inv[0] + m[1]*inv[4] + m[2]*inv[8] + m[3]*inv[12];
            float invdet = 1.0f / det;
            for (int i = 0; i < 16; ++i) taa_inv_vp[i] = inv[i] * invdet;
        }

        GLint loc = glGetUniformLocation(g_taa_pass.prog, "uInvViewProj");
        glUniformMatrix4fv(loc, 1, GL_FALSE, taa_inv_vp);
        loc = glGetUniformLocation(g_taa_pass.prog, "uPrevViewProj");
        glUniformMatrix4fv(loc, 1, GL_FALSE, g_prev_viewproj.m);

        pass_set_vec3(&g_taa_pass, "uCamPos", cam);
        vec2 res = { (float)g_composite_fb.width, (float)g_composite_fb.height };
        pass_set_vec2(&g_taa_pass, "uResolution", res);
        pass_set_f32(&g_taa_pass, "uBlendFactor", 0.9f);

        glDrawArrays(GL_TRIANGLES, 0, 3);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);

        g_taa_ping = prev;   /* swap for next frame */

        /* Save this frame's viewProj for next frame's motion vectors */
        g_prev_viewproj = taa_vp;
    }

    /* Pick the source HDR buffer */
    Framebuffer* final_hdr = g_taa_enabled ? &g_taa_history[1 - g_taa_ping] : &g_composite_fb;

    /* DOF: read scene color + depth, write to g_dof_fb */
    if (g_dof_enabled && g_dof_fb.fbo) {
        fb_bind(&g_dof_fb);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        pass_use(&g_dof_pass);
    check_gl_errors("after pass_use");

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, final_hdr->color);
        pass_set_i32(&g_dof_pass, "uSceneColor", 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, g_scene_fb.depth);
        pass_set_i32(&g_dof_pass, "uSceneDepth", 1);

        /* Recompute view matrix (same as scene render) */
        vec3 dof_target = v3(0,0,0);
        vec3 dof_up = v3(0,1,0);
        mat4 dof_view = m4_look_at(cam, dof_target, dof_up);
        mat4 dof_proj = m4_perspective(50.0f * 3.14159265f / 180.0f,
                                       (float)g_scene_fb.width / (float)g_scene_fb.height,
                                       0.1f, 200.0f);
        mat4 dof_vp = m4_mul(dof_proj, dof_view);

        float dof_inv_vp[16];
        {
            const float* m = dof_vp.m;
            float inv[16];
            inv[0] =  m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15] + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
            inv[4] = -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15] - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
            inv[8] =  m[4]*m[9]*m[15] - m[4]*m[11]*m[13] - m[8]*m[5]*m[15] + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
            inv[12] = -m[4]*m[9]*m[14] + m[4]*m[10]*m[13] + m[8]*m[5]*m[14] - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];
            inv[1] = -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15] - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
            inv[5] =  m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15] + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
            inv[9] = -m[0]*m[9]*m[15] + m[0]*m[11]*m[13] + m[8]*m[1]*m[15] - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
            inv[13] = m[0]*m[9]*m[14] - m[0]*m[10]*m[13] - m[8]*m[1]*m[14] + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];
            inv[2] =  m[1]*m[6]*m[15] - m[1]*m[7]*m[14] - m[5]*m[2]*m[15] + m[5]*m[3]*m[14] + m[13]*m[2]*m[7] - m[13]*m[3]*m[6];
            inv[6] = -m[0]*m[6]*m[15] + m[0]*m[7]*m[14] + m[4]*m[2]*m[15] - m[4]*m[3]*m[14] - m[12]*m[2]*m[7] + m[12]*m[3]*m[6];
            inv[10] = m[0]*m[5]*m[15] - m[0]*m[7]*m[13] - m[4]*m[1]*m[15] + m[4]*m[3]*m[13] + m[12]*m[1]*m[7] - m[12]*m[3]*m[5];
            inv[14] = -m[0]*m[5]*m[14] + m[0]*m[6]*m[13] + m[4]*m[1]*m[14] - m[4]*m[2]*m[13] - m[12]*m[1]*m[6] + m[12]*m[2]*m[5];
            inv[3] = -m[1]*m[6]*m[11] + m[1]*m[7]*m[10] + m[5]*m[2]*m[11] - m[5]*m[3]*m[10] - m[9]*m[2]*m[7] + m[9]*m[3]*m[6];
            inv[7] =  m[0]*m[6]*m[11] - m[0]*m[7]*m[10] - m[4]*m[2]*m[11] + m[4]*m[3]*m[10] + m[8]*m[2]*m[7] - m[8]*m[3]*m[6];
            inv[11] = -m[0]*m[5]*m[11] + m[0]*m[7]*m[9] + m[4]*m[1]*m[11] - m[4]*m[3]*m[9] - m[8]*m[1]*m[7] + m[8]*m[3]*m[5];
            inv[15] = m[0]*m[5]*m[10] - m[0]*m[6]*m[9] - m[4]*m[1]*m[10] + m[4]*m[2]*m[9] + m[8]*m[1]*m[6] - m[8]*m[2]*m[5];
            float det = m[0]*inv[0] + m[1]*inv[4] + m[2]*inv[8] + m[3]*inv[12];
            float invdet = 1.0f / det;
            for (int i = 0; i < 16; ++i) dof_inv_vp[i] = inv[i] * invdet;
        }

        GLint loc = glGetUniformLocation(g_dof_pass.prog, "uInvViewProj");
        glUniformMatrix4fv(loc, 1, GL_FALSE, dof_inv_vp);

        pass_set_vec3(&g_dof_pass, "uCamPos", cam);
        pass_set_f32 (&g_dof_pass, "uFocusDistance", g_focus_distance);
        pass_set_f32 (&g_dof_pass, "uFocusRange", g_focus_range);
        pass_set_f32 (&g_dof_pass, "uMaxBlurRadius", g_max_blur_radius);
        vec2 dof_res = { (float)g_scene_fb.width, (float)g_scene_fb.height };
        pass_set_vec2(&g_dof_pass, "uInvResolution", (vec2){1.0f/dof_res.x, 1.0f/dof_res.y});

        glDrawArrays(GL_TRIANGLES, 0, 3);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);

        /* Redirect bloom and composite to read from DOF output */
        final_hdr = &g_dof_fb;
    }

    /* Bloom: bright pass + downsample + upsample chain */
    bloom_run(&g_bloom, final_hdr, 0.8f);

    /* Bloom debug: if M cycled to mode 13, blit the bloom mip directly */
    if (g_debug_mode == 13) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, g_screen_w, g_screen_h);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        pass_use(&g_bloom_debug_pass);
    check_gl_errors("after pass_use");
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_bloom.mips[0].color);
        pass_set_i32(&g_bloom_debug_pass, "uBloom", 0);
        vec2 inv_dbg = { 1.0f / (float)g_bloom.mips[0].width, 1.0f / (float)g_bloom.mips[0].height };
        pass_set_vec2(&g_bloom_debug_pass, "uInvSize", inv_dbg);
        pass_set_f32 (&g_bloom_debug_pass, "uGain", 5.0f);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        return;
    }

    /* Final composite: HDR + bloom, tonemapped to canvas */
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, g_screen_w, g_screen_h);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glClearColor(0.06f, 0.06f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    pass_use(&g_bloom_composite_pass);
    check_gl_errors("after pass_use");

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, final_hdr->color);
    pass_set_i32(&g_bloom_composite_pass, "uScene", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, g_bloom.mips[0].color);
    pass_set_i32(&g_bloom_composite_pass, "uBloom", 1);

    pass_set_f32(&g_bloom_composite_pass, "uBloomStrength", 0.15f);
    vec2 inv = { 1.0f / (float)final_hdr->width, 1.0f / (float)final_hdr->height };
    pass_set_vec2(&g_bloom_composite_pass, "uInvSize", inv);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    /* Overlays */
    glViewport(8, 8, 200, 200);
    pass_use(&g_octa_debug_pass);
    check_gl_errors("after pass_use");
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_capture.fb.color);
    pass_set_tex(&g_octa_debug_pass, "uOcta", 0);
    vec2 inv2 = { 1.0f / (float)g_capture.fb.width, 1.0f / (float)g_capture.fb.height };
    pass_set_vec2(&g_octa_debug_pass, "uInvResolution", inv2);
    pass_set_f32 (&g_octa_debug_pass, "uGain", 1.0f);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glViewport(8, g_screen_h - 200 - 8, 200, 200);
    pass_use(&g_octa_debug_pass);
    check_gl_errors("after pass_use");
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_sh_recon_fb.color);
    pass_set_tex(&g_octa_debug_pass, "uOcta", 0);
    vec2 inv3 = { 1.0f / (float)g_sh_recon_fb.width, 1.0f / (float)g_sh_recon_fb.height };
    pass_set_vec2(&g_octa_debug_pass, "uInvResolution", inv3);
    pass_set_f32 (&g_octa_debug_pass, "uGain", 1.0f);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glViewport(0, 0, g_screen_w, g_screen_h);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    g_frame_index++;

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        printf("GL error: 0x%04X\n", err);
        while (glGetError() != GL_NO_ERROR) {}
    }
    check_gl_errors("end of frame");
}

int main(void) {
    EmscriptenWebGLContextAttributes attrs;
    emscripten_webgl_init_context_attributes(&attrs);
    attrs.majorVersion = 2;
    attrs.minorVersion = 0;
    attrs.alpha = EM_FALSE;
    attrs.depth = EM_TRUE;
    attrs.antialias = EM_TRUE;

    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx =
        emscripten_webgl_create_context("#canvas", &attrs);
    if (ctx <= 0) { fprintf(stderr, "failed to create WebGL2 context\n"); return 1; }
    emscripten_webgl_make_context_current(ctx);

    printf("GL_VERSION  = %s\n", glGetString(GL_VERSION));
    printf("GL_RENDERER = %s\n", glGetString(GL_RENDERER));

    init();
    emscripten_set_main_loop(frame, 0, 1);
    return 0;
}
