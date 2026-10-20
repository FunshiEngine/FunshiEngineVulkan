#version 450 core

// Fragment shader del cubemap del cielo.
//
// El bloque de uniforms es el mismo que en el vertex shader (mismo binding,
// mismo layout).

layout(location = 0) in vec3 vTexCoord;

layout(location = 0) out vec4 FragColor;

layout(std140, binding = 0) uniform UboCubemap {
    mat4 uView;
    mat4 uProjection;
} ubo;

layout(set = 0, binding = 1) uniform samplerCube uSkybox;

void main() {
    FragColor = texture(uSkybox, vTexCoord);
}
