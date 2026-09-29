#version 300 es
precision highp float;

in vec2 vUV;

uniform sampler2D uSceneColor;   /* HDR scene (post-composite, pre-bloom) */
uniform sampler2D uSceneDepth;   /* depth texture */
uniform mat4 uInvViewProj;
uniform vec3 uCamPos;
uniform float uFocusDistance;    /* world units from camera */
uniform float uFocusRange;       /* how quickly things go out of focus */
uniform float uMaxBlurRadius;    /* in pixels */
uniform vec2 uInvResolution;

out vec4 fragColor;

/* Linearize perspective depth to world-space distance from camera */
float linearize_depth(float d, float znear, float zfar) {
    /* For a perspective projection with NDC z in [-1,1] mapped to [0,1] */
    float z_ndc = d * 2.0 - 1.0;
    return (2.0 * znear * zfar) / (zfar + znear - z_ndc * (zfar - znear));
}

vec3 world_from_depth(vec2 uv, float depth) {
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 world = uInvViewProj * ndc;
    return world.xyz / world.w;
}

/* Compute signed CoC in pixels. Positive = far blur, negative = near blur. */
float compute_coc(vec2 uv, float depth) {
    if (depth >= 0.9999) {
        /* Sky: always far-blurred proportionally to distance */
        return uMaxBlurRadius;
    }
    vec3 world = world_from_depth(uv, depth);
    float dist = distance(uCamPos, world);

    /* Signed distance from focus plane */
    float signed_dist = dist - uFocusDistance;
    /* Normalize by focus range and aperture */
    float coc = signed_dist / uFocusRange;

    /* Clamp to blur radius */
    return clamp(coc * uMaxBlurRadius, -uMaxBlurRadius, uMaxBlurRadius);
}

void main() {
    vec2 uv = gl_FragCoord.xy * uInvResolution;

    float depth = texture(uSceneDepth, uv).r;
    vec3 sharp = texture(uSceneColor, uv).rgb;
    float center_coc = compute_coc(uv, depth);

    float center_abs = abs(center_coc);

    /* If the pixel is nearly in focus, output it sharp */
    if (center_abs < 0.5) {
        fragColor = vec4(sharp, 1.0);
        return;
    }

    /* Bokeh sample pattern: 16-sample ring at radius ~ center_coc */
    const int SAMPLES = 16;
    const float PI = 3.14159265359;

    vec3 accum = vec3(0.0);
    float weight_sum = 0.0;

    /* Add a bit of jitter for smoother result */
    float jitter = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);

    for (int i = 0; i < SAMPLES; ++i) {
        float a = (float(i) / float(SAMPLES)) * 2.0 * PI + jitter;
        vec2 offset = vec2(cos(a), sin(a)) * uMaxBlurRadius * 0.7;
        vec2 sample_uv = uv + offset * uInvResolution;

        vec3 c = texture(uSceneColor, sample_uv).rgb;
        float d = texture(uSceneDepth, sample_uv).r;
        float coc = compute_coc(sample_uv, d);

        /* Weight: sample contributes if its CoC is large enough to reach this pixel */
        float sample_coc_abs = abs(coc);
        float dist_to_center = length(offset);
        float w = max(0.0, sample_coc_abs - dist_to_center + 1.0);

        accum += c * w;
        weight_sum += w;
    }

    vec3 blurred = (weight_sum > 0.0001) ? accum / weight_sum : sharp;

    /* Blend sharp and blurred based on CoC */
    float blend = clamp(center_abs / uMaxBlurRadius, 0.0, 1.0);
    vec3 result = mix(sharp, blurred, blend);
    fragColor = vec4(result, 1.0);
}
