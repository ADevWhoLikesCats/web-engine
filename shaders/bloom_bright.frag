#version 300 es
precision highp float;

uniform sampler2D uScene;
uniform vec2 uTexel;
uniform float uThreshold;

out vec4 fragColor;

void main() {
    vec2 uv = gl_FragCoord.xy * uTexel;
    vec3 c = texture(uScene, uv).rgb;

    float lum = dot(c, vec3(0.2126, 0.7152, 0.0722));
    float soft = lum - uThreshold + 0.1;
    soft = clamp(soft, 0.0, 0.2);
    soft = soft * soft / 0.4;

    float contrib = max(soft, lum - uThreshold) / max(lum, 1e-5);
    vec3 bloom = c * contrib;

    /* Cap bloom contribution so a single bright pixel can't streak */
    bloom = min(bloom, vec3(5.0));

    fragColor = vec4(bloom, 1.0);
}
