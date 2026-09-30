#version 300 es
// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The web-engine Authors
precision highp float;

uniform sampler2D uSceneDepth;      // full-res depth
uniform sampler2D uShadowMap;       // directional shadow map
uniform mat4      uInvViewProj;     // inverse view-projection
uniform mat4      uLightVP;         // light view-projection
uniform vec3      uCamPos;
uniform vec3      uLightDir;        // direction light travels (normalized)
uniform vec3      uLightColor;
uniform float     uLightIntensity;
uniform vec2      uFullResolution;  // full-res canvas size
uniform vec2      uHalfResolution;  // half-res fog size
uniform float     uFogDensity;
uniform float     uFogHeightFall;
uniform float     uFogMaxDist;
uniform float     uFogSteps;
uniform float     uSunScatter;
uniform float     uAmbientScatter;

out vec4 fragColor;

// Reconstruct world position from full-res screen UV + depth
vec3 world_from_depth(vec2 uv, float depth) {
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 world = uInvViewProj * ndc;
    return world.xyz / world.w;
}

// PCF shadow lookup (single tap — steps are already averaged)
float shadow_at(vec3 world_pos, float NdotL) {
    vec4 lightClip = uLightVP * vec4(world_pos, 1.0);
    vec3 proj = lightClip.xyz / lightClip.w;
    proj = proj * 0.5 + 0.5;
    if (proj.x < 0.0 || proj.x > 1.0 ||
        proj.y < 0.0 || proj.y > 1.0 ||
        proj.z > 1.0) return 1.0;
    float d = texture(uShadowMap, proj.xy).r;
    float bias = max(0.002 * (1.0 - NdotL), 0.0008);
    return (proj.z - bias <= d) ? 1.0 : 0.0;
}

// Henyey-Greenstein phase function for forward scattering
float hg_phase(float cos_theta, float g) {
    float g2 = g * g;
    float denom = 1.0 + g2 - 2.0 * g * cos_theta;
    return (1.0 - g2) / (4.0 * 3.14159265 * pow(denom, 1.5));
}

void main() {
    // This pass runs at half-res. Map the half-res fragment to full-res UV.
    vec2 half_uv = gl_FragCoord.xy / uHalfResolution;
    vec2 full_uv = half_uv;

    float depth = texture(uSceneDepth, full_uv).r;

    // The far distance: if depth is sky, march to max distance
    float scene_dist;
    vec3 world;
    if (depth >= 0.9999) {
        scene_dist = uFogMaxDist;
        world = uCamPos;
    } else {
        world = world_from_depth(full_uv, depth);
        scene_dist = length(world - uCamPos);
        scene_dist = min(scene_dist, uFogMaxDist);
    }

    vec3 ray_dir = normalize(world - uCamPos);
    if (depth >= 0.9999) {
        // Reconstruct ray direction from UV for sky pixels
        vec4 ndc = vec4(full_uv * 2.0 - 1.0, 1.0, 1.0);
        vec4 world_far = uInvViewProj * ndc;
        ray_dir = normalize(world_far.xyz / world_far.w - uCamPos);
    }

    vec3 L = normalize(-uLightDir);
    float NdotL_view = max(dot(ray_dir, L), 0.0);

    // Phase function: forward scattering toward the sun
    float phase_sun     = hg_phase(dot(ray_dir, L), 0.6);
    float phase_ambient = 1.0 / (4.0 * 3.14159265);

    float step_size = scene_dist / uFogSteps;

    vec3 accum_scatter    = vec3(0.0);
    float accum_transmit  = 1.0;

    // Jitter start position to break banding (interleaved gradient noise)
    float jitter = fract(52.9829189 * fract(0.06711056 * gl_FragCoord.x +
                                            0.00583715 * gl_FragCoord.y));

    vec3 light_radiance = uLightColor * uLightIntensity;

    for (int i = 0; i < 128; ++i) {
        if (float(i) >= uFogSteps) break;

        float t = (float(i) + jitter) * step_size;
        if (t > scene_dist) break;

        vec3 pos = uCamPos + ray_dir * t;

        // Height-based density: denser near the ground
        float heightFactor = exp(-uFogHeightFall * max(pos.y, 0.0));
        float density = uFogDensity * heightFactor;

        // Sun visibility at this point
        float shadow = shadow_at(pos, NdotL_view);

        // Scattering contribution
        vec3 scatter_sun     = light_radiance * phase_sun     * uSunScatter     * shadow;
        vec3 scatter_ambient = light_radiance * phase_ambient * uAmbientScatter;

        vec3 scatter = (scatter_sun + scatter_ambient) * density * step_size;

        // Accumulate with extinction
        float extinction = density * step_size;
        accum_scatter += accum_transmit * scatter;
        accum_transmit *= exp(-extinction);
    }

    fragColor = vec4(accum_scatter, accum_transmit);
}
