#version 450 core

// Vertex shader del batch de lineas "gruesas" (grilla, marcadores de luz y
// camara, wireframes de colliders).
//
// Cada segmento llega expandido a un quad por el LineBuilder: los atributos
// son el inicio y el fin del segmento, el lado (+1/-1) y el avance a lo largo
// del segmento, y el color.
//
// El vertex shader proyecta ambos extremos, calcula la perpendicular de la
// direccion en pantalla y desplaza el vertice hacia su "lado" un numero de
// pixeles dado por uWidth: asi el ancho es constante en pixeles, con cualquier
// distancia y en cualquier resolucion.
//
// Tambien resuelve el recorte por el plano cercano en espacio vista (uNear): un
// segmento con un extremo detras de la camara se recorta contra el plano en vez
// de estirarse desde una posicion invalida, y uno completamente detras se
// colapsa fuera del frustum (asi la grilla no "explota" al cruzarse con la
// camara).
//
// El bloque de uniforms es el mismo en el vertex y en el fragment shader: en
// Vulkan los dos shaders comparten el binding, y el layout del bloque tiene
// que ser identico en los dos.

layout(location = 0) in vec3 aStart;
layout(location = 1) in vec3 aEnd;
layout(location = 2) in float aSide;
layout(location = 3) in float aAlong;
layout(location = 4) in vec4 aColor;

layout(std140, binding = 0) uniform UboLineas {
    mat4 uModel;
    mat4 uView;
    mat4 uProjection;
    vec4 uViewport;  // (anchoPx, altoPx, -, -)
    float uWidth;    // ancho de la linea en pixeles
    float uNear;     // distancia al plano cercano (positivo)
    vec3 uCameraPosition;
    float uDistanceFadeStart;
    float uDistanceFadeEnd;
} ubo;

layout(location = 0) out vec4 vColor;
layout(location = 1) out vec3 vWorldPosition;

void main() {
    // 1. Recorte del plano cercano en espacio vista (z hacia -near). La
    //    geometria viene en el espacio local del objeto (grilla, collider), asi
    //    que pasa por uModel antes que por la vista.
    vec3 worldS = (ubo.uModel * vec4(aStart, 1.0)).xyz;
    vec3 worldE = (ubo.uModel * vec4(aEnd, 1.0)).xyz;
    vec3 s = (ubo.uView * vec4(worldS, 1.0)).xyz;
    vec3 e = (ubo.uView * vec4(worldE, 1.0)).xyz;
    float nearZ = -ubo.uNear;
    bool sAdelante = s.z <= nearZ;
    bool eAdelante = e.z <= nearZ;
    if (!sAdelante && !eAdelante) {
        // Enteramente detras de la camara: vertice degenerado fuera de pantalla
        // (z fuera del rango de profundidad: se descarta por recorte).
        gl_Position = vec4(0.0, 0.0, 2.0, 1.0);
        vColor = aColor;
        vWorldPosition = worldS;
        return;
    }
    if (!sAdelante) {
        float t = (nearZ - s.z) / (e.z - s.z);
        s = mix(s, e, t);
        worldS = mix(worldS, worldE, t);
    }
    if (!eAdelante) {
        float t = (nearZ - e.z) / (s.z - e.z);
        e = mix(e, s, t);
        worldE = mix(worldE, worldS, t);
    }

    // 2. A clip space y de ahi a NDC.
    vec4 clipS = ubo.uProjection * vec4(s, 1.0);
    vec4 clipE = ubo.uProjection * vec4(e, 1.0);
    vec2 ndcS = clipS.xy / clipS.w;
    vec2 ndcE = clipE.xy / clipE.w;

    // 3. Perpendicular en pixeles: se trabaja en pixeles y no en NDC para que el
    //    ancho sea isotropico (en NDC los ejes X e Y tienen escalas distintas).
    vec2 px = 0.5 * ubo.uViewport.xy;
    vec2 dir = (ndcE - ndcS) * px;
    float largo = length(dir);
    vec2 normal = (largo > 1e-6) ? vec2(-dir.y, dir.x) / largo : vec2(0.0);
    vec2 offsetPx = normal * (aSide * ubo.uWidth * 0.5);

    // 4. Interpola el punto sobre el segmento y le suma el desplazamiento.
    vec4 clip = mix(clipS, clipE, aAlong);
    clip.xy += (offsetPx / px) * clip.w;
    gl_Position = clip;
    vColor = aColor;
    vWorldPosition = mix(worldS, worldE, aAlong);
}
