#version 300 es
precision highp float;

uniform sampler2D uSource;    /* smaller (blurred) */
uniform sampler2D uTarget;    /* larger (base to add to) */
uniform vec2 uTexel;          /* 1 / target_size */

out vec4 fragColor;

void main() {
    vec2 uv = gl_FragCoord.xy * uTexel;
    vec3 small = texture(uSource, uv).rgb;
    vec3 big   = texture(uTarget, uv).rgb;
    /* Additive blend with the smaller-level's wider blur */
    fragColor = vec4(big + small, 1.0);
}
