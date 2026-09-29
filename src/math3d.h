#ifndef MATH3D_H
#define MATH3D_H

#include <math.h>
#include <string.h>

typedef struct { float x, y; }         vec2;
typedef struct { float x, y, z; }      vec3;
typedef struct { float x, y, z, w; }   vec4;
typedef struct { float m[16]; }        mat4;

static inline vec3 v3(float x, float y, float z) { return (vec3){ x, y, z }; }
static inline vec3 v3_add(vec3 a, vec3 b) { return v3(a.x+b.x, a.y+b.y, a.z+b.z); }
static inline vec3 v3_sub(vec3 a, vec3 b) { return v3(a.x-b.x, a.y-b.y, a.z-b.z); }
static inline vec3 v3_scale(vec3 a, float s) { return v3(a.x*s, a.y*s, a.z*s); }

static inline float v3_dot(vec3 a, vec3 b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

static inline vec3 v3_cross(vec3 a, vec3 b) {
    return v3(
        a.y*b.z - a.z*b.y,
        a.z*b.x - a.x*b.z,
        a.x*b.y - a.y*b.x
    );
}

static inline float v3_len(vec3 a) { return sqrtf(v3_dot(a, a)); }

static inline vec3 v3_norm(vec3 a) {
    float l = v3_len(a);
    return l > 1e-8f ? v3_scale(a, 1.0f / l) : v3(0, 0, 0);
}

static inline mat4 m4_identity(void) {
    mat4 r;
    memset(r.m, 0, sizeof(r.m));
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
}

static inline mat4 m4_mul(mat4 a, mat4 b) {
    mat4 r;
    for (int c = 0; c < 4; c++) {
        for (int row = 0; row < 4; row++) {
            float s = 0.0f;
            for (int k = 0; k < 4; k++) {
                s += a.m[k*4 + row] * b.m[c*4 + k];
            }
            r.m[c*4 + row] = s;
        }
    }
    return r;
}

static inline mat4 m4_translate(vec3 t) {
    mat4 r = m4_identity();
    r.m[12] = t.x;
    r.m[13] = t.y;
    r.m[14] = t.z;
    return r;
}

static inline mat4 m4_scale(vec3 s) {
    mat4 r = m4_identity();
    r.m[0]  = s.x;
    r.m[5]  = s.y;
    r.m[10] = s.z;
    return r;
}

static inline mat4 m4_rotate_x(float rad) {
    float c = cosf(rad), s = sinf(rad);
    mat4 r = m4_identity();
    r.m[5]  =  c; r.m[6]  =  s;
    r.m[9]  = -s; r.m[10] =  c;
    return r;
}

static inline mat4 m4_rotate_y(float rad) {
    float c = cosf(rad), s = sinf(rad);
    mat4 r = m4_identity();
    r.m[0]  =  c; r.m[2]  = -s;
    r.m[8]  =  s; r.m[10] =  c;
    return r;
}

static inline mat4 m4_rotate_z(float rad) {
    float c = cosf(rad), s = sinf(rad);
    mat4 r = m4_identity();
    r.m[0] =  c; r.m[1] =  s;
    r.m[4] = -s; r.m[5] =  c;
    return r;
}

static inline mat4 m4_perspective(float fovy_rad, float aspect, float znear, float zfar) {
    float f = 1.0f / tanf(fovy_rad * 0.5f);
    mat4 r;
    memset(r.m, 0, sizeof(r.m));
    r.m[0]  = f / aspect;
    r.m[5]  = f;
    r.m[10] = (zfar + znear) / (znear - zfar);
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * zfar * znear) / (znear - zfar);
    return r;
}

static inline mat4 m4_look_at(vec3 eye, vec3 target, vec3 up) {
    vec3 f = v3_norm(v3_sub(target, eye));
    vec3 s = v3_norm(v3_cross(f, up));
    vec3 u = v3_cross(s, f);

    mat4 r = m4_identity();
    r.m[0] =  s.x; r.m[4] =  s.y; r.m[8]  =  s.z;
    r.m[1] =  u.x; r.m[5] =  u.y; r.m[9]  =  u.z;
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
    r.m[12] = -v3_dot(s, eye);
    r.m[13] = -v3_dot(u, eye);
    r.m[14] =  v3_dot(f, eye);
    return r;
}

/* Inverse-transpose of the upper-left 3x3 of m, written column-major. */
static inline void m4_to_mat3_normal(mat4 m, float out[9]) {
    float a00 = m.m[0], a01 = m.m[1], a02 = m.m[2];
    float a10 = m.m[4], a11 = m.m[5], a12 = m.m[6];
    float a20 = m.m[8], a21 = m.m[9], a22 = m.m[10];

    float det = a00 * (a11*a22 - a12*a21)
              - a01 * (a10*a22 - a12*a20)
              + a02 * (a10*a21 - a11*a20);
    float invDet = (fabsf(det) > 1e-8f) ? 1.0f / det : 0.0f;

    float i00 =  (a11*a22 - a12*a21) * invDet;
    float i01 = -(a01*a22 - a02*a21) * invDet;
    float i02 =  (a01*a12 - a02*a11) * invDet;

    float i10 = -(a10*a22 - a12*a20) * invDet;
    float i11 =  (a00*a22 - a02*a20) * invDet;
    float i12 = -(a00*a12 - a02*a10) * invDet;

    float i20 =  (a10*a21 - a11*a20) * invDet;
    float i21 = -(a00*a21 - a01*a20) * invDet;
    float i22 =  (a00*a11 - a01*a10) * invDet;

    out[0] = i00; out[1] = i10; out[2] = i20;
    out[3] = i01; out[4] = i11; out[5] = i21;
    out[6] = i02; out[7] = i12; out[8] = i22;
}

#endif /* MATH3D_H */
