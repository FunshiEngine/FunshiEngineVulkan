#version 450 core

// Fragment shader de sprites: tinte multiplicativo sobre el texel (o color
// plano si no hay textura) y descarte de los pixeles totalmente transparentes.
//
// El bloque de uniforms es el mismo que en el vertex shader (mismo binding,
// mismo layout).

layout(location = 0) in vec2 vUv;

layout(location = 0) out vec4 FragColor;

layout(std140, binding = 0) uniform UboSprites {
    mat4 uModel;
    mat4 uView;
    mat4 uProjection;
    vec4 uUvRect;
    vec4 uTint;
    int uUseTexture;
} ubo;

layout(set = 0, binding = 1) uniform sampler2D uTexture;

void main() {
    vec4 color = ubo.uTint;
    if (ubo.uUseTexture == 1) color *= texture(uTexture, vUv);
    if (color.a <= 0.0001) discard;
    FragColor = color;
}
