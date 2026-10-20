#version 450 core

// Programa de sprites 2D (SpriteRenderer) y tilemaps: quad texturizado sin
// iluminacion.
//
// Atributos: 0 = posicion (vec3), 2 = uv (vec2). El 1 queda para la normal,
// que un sprite no usa.
//
// El recorte de la region de la textura (spritesheet) viaja en uUvRect
// (u0,v0,u1,v1) y se mezcla por vertice con la UV base del quad; asi una sola
// malla compartida sirve para cualquier region, sin duplicar geometria. El tinte
// y la opacidad (fade del componente) van en uTint y se multiplican sobre el
// texel (modo MODULATE, igual que el material 3D).
//
// El bloque de uniforms es el mismo en el vertex y en el fragment shader: en
// Vulkan los dos shaders comparten el binding, y el layout del bloque tiene
// que ser identico en los dos.

layout(location = 0) in vec3 aPosition;
layout(location = 2) in vec2 aUv;

layout(std140, binding = 0) uniform UboSprites {
    mat4 uModel;
    mat4 uView;
    mat4 uProjection;
    vec4 uUvRect;  // (u0, v0, u1, v1)
    vec4 uTint;
    int uUseTexture;
} ubo;

layout(location = 0) out vec2 vUv;

void main() {
    vUv = mix(ubo.uUvRect.xy, ubo.uUvRect.zw, aUv);
    gl_Position = ubo.uProjection * ubo.uView * ubo.uModel * vec4(aPosition, 1.0);
}
