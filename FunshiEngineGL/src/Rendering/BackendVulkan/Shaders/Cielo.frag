#version 450 core

// Fragment shader del cielo degradado.
//
// Convencion: el motor entrega las matrices en el convenio de OpenGL (Y
// arriba, z en [-1,1]). El backend traduce las matrices al convenio de
// Vulkan (Y abajo, z en [0,1]); en particular, uInvViewProj llega con la
// columna Y invertida, de modo que el NDC que emite el vertex shader se
// des-proyecta con la misma direccion de vista que en OpenGL.
//
// El bloque de uniforms es el mismo que en el vertex shader (mismo binding,
// mismo layout).

layout(location = 0) in vec2 vNdc;

layout(location = 0) out vec4 FragColor;

layout(std140, binding = 0) uniform UboCielo {
    vec3 uColorTop;
    vec3 uColorBottom;
    mat4 uInvViewProj;
    vec3 uCamPos;
    float uTransicion;
} ubo;

void main() {
    // Rayo de vista de ESTE pixel en espacio de mundo: se des-proyecta el NDC
    // al plano lejano y se resta la camara. Asi el color depende de hacia donde
    // se mira y no de donde cae el pixel: el cielo va con la camara.
    vec4 lejano = ubo.uInvViewProj * vec4(vNdc, 1.0, 1.0);
    vec3 dir = normalize(lejano.xyz / lejano.w - ubo.uCamPos);

    // dir.y va de -1 (suelo) a +1 (cenit) y vale 0 en el horizonte: el degradado
    // se reparte alrededor del horizonte y satura en los dos extremos.
    float t = clamp(dir.y / ubo.uTransicion * 0.5 + 0.5, 0.0, 1.0);
    FragColor = vec4(mix(ubo.uColorBottom, ubo.uColorTop, t), 1.0);
}
