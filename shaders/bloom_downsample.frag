#version 300 es
precision highp float;

uniform sampler2D uSource;
uniform vec2 uTexel;   /* 1 / source_size */

out vec4 fragColor;

void main() {
    vec2 uv = gl_FragCoord.xy * uTexel;
    /* 4-tap box filter */
    vec3 c = vec3(0.0);
    c += texture(uSource, uv + vec2(-0.5, -0.5) * uTexel * 2.0).rgb;
    c += texture(uSource, uv + vec2( 0.5, -0.5) * uTexel * 2.0).rgb;
    c += texture(uSource, uv + vec2(-0.5,  0.5) * uTexel * 2.0).rgb;
    c += texture(uSource, uv + vec2( 0.5,  0.5) * uTexel * 2.0).rgb;
    fragColor = vec4(c * 0.25, 1.0);
}
