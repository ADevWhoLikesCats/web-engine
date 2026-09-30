#version 300 es
precision highp float;

uniform sampler2D uSource;   /* smaller (blurred) — the level below */
uniform vec2 uTexel;         /* 1 / target_size */

out vec4 fragColor;

void main() {
    vec2 uv = gl_FragCoord.xy * uTexel;
    vec3 small = texture(uSource, uv).rgb;
    /* Additive blend with the framebuffer's current content:
       the value from the level below is added to the accumulated
       result already stored in the target mip. */
    fragColor = vec4(small, 1.0);
}
