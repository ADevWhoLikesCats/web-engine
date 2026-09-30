#version 300 es
// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The web-engine Authors
precision highp float;

uniform sampler2D uSceneColor;    // full-res HDR scene
uniform sampler2D uFogTexture;    // half-res fog (rgb=scatter, a=transmittance)
uniform vec2      uInvResolution;

out vec4 fragColor;

void main() {
    vec2 uv = gl_FragCoord.xy * uInvResolution;
    vec3 scene = texture(uSceneColor, uv).rgb;
    vec4 fog   = texture(uFogTexture, uv);

    // Composite: color * transmittance + scattered light
    vec3 result = scene * fog.a + fog.rgb;
    fragColor = vec4(result, 1.0);
}
