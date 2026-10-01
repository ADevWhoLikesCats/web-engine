#version 300 es
precision highp float;

in vec2 vUV;

uniform sampler2D uSceneColor;
uniform sampler2D uSceneDepth;
uniform sampler2D uGBufferAlbedo;
uniform sampler2D uGBufferNormal;
uniform sampler2D uGBufferEmissive;
uniform mat4      uInvViewProj;
uniform mat4      uViewProj;
uniform vec3      uCamPos;
uniform vec2      uResolution;

out vec4 fragColor;

void main() {
    /* Step 6a: output black. Verifies the pass runs cleanly.
       Step 6b will replace this with the ray march. */
    fragColor = vec4(0.0, 0.0, 0.0, 1.0);
}
