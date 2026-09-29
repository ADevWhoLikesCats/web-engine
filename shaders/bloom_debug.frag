#version 300 es
precision highp float;

uniform sampler2D uBloom;
uniform vec2 uInvSize;
uniform float uGain;

out vec4 fragColor;

void main() {
    vec2 uv = gl_FragCoord.xy * uInvSize;
    vec3 c = texture(uBloom, uv).rgb * uGain;
    fragColor = vec4(c, 1.0);
}
