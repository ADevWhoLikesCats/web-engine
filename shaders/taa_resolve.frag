#version 300 es
precision highp float;

in vec2 vUV;

uniform sampler2D uCurrentColor;   /* this frame's composite output */
uniform sampler2D uHistoryColor;   /* previous frame's TAA result */
uniform sampler2D uCurrentDepth;   /* scene depth (for sky mask) */
uniform mat4 uInvViewProj;         /* current frame's inverse viewProj */
uniform mat4 uPrevViewProj;        /* previous frame's viewProj */
uniform vec3 uCamPos;
uniform vec2 uResolution;
uniform float uBlendFactor;        /* 0.9 = 90% history, 10% new (default) */

out vec4 fragColor;

vec3 world_from_depth(vec2 uv, float depth) {
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 world = uInvViewProj * ndc;
    return world.xyz / world.w;
}

/* Neighborhood clamp: restrict history sample to the color range of the local neighborhood */
vec3 clip_to_neighborhood(vec3 history, vec2 uv, vec2 texel) {
    vec3 cmin = vec3(1e30);
    vec3 cmax = vec3(-1e30);
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            vec3 c = texture(uCurrentColor, uv + vec2(float(dx), float(dy)) * texel).rgb;
            cmin = min(cmin, c);
            cmax = max(cmax, c);
        }
    }
    /* Expand range slightly to prevent over-clamping */
    vec3 range = (cmax - cmin) * 1.5;
    vec3 mid = (cmax + cmin) * 0.5;
    return clamp(history, mid - range * 0.5, mid + range * 0.5);
}

void main() {
    vec2 uv = gl_FragCoord.xy / uResolution;
    vec2 texel = 1.0 / uResolution;

    float depth = texture(uCurrentDepth, uv).r;

    /* Sky: just copy the current frame (no TAA on background) */
    if (depth >= 0.9999) {
        fragColor = vec4(texture(uCurrentColor, uv).rgb, 1.0);
        return;
    }

    /* Reconstruct current world position */
    vec3 world_pos = world_from_depth(uv, depth);

    /* Project with previous frame's viewProj to find where this world point was on screen last frame */
    vec4 prev_clip = uPrevViewProj * vec4(world_pos, 1.0);
    if (prev_clip.w <= 0.0) {
        fragColor = vec4(texture(uCurrentColor, uv).rgb, 1.0);
        return;
    }
    vec2 prev_uv = prev_clip.xy / prev_clip.w * 0.5 + 0.5;

    /* Off-screen history: use current frame */
    if (prev_uv.x < 0.0 || prev_uv.x > 1.0 || prev_uv.y < 0.0 || prev_uv.y > 1.0) {
        fragColor = vec4(texture(uCurrentColor, uv).rgb, 1.0);
        return;
    }

    /* Sample history */
    vec3 current = texture(uCurrentColor, uv).rgb;
    vec3 history = texture(uHistoryColor, prev_uv).rgb;

    /* Clamp history to current frame's neighborhood (kills ghosting on moving content) */
    history = clip_to_neighborhood(history, uv, texel);

    /* Velocity-adaptive blend: at fast motion, trust current frame more */
    vec2 vel = abs(uv - prev_uv) * uResolution;
    float velocity_mag = length(vel);
    float blend = uBlendFactor;
    if (velocity_mag > 8.0) {
        blend = max(0.0, blend - (velocity_mag - 8.0) * 0.05);
    }

    vec3 result = mix(current, history, blend);
    fragColor = vec4(result, 1.0);
}
