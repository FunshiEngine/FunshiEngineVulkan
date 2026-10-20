#version 450 core

// Fragment shader del batch de lineas: color plano con el difuminado radial
// por distancia a la camara (la linea se vuelve transparente con la distancia
// y se descarta pasado el radio final).
//
// El bloque de uniforms es el mismo que en el vertex shader (mismo binding,
// mismo layout).

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec3 vWorldPosition;

layout(location = 0) out vec4 FragColor;

layout(std140, binding = 0) uniform UboLineas {
    mat4 uModel;
    mat4 uView;
    mat4 uProjection;
    vec4 uViewport;
    float uWidth;
    float uNear;
    vec3 uCameraPosition;
    float uDistanceFadeStart;
    float uDistanceFadeEnd;
} ubo;

void main() {
    float distanciaCamara = length(vWorldPosition - ubo.uCameraPosition);
    if (distanciaCamara >= ubo.uDistanceFadeEnd) discard;
    float rango = max(0.0001, ubo.uDistanceFadeEnd - ubo.uDistanceFadeStart);
    float t = clamp((distanciaCamara - ubo.uDistanceFadeStart) / rango, 0.0, 1.0);
    float opacidad = 1.0 - t * t;
    FragColor = vec4(vColor.rgb, vColor.a * opacidad);
}
