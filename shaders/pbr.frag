#version 300 es
precision highp float;

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;
in vec3 vTangent;
in vec3 vBitangent;
in vec4 vLightSpacePos;

uniform vec3  uCamPos;
uniform vec3  uLightDir;
uniform vec3  uLightColor;
uniform float uLightIntensity;

uniform int uDebugMode;

/* SH diffuse GI */
uniform sampler2D uSH;

/* SG specular GI (from probe grid) */
uniform sampler2D uProbeSGs;
uniform vec3      uSceneGridMin;
uniform vec3      uSceneGridMax;

/* Material textures */
uniform sampler2D uTexBasecolor;
uniform sampler2D uTexMR;
uniform sampler2D uTexNormal;
uniform sampler2D uTexEmissive;
uniform sampler2D uTexOcclusion;

uniform vec3  uBasecolorFactor;
uniform float uMetallicFactor;
uniform float uRoughnessFactor;
uniform float uNormalScale;

uniform int uHasBasecolor;
uniform int uHasMR;
uniform int uHasNormal;

/* Shadow */
uniform sampler2D uShadowMap;
uniform int       uShadowEnabled;

out vec4 fragColor;

const float PI = 3.14159265359;

/* ---------- SH L2 basis ---------- */
void sh_basis(vec3 d, out float Y[9]) {
    Y[0] = 0.282095;
    Y[1] = 0.488603 * d.y;
    Y[2] = 0.488603 * d.z;
    Y[3] = 0.488603 * d.x;
    Y[4] = 1.092548 * d.x * d.y;
    Y[5] = 1.092548 * d.y * d.z;
    Y[6] = 0.315392 * (3.0 * d.z * d.z - 1.0);
    Y[7] = 1.092548 * d.x * d.z;
    Y[8] = 0.546274 * (d.x * d.x - d.y * d.y);
}

vec3 sh_irradiance(vec3 n) {
    float Y[9];
    sh_basis(n, Y);
    const float A0 = 3.14159265;
    const float A1 = 2.09439510;
    const float A2 = 0.78539816;
    vec3 E = vec3(0.0);
    for (int i = 0; i < 9; ++i) {
        vec3 L = texelFetch(uSH, ivec2(i, 0), 0).rgb;
        float a = (i == 0) ? A0 : (i < 4) ? A1 : A2;
        E += L * Y[i] * a;
    }
    return max(E, vec3(0.0));
}

/* ---------- SG specular from probe grid ---------- */
vec3 sg_specular_at_probe(vec3 R, float roughness, int probe_row) {
    vec3 accum = vec3(0.0);
    float rough2 = max(roughness * roughness, 1e-4);
    for (int i = 0; i < 32; ++i) {
        vec4 axis_sharp = texelFetch(uProbeSGs, ivec2(i * 2 + 0, probe_row), 0);
        vec4 color_w    = texelFetch(uProbeSGs, ivec2(i * 2 + 1, probe_row), 0);
        vec3  axis      = axis_sharp.xyz;
        float sharp     = axis_sharp.w;
        vec3  color     = color_w.rgb;
        float w         = color_w.a;

        float s = sharp / (1.0 + sharp * rough2 * 2.0);
        float k = exp(s * (dot(R, axis) - 1.0));
        accum += color * k * w;
    }
    return max(accum * 0.10, vec3(0.0));
}

vec3 sg_specular(vec3 worldPos, vec3 R, float roughness) {
    vec3 span = max(uSceneGridMax - uSceneGridMin, vec3(1e-4));
    vec3 t = (worldPos - uSceneGridMin) / span;
    t = clamp(t, vec3(0.0), vec3(0.999));
    int px = int(t.x * 8.0);
    int py = int(t.y * 4.0);
    int pz = int(t.z * 8.0);
    int row = pz * (8 * 4) + py * 8 + px;
    return sg_specular_at_probe(R, roughness, row);
}

/* ---------- Shadows ---------- */
float hard_shadow(vec4 lightSpacePos, float bias) {
    vec3 proj = lightSpacePos.xyz / lightSpacePos.w;
    proj = proj * 0.5 + 0.5;
    if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0 || proj.z > 1.0) {
        return 1.0;
    }
    float d = texture(uShadowMap, proj.xy).r;
    return (proj.z - bias <= d) ? 1.0 : 0.0;
}

/* ---------- BRDF ---------- */
float D_GGX(float NdotH, float a) {
    float a2 = a * a;
    float d  = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}
