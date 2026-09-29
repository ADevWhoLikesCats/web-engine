#version 300 es
precision highp float;
uniform sampler2D uScene;
uniform vec2 uInvSize;
out vec4 fragColor;
void main() {
    vec2 uv = gl_FragCoord.xy * uInvSize;
    fragColor = vec4(texture(uScene, uv).rgb, 1.0);
}
