#version 300 es
precision highp float;

uniform sampler2D uSH;            /* 9x1 RGBA32F, one texel per coef */
uniform vec2      uInvResolution; /* 1/output_size */
uniform float     uExposure;

out vec4 fragColor;

const float PI = 3.14159265359;

void sh_basis(vec3 d, out float Y[9]) {
    Y[0] = 0.282095;
    Y[1] = 0.488603 * d.y;
    Y[2] = 0.488603 * d.z;
    Y[3] = 0.488603 * d.x;
    Y[4] = 1.092548 * d.x * d.y;
    Y[5] = 1.092548 * d.y * d.z;
    Y[6] = 0.315392 * (3.0 * d.z * d.z - 1.0);
    Y[7] = 1.092548 * d.x * d.z;
    Y[8] = 0.546274 * (d.x * d.x - d.y * d.y);
}

vec3 oct_decode(vec2 uv) {
    uv = uv * 2.0 - 1.0;
    vec3 n = vec3(uv.x, uv.y, 1.0 - abs(uv.x) - abs(uv.y));
    float t = max(-n.z, 0.0);
    n.x += n.x >= 0.0 ? -t : t;
    n.y += n.y >= 0.0 ? -t : t;
    return normalize(n);
}

/* ACES + sRGB */
vec3 aces(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x*(a*x + b)) / (x*(c*x + d) + e), 0.0, 1.0);
}
vec3 to_srgb(vec3 c) {
    return mix(pow(c, vec3(1.0/2.4)) * 1.055 - 0.055,
               c * 12.92,
               lessThanEqual(c, vec3(0.0031308)));
}

void main() {
    vec2 uv = gl_FragCoord.xy * uInvResolution;
    vec3 d = oct_decode(uv);

    float Y[9];
    sh_basis(d, Y);

    vec3 E = vec3(0.0);
    for (int i = 0; i < 9; ++i) {
        vec3 L = texelFetch(uSH, ivec2(i, 0), 0).rgb;
        E += L * Y[i];
    }
    E *= uExposure;

    fragColor = vec4(to_srgb(aces(E)), 1.0);
}
