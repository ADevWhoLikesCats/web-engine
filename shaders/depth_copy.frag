#version 300 es
precision highp float;
uniform sampler2D uDepth;
out vec4 fragColor;
void main() {
    ivec2 sz = textureSize(uDepth, 0);
    vec2 uv = gl_FragCoord.xy / vec2(float(sz.x), float(sz.y));
    fragColor = vec4(texture(uDepth, uv).r, 0.0, 0.0, 1.0);
}
