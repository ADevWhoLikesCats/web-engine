#version 300 es
precision highp float;

uniform sampler2D uOcta;
uniform float     uOctaSize;
uniform vec4      uSGAxisSharp[32];

out vec4 fragColor;

float texel_solid_angle(vec2 uv01, float N) {
    vec2 uv = uv01 * 2.0 - 1.0;
    float du = 2.0 / N;
    float r2 = dot(uv, uv);
    return (4.0 * du * du) / pow(1.0 + r2, 1.5);
}

vec3 oct_decode(vec2 uv) {
    uv = uv * 2.0 - 1.0;
    vec3 n = vec3(uv.x, uv.y, 1.0 - abs(uv.x) - abs(uv.y));
    float t = max(-n.z, 0.0);
    n.x += n.x >= 0.0 ? -t : t;
    n.y += n.y >= 0.0 ? -t : t;
    return normalize(n);
}

void main() {
    int x = int(gl_FragCoord.x);
    int sg = x >> 1;
    int parity = x & 1;

    if (parity == 0) {
        fragColor = uSGAxisSharp[sg];
        return;
    }

    vec3 axis = uSGAxisSharp[sg].xyz;
    float sharpness = uSGAxisSharp[sg].w;

    vec3 accum = vec3(0.0);
    float weight = 0.0;
    float N = uOctaSize;
    int Ni = int(N);

    for (int y = 0; y < Ni; ++y) {
        for (int xx = 0; xx < Ni; ++xx) {
            vec2 uv = (vec2(float(xx), float(y)) + 0.5) / N;
            vec3 d = oct_decode(uv);
            vec3 col = texture(uOcta, uv).rgb;
            float dOmega = texel_solid_angle(uv, N);

            float kernel = exp(sharpness * (dot(d, axis) - 1.0));
            accum += col * kernel * dOmega;
            weight += kernel * dOmega;
        }
    }

    vec3 amplitude = (weight > 1e-6) ? accum / weight : vec3(0.0);
    fragColor = vec4(amplitude, weight);
}
