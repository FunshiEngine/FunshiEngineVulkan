#version 450 core

// Cubemap del cielo (skybox): un cubo unitario centrado en la camara, con la
// profundidad forzada al plano lejano (z = w), para que quede "detras" de toda
// la geometria.
//
// El cubemap se muestrea por direccion: la coordenada de textura es la
// posicion local del vertice, que coincide con la direccion de mundo porque el
// cubo no se rota.
//
// El bloque de uniforms es el mismo en el vertex y en el fragment shader: en
// Vulkan los dos shaders comparten el binding, y el layout del bloque tiene
// que ser identico en los dos.

layout(location = 0) in vec3 aPos;

layout(std140, binding = 0) uniform UboCubemap {
    mat4 uView;
    mat4 uProjection;
} ubo;

layout(location = 0) out vec3 vTexCoord;

void main() {
    vTexCoord = aPos;
    vec4 pos = ubo.uProjection * ubo.uView * vec4(aPos, 1.0);
    gl_Position = pos.xyww; // z = w para estar en el plano lejano
}
