#version 300 es
precision highp float;

uniform sampler2D uSceneColor;   /* base scene render (with SG-only reflections) */
uniform sampler2D uSSRColor;     /* SSR hits: rgb = reflected color, a = hit or miss */
uniform sampler2D uSceneDepth;   /* scene depth for Fresnel fallback */
uniform vec3      uCamPos;
uniform mat4      uInvViewProj;
uniform vec2      uResolution;

out vec4 fragColor;

vec3 world_from_depth(vec2 uv, float depth) {
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 world = uInvViewProj * ndc;
    return world.xyz / world.w;
}

void main() {
    vec2 uv = gl_FragCoord.xy / uResolution;

    vec3 base = texture(uSceneColor, uv).rgb;
    vec4 ssr  = texture(uSSRColor, uv);
    float depth = texture(uSceneDepth, uv).r;

    /* Only composite over surfaces (not the sky) */
    if (depth >= 0.9999) {
        fragColor = vec4(base, 1.0);
        return;
    }

    /* Weight SSR by hit confidence and Fresnel (at glancing angles, more reflection) */
    vec3 world_pos = world_from_depth(uv, depth);
    vec3 V = normalize(uCamPos - world_pos);

    /* Reconstruct normal to compute a Fresnel-ish weight */
    vec2 texel = 1.0 / uResolution;
    vec3 p_right = world_from_depth(uv + vec2(texel.x, 0.0), texture(uSceneDepth, uv + vec2(texel.x, 0.0)).r);
    vec3 p_up    = world_from_depth(uv + vec2(0.0, texel.y), texture(uSceneDepth, uv + vec2(0.0, texel.y)).r);
    vec3 N = normalize(cross(p_right - world_pos, p_up - world_pos));

    float NdotV = max(dot(N, V), 0.001);
    /* Fresnel: stronger at grazing, weak head-on */
    float fres = pow(1.0 - NdotV, 5.0);
    float weight = mix(0.05, 0.6, fres);   /* base reflectivity ~5%, up to 60% at grazing */

    /* Only add SSR where it actually hit */
    vec3 reflected = ssr.rgb * ssr.a * weight;

    vec3 final = base + reflected;
    fragColor = vec4(final, 1.0);
}
