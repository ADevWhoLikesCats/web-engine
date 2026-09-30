#version 300 es
precision highp float;

uniform mat4 uInvViewProj;
uniform vec2 uInvResolution;

out vec4 fragColor;

vec3 sky_color(vec3 d) {
    float up = clamp(d.y, -1.0, 1.0);

    /* More saturated colors so ACES doesn't wash them out */
    vec3 zenith  = vec3(0.08, 0.20, 0.62);   /* deep blue */
    vec3 horizon = vec3(0.85, 0.72, 0.55);   /* warm cream */
    vec3 ground  = vec3(0.10, 0.09, 0.08);   /* dark below horizon */

    vec3 col;
    if (up >= 0.0) {
        /* Sharper gradient — the blue shows quickly as you look up */
        col = mix(horizon, zenith, pow(up, 0.35));
    } else {
        col = mix(horizon, ground, pow(-up, 0.5));
    }

    /* Sun — direction should match the scene light. Light dir is (-0.4,-1.0,-0.5),
       so the sun is at the opposite: (+0.4, +1.0, +0.5) normalized. */
    vec3 sun_dir = normalize(vec3(0.4, 1.0, 0.5));
    float sun = max(dot(d, sun_dir), 0.0);
    col += vec3(1.0, 0.95, 0.85) * pow(sun, 800.0) * 15.0;   /* bright sun disk */
    col += vec3(1.0, 0.80, 0.55) * pow(sun, 12.0)  * 0.4;    /* wider glow */

    return col;
}

void main() {
    vec2 uv = gl_FragCoord.xy * uInvResolution;
    vec4 ndc = vec4(uv * 2.0 - 1.0, 1.0, 1.0);
    vec4 world = uInvViewProj * ndc;
    vec3 dir = normalize(world.xyz / world.w);
    fragColor = vec4(sky_color(dir), 1.0);
}
