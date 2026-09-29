#version 300 es
precision highp float;

uniform sampler2D uHDRI;
uniform mat4 uInvViewProj;
uniform vec2 uInvResolution;

out vec4 fragColor;

void main() {
    vec2 uv = gl_FragCoord.xy * uInvResolution;
    vec4 ndc = vec4(uv * 2.0 - 1.0, 1.0, 1.0);
    vec4 world = uInvViewProj * ndc;
    vec3 dir = normalize(world.xyz / world.w);

    float phi = atan(dir.z, dir.x);
    float theta = acos(clamp(dir.y, -1.0, 1.0));
    vec2 sky_uv = vec2(phi / (2.0 * 3.14159265) + 0.5,
                       theta / 3.14159265);

    vec3 col = texture(uHDRI, sky_uv).rgb;
    fragColor = vec4(col, 1.0);
}