float G_SchlickGGX(float NdotV, float k) {
    return NdotV / (NdotV * (1.0 - k) + k);
}
float G_Smith(float NdotV, float NdotL, float rough) {
    float r = rough + 1.0;
    float k = (r * r) / 8.0;
    return G_SchlickGGX(NdotV, k) * G_SchlickGGX(NdotL, k);
}
vec3 F_Schlick(vec3 F0, float VdotH) {
    return F0 + (vec3(1.0) - F0) * pow(1.0 - VdotH, 5.0);
}
vec3 evalSpecular(vec3 F0, float rough, float NdotV, float NdotL, float NdotH, float VdotH) {
    float a = rough * rough;
    float D = D_GGX(NdotH, a);
    float G = G_Smith(NdotV, NdotL, rough);
    vec3  F = F_Schlick(F0, VdotH);
    return (D * G * F) / max(4.0 * NdotV * NdotL, 1e-4);
}

void main() {
    /* --- Material sample --- */
    vec3 albedo = uBasecolorFactor;
    if (uHasBasecolor == 1) albedo *= texture(uTexBasecolor, vUV).rgb;

    float metallic = uMetallicFactor;
    float roughness = uRoughnessFactor;
    if (uHasMR == 1) {
        vec4 mr = texture(uTexMR, vUV);
        roughness *= mr.g;
        metallic  *= mr.b;
    }
    roughness = clamp(roughness, 0.04, 1.0);
    metallic  = clamp(metallic, 0.0, 1.0);

    /* --- Normal (with normal map) --- */
    vec3 N = normalize(vNormal);
    if (uHasNormal == 1) {
        vec3 nmap = texture(uTexNormal, vUV).rgb * 2.0 - 1.0;
        nmap.xy *= uNormalScale;
        mat3 TBN = mat3(normalize(vTangent), normalize(vBitangent), N);
        N = normalize(TBN * nmap);
    }

    vec3 V = normalize(uCamPos - vWorldPos);
    vec3 L = normalize(-uLightDir);
    vec3 H = normalize(V + L);
    vec3 R = reflect(-V, N);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 1e-4);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 kD = (vec3(1.0) - F_Schlick(F0, VdotH)) * (1.0 - metallic);

    /* --- Debug views --- */
    if (uDebugMode == 1) { fragColor = vec4(albedo / PI, 1.0); return; }
    if (uDebugMode == 2) {
        vec3 spec = evalSpecular(F0, roughness, NdotV, NdotL, NdotH, VdotH);
        fragColor = vec4(spec, 1.0); return;
    }
    if (uDebugMode == 3) { fragColor = vec4(F_Schlick(F0, VdotH), 1.0); return; }
    if (uDebugMode == 4) { fragColor = vec4(vec3(roughness), 1.0); return; }
    if (uDebugMode == 5) { fragColor = vec4(vec3(metallic), 1.0); return; }
    if (uDebugMode == 6) { fragColor = vec4(sh_irradiance(N), 1.0); return; }
    if (uDebugMode == 7) { fragColor = vec4(N * 0.5 + 0.5, 1.0); return; }
    if (uDebugMode == 8) {
        vec3 refl = sg_specular(vWorldPos, R, roughness);
        fragColor = vec4(refl, 1.0); return;
    }
    if (uDebugMode == 9) { fragColor = vec4(R * 0.5 + 0.5, 1.0); return; }
    if (uDebugMode == 10) {
        float bias = max(0.0015 * (1.0 - NdotL), 0.0005);
        float s = (uShadowEnabled == 1) ? hard_shadow(vLightSpacePos, bias) : 1.0;
        fragColor = vec4(vec3(1.0 - s), 1.0);
        return;
    }

    if (uDebugMode == 11) {
        vec3 span = max(uSceneGridMax - uSceneGridMin, vec3(1e-4));
        vec3 t = clamp((vWorldPos - uSceneGridMin) / span, 0.0, 1.0);
        fragColor = vec4(t, 1.0);
        return;
    }

    /* --- Direct lighting --- */
    float shadow = 1.0;
    if (uShadowEnabled == 1) {
        float bias = max(0.0015 * (1.0 - NdotL), 0.0005);
        shadow = hard_shadow(vLightSpacePos, bias);
    }

    vec3 radiance = uLightColor * uLightIntensity;
    vec3 diffuse  = kD * albedo / PI * radiance;
    vec3 specular = evalSpecular(F0, roughness, NdotV, NdotL, NdotH, VdotH) * radiance;
    vec3 direct   = (diffuse + specular) * NdotL * shadow;

    /* --- Indirect diffuse (SH) --- */
    vec3 E_ambient = sh_irradiance(N);
    vec3 indirect_diffuse = kD * albedo / PI * E_ambient;

    /* --- Indirect specular (SG probe grid) --- */
    vec3 sg_refl = sg_specular(vWorldPos, R, roughness);
    vec3 F_indirect = F_Schlick(F0, NdotV);
    float roughAtten = 1.0 - roughness * 0.6;
    vec3 indirect_specular = sg_refl * F_indirect * roughAtten;

    vec3 color = direct + indirect_diffuse + indirect_specular;
    fragColor = vec4(color, 1.0);
}
