#version 300 es
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
layout(location=3) in vec4 aTangent;

uniform mat4 uModel;
uniform mat4 uViewProj;
uniform mat4 uLightVP;
uniform mat3 uNormalMat;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUV;
out vec3 vTangent;
out vec3 vBitangent;
out vec4 vLightSpacePos;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;

    vNormal    = normalize(uNormalMat * aNormal);
    vTangent   = normalize(uNormalMat * aTangent.xyz);
    vBitangent = cross(vNormal, vTangent) * aTangent.w;

    vUV = aUV;
    vLightSpacePos = uLightVP * worldPos;

    gl_Position = uViewProj * worldPos;
}
