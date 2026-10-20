#version 450 core

// Cielo degradado: fullscreen triangle (3 vertices, sin VBO) que cubre el
// viewport. Solo emite la coordenada NDC del pixel: el color sale de la
// DIRECCION de vista de ese pixel, no de su posicion en pantalla.
//
// El bloque de uniforms es el mismo en el vertex y en el fragment shader: en
// Vulkan los dos shaders comparten el binding, y el layout del bloque tiene
// que ser identico en los dos.

layout(location = 0) out vec2 vNdc;

layout(std140, binding = 0) uniform UboCielo {
    vec3 uColorTop;    // color del cenit (mirando bien hacia arriba)
    vec3 uColorBottom; // color del suelo (mirando bien hacia abajo)
    mat4 uInvViewProj; // inversa de projection*view
    vec3 uCamPos;      // posicion de la camara en el mundo
    float uTransicion; // media anchura de la transicion en unidades de dir.y
} ubo;

void main() {
    vec2 pos[3] = vec2[3](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );
    vec2 p = pos[gl_VertexIndex];
    vNdc = p;
    gl_Position = vec4(p, 0.0, 1.0);
}
