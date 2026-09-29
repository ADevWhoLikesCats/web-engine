#version 300 es
precision highp float;

uniform sampler2D uScene;
uniform vec2 uTexel;
uniform float uThreshold;

out vec4 fragColor;

void main() {
    vec2 uv = gl_FragCoord.xy * uTexel;
    vec3 c = texture(uScene, uv).rgb;

    /* Soft knee: pixels just above threshold contribute partially */
    float lum = dot(c, vec3(0.2126, 0.7152, 0.0722));
    float soft = lum - uThreshold + 0.1;
    soft = clamp(soft, 0.0, 0.2);
    soft = soft * soft / 0.4;

    float contrib = max(soft, lum - uThreshold) / max(lum, 1e-5);
    fragColor = vec4(c * contrib, 1.0);
}
