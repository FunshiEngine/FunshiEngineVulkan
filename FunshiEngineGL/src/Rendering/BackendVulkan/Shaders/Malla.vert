#version 450 core

// Programa de malla: Blinn-Phong en espacio mundo que replica el modelo de
// iluminacion del motor (LightSystem + Material).
//
// Atributos: 0 = posicion (vec3), 1 = normal (vec3), 2 = uv (vec2),
// 3 = tangente (vec3), 4 = bitangente (vec3).
//
// El bloque de uniforms es el mismo en el vertex y en el fragment shader: en
// Vulkan los dos shaders comparten el binding, y el layout del bloque tiene
// que ser identico en los dos.

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUv;
layout(location = 3) in vec3 aTangent;
layout(location = 4) in vec3 aBitangent;

layout(std140, binding = 0) uniform UboMalla {
    mat4 uModel;
    mat4 uView;
    mat4 uProjection;
    mat3 uNormalMatrix;
    vec3 uCameraPosition;
    float uDistanceFadeStart;
    float uDistanceFadeEnd;
    vec3 uGlobalAmbient;
    int uLightCount;
    int uLightTypes[8];
    vec3 uLightWorldPos[8];
    vec3 uLightDir[8];
    vec3 uLightAmbient[8];
    vec3 uLightDiffuse[8];
    vec3 uLightSpecular[8];
    float uLightConstant[8];
    float uLightLinear[8];
    float uLightQuadratic[8];
    float uSpotCutoffCos[8];
    vec4 uMaterialAmbient;
    vec4 uMaterialDiffuse;
    vec4 uMaterialSpecular;
    vec4 uMaterialEmission;
    float uMaterialShininess;
    int uUseTexture;
    int uUseSpecularMap;
    int uUseEmissionMap;
    int uUseNormalMap;
} ubo;

layout(location = 0) out vec3 vNormalWorld;
layout(location = 1) out vec3 vWorldPos;
layout(location = 2) out vec2 vUv;
layout(location = 3) out mat3 vTbn;

void main() {
    vec4 world = ubo.uModel * vec4(aPosition, 1.0);
    vWorldPos = world.xyz;
    vNormalWorld = ubo.uNormalMatrix * aNormal;
    vUv = aUv;

    // Marco TBN en espacio mundo: la tangente se re-ortogonaliza contra la
    // normal (Gram-Schmidt) y la bitangente compensa la orientacion
    // (handedness) con el signo del producto mixto para que las caras
    // espejadas no se quiebren.
    vec3 t = normalize(ubo.uNormalMatrix * aTangent);
    vec3 n = normalize(vNormalWorld);
    t = normalize(t - dot(t, n) * n);
    vec3 b = cross(n, t);
    if (dot(cross(n, t), ubo.uNormalMatrix * aBitangent) < 0.0) b = -b;
    vTbn = mat3(t, b, n);

    gl_Position = ubo.uProjection * ubo.uView * world;
}
