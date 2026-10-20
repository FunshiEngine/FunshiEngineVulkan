#version 450 core

// Fragment shader de la malla. El bloque de uniforms es el mismo que en el
// vertex shader (mismo binding, mismo layout).

layout(location = 0) in vec3 vNormalWorld;
layout(location = 1) in vec3 vWorldPos;
layout(location = 2) in vec2 vUv;
layout(location = 3) in mat3 vTbn;

layout(location = 0) out vec4 FragColor;

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

// Texturas opcionales del Material. Con su flag en 0 el material se colorea
// solo con los uniforms; cuando la textura esta, su texel multiplica
// (MODULA) al canal correspondiente.
layout(set = 0, binding = 1) uniform sampler2D uDiffuseTex;
layout(set = 0, binding = 2) uniform sampler2D uSpecularTex;
layout(set = 0, binding = 3) uniform sampler2D uEmissionTex;
layout(set = 0, binding = 4) uniform sampler2D uNormalTex;

vec3 normalizarSeguro(vec3 v) {
    float len = length(v);
    return (len < 1e-8) ? vec3(0.0) : v / len;
}

vec3 contribucionLuz(int i, vec3 N, vec3 V, vec3 fragPos, vec3 baseDiffuse,
                     vec3 baseSpecular) {
    int type = ubo.uLightTypes[i];

    vec3 L;
    float atenuacion = 1.0;
    if (type == 0) {
        L = -normalizarSeguro(ubo.uLightWorldPos[i]);
    } else {
        vec3 toLight = ubo.uLightWorldPos[i] - fragPos;
        float dist = length(toLight);
        if (dist < 1e-5) return ubo.uLightAmbient[i] * ubo.uMaterialAmbient.rgb;
        L = toLight / dist;
        atenuacion = 1.0 / max(1e-5,
            ubo.uLightConstant[i] + ubo.uLightLinear[i] * dist +
            ubo.uLightQuadratic[i] * dist * dist);
    }

    // El ambiente de cada luz NO se atenua con la distancia. El diffuse y el
    // especular si.
    vec3 resultado = ubo.uLightAmbient[i] * ubo.uMaterialAmbient.rgb;

    float factorSpot = 1.0;
    if (type == 2) {
        float cosAngulo = dot(-L, normalizarSeguro(ubo.uLightDir[i]));
        if (cosAngulo < ubo.uSpotCutoffCos[i]) {
            return resultado; // fuera del cono: solo ambiente
        }
        factorSpot = cosAngulo; // exponente del spot = 1
    }

    float ndotl = max(dot(N, L), 0.0);
    vec3 difuso = ndotl * ubo.uLightDiffuse[i] * baseDiffuse;

    vec3 especular = vec3(0.0);
    if (ndotl > 0.0 && ubo.uMaterialShininess > 0.0) {
        vec3 H = normalizarSeguro(L + V);
        float ndoth = max(dot(N, H), 0.0);
        especular = pow(ndoth, ubo.uMaterialShininess) *
                    ubo.uLightSpecular[i] * baseSpecular;
    }

    resultado += (difuso + especular) * (atenuacion * factorSpot);
    return resultado;
}

void main() {
    float distanciaCamara = length(vWorldPos - ubo.uCameraPosition);
    if (distanciaCamara >= ubo.uDistanceFadeEnd) discard;
    float rangoDifuminado = max(0.0001, ubo.uDistanceFadeEnd - ubo.uDistanceFadeStart);
    float t = clamp((distanciaCamara - ubo.uDistanceFadeStart) / rangoDifuminado,
                    0.0, 1.0);
    float opacidadDistancia = 1.0 - t * t;

    // Normal del fragmento: con normal map se perturba en espacio tangente
    // (vTbn); sin el, es la normal interpolada de la geometria.
    vec3 N = normalizarSeguro(vNormalWorld);
    if (ubo.uUseNormalMap == 1) {
        vec3 tn = normalize(texture(uNormalTex, vUv).rgb * 2.0 - 1.0);
        N = normalizarSeguro(vTbn * tn);
    }
    vec3 V = normalizarSeguro(ubo.uCameraPosition - vWorldPos);

    // El diffuse modulado que ven las luces: texel de textura por diffuse del
    // material. Sin textura (o con la malla sin UVs) queda igual.
    vec3 baseDiffuse = ubo.uMaterialDiffuse.rgb;
    if (ubo.uUseTexture == 1) baseDiffuse *= texture(uDiffuseTex, vUv).rgb;
    vec3 baseSpecular = ubo.uMaterialSpecular.rgb;
    if (ubo.uUseSpecularMap == 1) baseSpecular *= texture(uSpecularTex, vUv).rgb;

    // Ambiente global por material ambiente mas emision, una sola vez.
    vec3 color = ubo.uGlobalAmbient * ubo.uMaterialAmbient.rgb;
    for (int i = 0; i < 8; ++i) {
        if (i >= ubo.uLightCount) break;
        color += contribucionLuz(i, N, V, vWorldPos, baseDiffuse, baseSpecular);
    }
    vec3 emision = ubo.uMaterialEmission.rgb;
    if (ubo.uUseEmissionMap == 1) emision *= texture(uEmissionTex, vUv).rgb;
    color += emision;

    FragColor = vec4(color, ubo.uMaterialDiffuse.a * opacidadDistancia);
}
