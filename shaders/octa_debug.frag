#version 300 es
precision highp float;

uniform sampler2D uOcta;
uniform vec2 uInvResolution;
uniform float uGain;

out vec4 fragColor;

const float PI = 3.14159265359;

/* ACES + sRGB so we can eyeball the HDR values */
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
    vec3 hdr = texture(uOcta, uv).rgb * uGain;
    vec3 ldr = to_srgb(aces(hdr));
    fragColor = vec4(ldr, 1.0);
}
