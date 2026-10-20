/*
    FunshiEngineGL - Motor de juegos 3D con API grafica intercambiable
    Copyright 2026 Gianfranco Ivan Enrique

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.

    SPDX-License-Identifier: Apache-2.0
*/
#ifndef CONTRATORENDER_H
#define CONTRATORENDER_H

// Contrato de render del motor.
//
// El motor no depende de ninguna API grafica: define QUE necesita (recursos,
// estados, uniformes) y deja COMO se resuelve en manos del backend. Este
// header es la frontera entre las dos cosas: el backend que se implemente
// tiene que respetar los nombres de los uniformes, el layout de los vertices y
// las convenciones de espacio que aqui se declaran.
//
// El texto GLSL (o SPIR-V, o el dialecto que use la API) vive en el backend:
// cada API tiene su propio dialecto y sus propias reglas de empaquetado de
// uniformes, asi que el fuente no puede ser del motor.

#include <cstddef>

namespace Rendering {
namespace Contrato {

// Tipo de programa que el motor pide al backend. El backend decide como lo
// construye (dialecto, herramientas de compilacion, formato binario).
enum class ShaderKind {
    Malla,    // mallas con material e iluminacion
    Lineas,   // batch de lineas con ancho en pixeles
    Sprites,  // quad texturizado (sprites y tilemaps)
    Cielo,    // degradado de fondo a pantalla completa
    Cubemap   // cubemap del cielo (skybox)
};

// Numero maximo de luces que el motor envia al shader. El backend tiene que
// reservar esta cantidad en su bloque de uniformes.
constexpr int kMaxLuces = 8;

// Locaciones de los atributos de vertice de una malla.
enum class Atributo : unsigned {
    Posicion = 0,
    Normal = 1,
    Uv = 2,
    Tangente = 3,
    Bitangente = 4
};

// Locaciones de los atributos de vertice del batch de lineas. El formato es
// el de LineBuilder: 12 floats por vertice.
enum class AtributoLinea : unsigned {
    Inicio = 0,
    Fin = 1,
    Lado = 2,
    Avance = 3,
    Color = 4
};

// Nombres de los uniformes. El backend los expone con estos nombres exactos:
// son el unico punto de contacto entre el motor y el shader.
//
// Los uniformes de luz y material son vectores de kMaxLuces elementos; el
// motor los escribe con el nombre del array y el indice entre corchetes
// (por ejemplo "uLightTypes[0]"), que es la forma en que el motor los nombra
// y la que el backend tiene que reconocer.
namespace Uniforme {

// Transformaciones y camara (programa de malla).
constexpr const char* kModel = "uModel";
constexpr const char* kView = "uView";
constexpr const char* kProjection = "uProjection";
constexpr const char* kNormalMatrix = "uNormalMatrix";
constexpr const char* kCameraPosition = "uCameraPosition";
constexpr const char* kDistanceFadeStart = "uDistanceFadeStart";
constexpr const char* kDistanceFadeEnd = "uDistanceFadeEnd";

// Iluminacion.
constexpr const char* kGlobalAmbient = "uGlobalAmbient";
constexpr const char* kLightCount = "uLightCount";
constexpr const char* kLightTypes = "uLightTypes";
constexpr const char* kLightWorldPos = "uLightWorldPos";
constexpr const char* kLightDir = "uLightDir";
constexpr const char* kLightAmbient = "uLightAmbient";
constexpr const char* kLightDiffuse = "uLightDiffuse";
constexpr const char* kLightSpecular = "uLightSpecular";
constexpr const char* kLightConstant = "uLightConstant";
constexpr const char* kLightLinear = "uLightLinear";
constexpr const char* kLightQuadratic = "uLightQuadratic";
constexpr const char* kSpotCutoffCos = "uSpotCutoffCos";

// Material.
constexpr const char* kMaterialAmbient = "uMaterialAmbient";
constexpr const char* kMaterialDiffuse = "uMaterialDiffuse";
constexpr const char* kMaterialSpecular = "uMaterialSpecular";
constexpr const char* kMaterialEmission = "uMaterialEmission";
constexpr const char* kMaterialShininess = "uMaterialShininess";

// Texturas del material. Los cuatro sampler ocupan la unidad de texture que
// indique el motor (0 difusa, 1 especular, 2 emision, 3 normal).
constexpr const char* kDiffuseTex = "uDiffuseTex";
constexpr const char* kSpecularTex = "uSpecularTex";
constexpr const char* kEmissionTex = "uEmissionTex";
constexpr const char* kNormalTex = "uNormalTex";
constexpr const char* kUseTexture = "uUseTexture";
constexpr const char* kUseSpecularMap = "uUseSpecularMap";
constexpr const char* kUseEmissionMap = "uUseEmissionMap";
constexpr const char* kUseNormalMap = "uUseNormalMap";

// Batch de lineas.
constexpr const char* kViewport = "uViewport";
constexpr const char* kWidth = "uWidth";
constexpr const char* kNear = "uNear";

// Sprites y tilemaps.
constexpr const char* kUvRect = "uUvRect";
constexpr const char* kTexture = "uTexture";
constexpr const char* kTint = "uTint";

// Cielo degradado.
constexpr const char* kColorTop = "uColorTop";
constexpr const char* kColorBottom = "uColorBottom";
constexpr const char* kInvViewProj = "uInvViewProj";
constexpr const char* kCamPos = "uCamPos";
constexpr const char* kTransicion = "uTransicion";

// Cubemap del cielo.
constexpr const char* kSkybox = "uSkybox";

} // namespace Uniforme

// Tipo de un uniform del contrato. El backend decide donde lo ubica (un
// bloque de uniformes, un push constant, un descriptor), pero el motor solo
// necesita saber el tipo para escribir el valor.
enum class TipoUniforme {
    Mat4,
    Mat3,
    Vec3,
    Vec4,
    Float,
    Int,
    Sampler2D,
    SamplerCube
};

// Uniform que el motor escribe y que el backend tiene que exponer.
struct UniformeRequerido {
    const char* nombre;
    TipoUniforme tipo;
    // Cantidad de elementos: 1 para un escalar o un vector, kMaxLuces para
    // los arrays de luces. El motor escribe los arrays elemento por elemento
    // con el nombre del array y el indice entre corchetes ("uLightTypes[0]"),
    // que es la forma en que el backend tiene que reconocerlos.
    int elementos;
};

// Uniformes que cada tipo de programa tiene que exponer.
//
// Esta tabla es el contrato de render: el motor escribe exactamente estos
// nombres y el backend los resuelve. Lo que el programa hace con ellos (el
// modelo de iluminacion, el difuminado radial, el recorte de la textura) es
// responsabilidad del backend, asi que cada tipo declara aqui su semantica
// para que cualquier implementacion la respete.
namespace Detalle {

// Malla: transformaciones del modelo, camara y matrices de vista y
// proyeccion; difuminado por distancia a la camara; modelo de iluminacion con
// hasta kMaxLuces luces (0 direccional, 1 de punto, 2 spot) y sus terminos de
// atenuacion; material con ambiente, difuso, especular, emision y brillo; y
// los cuatro mapas de textura (difusa, especular, emision y normal) con sus
// banderas de activacion. Las banderas valen 0 cuando el mapa no esta.
inline constexpr UniformeRequerido kMalla[] = {
    {Uniforme::kModel, TipoUniforme::Mat4, 1},
    {Uniforme::kView, TipoUniforme::Mat4, 1},
    {Uniforme::kProjection, TipoUniforme::Mat4, 1},
    {Uniforme::kNormalMatrix, TipoUniforme::Mat3, 1},
    {Uniforme::kCameraPosition, TipoUniforme::Vec3, 1},
    {Uniforme::kDistanceFadeStart, TipoUniforme::Float, 1},
    {Uniforme::kDistanceFadeEnd, TipoUniforme::Float, 1},
    {Uniforme::kGlobalAmbient, TipoUniforme::Vec3, 1},
    {Uniforme::kLightCount, TipoUniforme::Int, 1},
    {Uniforme::kLightTypes, TipoUniforme::Int, kMaxLuces},
    {Uniforme::kLightWorldPos, TipoUniforme::Vec3, kMaxLuces},
    {Uniforme::kLightDir, TipoUniforme::Vec3, kMaxLuces},
    {Uniforme::kLightAmbient, TipoUniforme::Vec3, kMaxLuces},
    {Uniforme::kLightDiffuse, TipoUniforme::Vec3, kMaxLuces},
    {Uniforme::kLightSpecular, TipoUniforme::Vec3, kMaxLuces},
    {Uniforme::kLightConstant, TipoUniforme::Float, kMaxLuces},
    {Uniforme::kLightLinear, TipoUniforme::Float, kMaxLuces},
    {Uniforme::kLightQuadratic, TipoUniforme::Float, kMaxLuces},
    {Uniforme::kSpotCutoffCos, TipoUniforme::Float, kMaxLuces},
    {Uniforme::kMaterialAmbient, TipoUniforme::Vec4, 1},
    {Uniforme::kMaterialDiffuse, TipoUniforme::Vec4, 1},
    {Uniforme::kMaterialSpecular, TipoUniforme::Vec4, 1},
    {Uniforme::kMaterialEmission, TipoUniforme::Vec4, 1},
    {Uniforme::kMaterialShininess, TipoUniforme::Float, 1},
    {Uniforme::kDiffuseTex, TipoUniforme::Sampler2D, 1},
    {Uniforme::kSpecularTex, TipoUniforme::Sampler2D, 1},
    {Uniforme::kEmissionTex, TipoUniforme::Sampler2D, 1},
    {Uniforme::kNormalTex, TipoUniforme::Sampler2D, 1},
    {Uniforme::kUseTexture, TipoUniforme::Int, 1},
    {Uniforme::kUseSpecularMap, TipoUniforme::Int, 1},
    {Uniforme::kUseEmissionMap, TipoUniforme::Int, 1},
    {Uniforme::kUseNormalMap, TipoUniforme::Int, 1},
};

// Lineas: el ancho se mide en pixeles, no en unidades de mundo, asi que el
// programa necesita el tamano del viewport y la distancia al plano cercano
// para convertir el desplazamiento lateral en una pantalla de ancho
// constante. El difuminado es radial: la linea se vuelve transparente con la
// distancia a la camara y se descarta por completo pasado el radio final.
inline constexpr UniformeRequerido kLineas[] = {
    {Uniforme::kModel, TipoUniforme::Mat4, 1},
    {Uniforme::kView, TipoUniforme::Mat4, 1},
    {Uniforme::kProjection, TipoUniforme::Mat4, 1},
    {Uniforme::kViewport, TipoUniforme::Vec4, 1},
    {Uniforme::kWidth, TipoUniforme::Float, 1},
    {Uniforme::kNear, TipoUniforme::Float, 1},
    {Uniforme::kCameraPosition, TipoUniforme::Vec3, 1},
    {Uniforme::kDistanceFadeStart, TipoUniforme::Float, 1},
    {Uniforme::kDistanceFadeEnd, TipoUniforme::Float, 1},
};

// Sprites y tilemaps: recorte de la region de textura por vertice (el rect
// uv), tinte multiplicativo, y el sampler con la bandera que indica si hay
// textura o hay que usar el color plano.
inline constexpr UniformeRequerido kSprites[] = {
    {Uniforme::kModel, TipoUniforme::Mat4, 1},
    {Uniforme::kView, TipoUniforme::Mat4, 1},
    {Uniforme::kProjection, TipoUniforme::Mat4, 1},
    {Uniforme::kUvRect, TipoUniforme::Vec4, 1},
    {Uniforme::kTint, TipoUniforme::Vec4, 1},
    {Uniforme::kTexture, TipoUniforme::Sampler2D, 1},
    {Uniforme::kUseTexture, TipoUniforme::Int, 1},
};

// Cielo: degradado de dos colores (superior e inferior) proyectado sobre la
// pantalla, con la view-projection inversa para reconstruir la direccion de
// cada fragmento, la posicion de la camara y el factor de transicion entre
// el cielo y el fondo plano.
inline constexpr UniformeRequerido kCielo[] = {
    {Uniforme::kColorTop, TipoUniforme::Vec3, 1},
    {Uniforme::kColorBottom, TipoUniforme::Vec3, 1},
    {Uniforme::kInvViewProj, TipoUniforme::Mat4, 1},
    {Uniforme::kCamPos, TipoUniforme::Vec3, 1},
    {Uniforme::kTransicion, TipoUniforme::Float, 1},
};

// Cubemap del cielo: la caja va centrada en la camara y se dibuja en el plano
// lejano (la profundidad se fuerza al maximo), asi que solo necesita la vista
// sin traslacion, la proyeccion y el sampler del cubemap.
inline constexpr UniformeRequerido kCubemap[] = {
    {Uniforme::kView, TipoUniforme::Mat4, 1},
    {Uniforme::kProjection, TipoUniforme::Mat4, 1},
    {Uniforme::kSkybox, TipoUniforme::SamplerCube, 1},
};

} // namespace Detalle

// Uniformes que el tipo de programa indicado tiene que exponer. Devuelve
// nullptr si el tipo no existe.
inline const UniformeRequerido* uniformesDe(ShaderKind kind, int& cantidad) {
    switch (kind) {
        case ShaderKind::Malla:
            cantidad = static_cast<int>(sizeof(Detalle::kMalla) /
                                        sizeof(Detalle::kMalla[0]));
            return Detalle::kMalla;
        case ShaderKind::Lineas:
            cantidad = static_cast<int>(sizeof(Detalle::kLineas) /
                                        sizeof(Detalle::kLineas[0]));
            return Detalle::kLineas;
        case ShaderKind::Sprites:
            cantidad = static_cast<int>(sizeof(Detalle::kSprites) /
                                        sizeof(Detalle::kSprites[0]));
            return Detalle::kSprites;
        case ShaderKind::Cielo:
            cantidad = static_cast<int>(sizeof(Detalle::kCielo) /
                                        sizeof(Detalle::kCielo[0]));
            return Detalle::kCielo;
        case ShaderKind::Cubemap:
            cantidad = static_cast<int>(sizeof(Detalle::kCubemap) /
                                        sizeof(Detalle::kCubemap[0]));
            return Detalle::kCubemap;
    }
    cantidad = 0;
    return nullptr;
}

// Nombre del tipo de programa, para logs.
inline const char* nombreDe(ShaderKind kind) {
    switch (kind) {
        case ShaderKind::Malla:
            return "malla";
        case ShaderKind::Lineas:
            return "lineas";
        case ShaderKind::Sprites:
            return "sprites";
        case ShaderKind::Cielo:
            return "cielo";
        case ShaderKind::Cubemap:
            return "cubemap";
    }
    return "desconocido";
}

// Numero de bytes que ocupa un float en los datos que el motor sube a la GPU.
constexpr std::size_t kBytesPorFloat = 4;

// Estructura de un vertice de malla en memoria (lo que entrega el AssetManager).
// El backend decide como lo empaqueta en su buffer; el motor solo garantiza
// el orden de los campos.
struct VerticeMalla {
    float posicion[3];
    float normal[3];
    float uv[2];
    float tangente[3];
    float bitangente[3];
};

// Estructura de un vertice del batch de lineas (el formato de LineBuilder).
struct VerticeLinea {
    float inicio[3];
    float fin[3];
    float lado;
    float avance;
    float color[4];
};

} // namespace Contrato
} // namespace Rendering

#endif
