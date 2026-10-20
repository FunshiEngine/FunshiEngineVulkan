/*
    FunshiEngineGL - Motor de juegos 3D con API grafica intercambiable e ImGui
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
// Pruebas headless del backend de Vulkan.
//
// El backend se comprueba sin ventana: se crean la instancia y el dispositivo
// sin superficie y se dibuja sobre un render target fuera de pantalla. Al
// final se leen los pixeles de la imagen, asi que la comprobacion es
// determinista: no depende de como se vea la pantalla sino de lo que el
// backend escribio en la imagen.
//
// Que no haya ventana no cambia lo que se prueba: el dibujo, la conversion de
// la proyeccion, el bloque de uniformes, los descriptores y los pipelines son
// los mismos en el swapchain y en un render target.
//
// Si la maquina no tiene Vulkan (o no hay un dispositivo utilizable) el test
// sale con 77 (skipped) y la suite sigue verde.

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "../FunshiEngineGL/src/Rendering/BackendVulkan/BackendVulkan.h"

namespace {
int total = 0;
int fallos = 0;

#define CHECK(cond, msg)                                                      \
    do {                                                                      \
        ++total;                                                              \
        if (!(cond)) {                                                        \
            ++fallos;                                                         \
            std::cout << "FALLO: " << msg << " (linea " << __LINE__ << ")"    \
                      << std::endl;                                           \
        }                                                                     \
    } while (0)

namespace Contrato = Rendering::Contrato;
namespace Backend = Rendering::Backend;
namespace Vulkan = Rendering::Backend::Vulkan;

using Backend::Handle;

// Escribe un uniforme y comprueba que el backend lo expone (una location
// negativa significa que el programa no declara ese uniforme).
void escribirMatriz(Vulkan::BackendVulkan& backend, Handle programa,
                    const char* nombre, const glm::mat4& valor) {
    const int location = backend.uniformLocation(programa, nombre);
    CHECK(location >= 0, std::string("el programa de malla expone el uniforme ") +
                             nombre);
    if (location >= 0) backend.setUniformMat4(location, valor);
}

// Nombre de un elemento de un array de uniformes: el contrato expone el
// nombre del array y el indice va entre corchetes.
std::string nombreDeElemento(const char* nombre, int indice) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%s[%d]", nombre, indice);
    return std::string(buffer);
}

} // namespace

int main() {
    Vulkan::BackendVulkan backend;

    // Sin ventana: no hay superficie y el trabajo se hace sobre render
    // targets.
    if (!backend.init(nullptr)) {
        std::cout << "SKIP: no hay un dispositivo Vulkan utilizable en la "
                     "maquina."
                  << std::endl;
        return 77;
    }

    CHECK(backend.available(),
          "el backend queda disponible despues de inicializar");

    constexpr int kAncho = 64;
    constexpr int kAlto = 64;

    Handle objetivo = backend.createRenderTarget(kAncho, kAlto);
    CHECK(objetivo != Backend::kInvalidHandle, "se crea el render target");

    backend.beginFrame();
    backend.setViewport(0, 0, kAncho, kAlto);
    backend.applyBaseState();
    backend.bindRenderTarget(objetivo);

    const float negro[3] = {0.0f, 0.0f, 0.0f};
    backend.clearScreen(negro);

    // Triangulo que cubre la imagen entera.
    const float posiciones[9] = {-1.0f, -1.0f, 0.0f, 3.0f, -1.0f, 0.0f,
                                 -1.0f, 3.0f, 0.0f};
    const float normales[9] = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
                               0.0f, 0.0f, 1.0f};
    const float uvs[6] = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f};
    const float tangentes[9] = {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                                1.0f, 0.0f, 0.0f};
    const float bitangentes[9] = {0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
                                  0.0f, 1.0f, 0.0f};
    const unsigned int indices[3] = {0, 1, 2};

    Backend::MeshData datos{};
    datos.vertices = posiciones;
    datos.vertexCount = 3;
    datos.normals = normales;
    datos.normalCount = 3;
    datos.uvs = uvs;
    datos.uvCount = 3;
    datos.tangents = tangentes;
    datos.tangentCount = 3;
    datos.bitangents = bitangentes;
    datos.bitangentCount = 3;
    datos.indices = indices;
    datos.indexCount = 3;

    Handle malla = backend.createMesh(datos);
    CHECK(malla != Backend::kInvalidHandle, "se crea la malla");

    Handle programa = backend.createProgram(Contrato::ShaderKind::Malla);
    CHECK(programa != Backend::kInvalidHandle, "se crea el programa de malla");
    if (programa == Backend::kInvalidHandle || malla == Backend::kInvalidHandle) {
        std::cout << "FALLOS: " << fallos << ": " << total
                  << " comprobaciones" << std::endl;
        return 1;
    }

    backend.useProgram(programa);

    // Transformaciones identidad: con la proyeccion identidad el triangulo
    // cubre el clip space entero.
    const glm::mat4 identidad(1.0f);
    escribirMatriz(backend, programa, Contrato::Uniforme::kModel, identidad);
    escribirMatriz(backend, programa, Contrato::Uniforme::kView, identidad);
    escribirMatriz(backend, programa, Contrato::Uniforme::kProjection, identidad);

    const int locationNormal = backend.uniformLocation(
        programa, Contrato::Uniforme::kNormalMatrix);
    CHECK(locationNormal >= 0,
          "el programa de malla expone la matriz de normales");
    if (locationNormal >= 0)
        backend.setUniformMat3(locationNormal, glm::mat3(1.0f));

    const int locationCamara = backend.uniformLocation(
        programa, Contrato::Uniforme::kCameraPosition);
    CHECK(locationCamara >= 0,
          "el programa de malla expone la posicion de la camara");
    if (locationCamara >= 0)
        backend.setUniformVec3(locationCamara, glm::vec3(0.0f, 0.0f, 0.0f));

    const int locationFadeInicio = backend.uniformLocation(
        programa, Contrato::Uniforme::kDistanceFadeStart);
    const int locationFadeFin = backend.uniformLocation(
        programa, Contrato::Uniforme::kDistanceFadeEnd);
    CHECK(locationFadeInicio >= 0 && locationFadeFin >= 0,
          "el programa de malla expone el rango de difuminado");
    if (locationFadeInicio >= 0)
        backend.setUniformFloat(locationFadeInicio, 0.0f);
    if (locationFadeFin >= 0) backend.setUniformFloat(locationFadeFin, 1000.0f);

    // Iluminacion: solo el ambiente global, para que el color final sea
    // exactamente el del material (sin luces ni especular).
    const int locationAmbiente = backend.uniformLocation(
        programa, Contrato::Uniforme::kGlobalAmbient);
    CHECK(locationAmbiente >= 0, "el programa de malla expone el ambiente global");
    if (locationAmbiente >= 0)
        backend.setUniformVec3(locationAmbiente, glm::vec3(1.0f, 1.0f, 1.0f));

    const int locationLuces = backend.uniformLocation(
        programa, Contrato::Uniforme::kLightCount);
    CHECK(locationLuces >= 0, "el programa de malla expone la cantidad de luces");
    if (locationLuces >= 0) backend.setUniformInt(locationLuces, 0);

    const float ambienteMaterial[4] = {0.25f, 0.5f, 0.75f, 1.0f};
    const int locationMaterialAmbiente = backend.uniformLocation(
        programa, Contrato::Uniforme::kMaterialAmbient);
    CHECK(locationMaterialAmbiente >= 0,
          "el programa de malla expone el ambiente del material");
    if (locationMaterialAmbiente >= 0)
        backend.setUniformVec4(locationMaterialAmbiente, ambienteMaterial);

    const float difusoMaterial[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    const int locationMaterialDifuso = backend.uniformLocation(
        programa, Contrato::Uniforme::kMaterialDiffuse);
    CHECK(locationMaterialDifuso >= 0,
          "el programa de malla expone el difuso del material");
    if (locationMaterialDifuso >= 0)
        backend.setUniformVec4(locationMaterialDifuso, difusoMaterial);

    const float especularMaterial[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    const int locationMaterialEspecular = backend.uniformLocation(
        programa, Contrato::Uniforme::kMaterialSpecular);
    if (locationMaterialEspecular >= 0)
        backend.setUniformVec4(locationMaterialEspecular, especularMaterial);

    const float emisionMaterial[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    const int locationMaterialEmision = backend.uniformLocation(
        programa, Contrato::Uniforme::kMaterialEmission);
    if (locationMaterialEmision >= 0)
        backend.setUniformVec4(locationMaterialEmision, emisionMaterial);

    const int locationBrillo = backend.uniformLocation(
        programa, Contrato::Uniforme::kMaterialShininess);
    if (locationBrillo >= 0) backend.setUniformFloat(locationBrillo, 0.0f);

    // Sin texturas: el material se colorea solo con los uniformes.
    const char* banderas[] = {Contrato::Uniforme::kUseTexture,
                              Contrato::Uniforme::kUseSpecularMap,
                              Contrato::Uniforme::kUseEmissionMap,
                              Contrato::Uniforme::kUseNormalMap};
    for (const char* bandera : banderas) {
        const int location = backend.uniformLocation(programa, bandera);
        CHECK(location >= 0,
              std::string("el programa de malla expone la bandera ") + bandera);
        if (location >= 0) backend.setUniformInt(location, 0);
    }

    backend.drawMesh(malla, 3);
    backend.endFrame();

    std::vector<unsigned char> pixeles;
    const bool leido = backend.leerPixeles(objetivo, pixeles);
    CHECK(leido, "se leen los pixeles del render target");

    const std::size_t esperadoTamano =
        static_cast<std::size_t>(kAncho) * static_cast<std::size_t>(kAlto) * 4;
    CHECK(pixeles.size() == esperadoTamano,
          "la imagen leida tiene el tamano esperado");

    if (pixeles.size() == esperadoTamano) {
        // El ambiente global es blanco, no hay luces y no hay emision: el
        // pixel tiene que ser el ambiente del material.
        const int esperado[3] = {64, 128, 191};
        for (int y = 0; y < kAlto; y += 8) {
            for (int x = 0; x < kAncho; x += 8) {
                const unsigned char* pixel =
                    &pixeles[(static_cast<std::size_t>(y) * kAncho + x) * 4];
                for (int canal = 0; canal < 3; ++canal) {
                    CHECK(std::abs(static_cast<int>(pixel[canal]) -
                                   esperado[canal]) <= 2,
                          "el pixel dibujado tiene el color del material");
                }
            }
        }
    }

    // La matriz de normales se lee como la escribio el backend. Con una luz
    // direccional el color depende de la normal que calcula el shader, asi
    // que esta comprobacion distingue la matriz bien escrita de la que llega
    // con las columnas corridas.
    {
        backend.beginFrame();
        backend.setViewport(0, 0, kAncho, kAlto);
        backend.applyBaseState();
        backend.bindRenderTarget(objetivo);
        backend.clearScreen(negro);

        // Rotacion sobre el eje Z: la normal del triangulo (0, 0, 1) no
        // cambia, asi que el shader tiene que ver la misma normal que con la
        // matriz identidad.
        const glm::mat3 rotacionZ(0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f,
                                   0.0f, 0.0f, 1.0f);
        if (locationNormal >= 0)
            backend.setUniformMat3(locationNormal, rotacionZ);

        // Sin ambiente: el color sale solo de la luz, y la luz solo ilumina si
        // la normal es la que dice la matriz.
        if (locationAmbiente >= 0)
            backend.setUniformVec3(locationAmbiente, glm::vec3(0.0f));
        const float ambienteNegro[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        if (locationMaterialAmbiente >= 0)
            backend.setUniformVec4(locationMaterialAmbiente, ambienteNegro);

        // Luz direccional que apunta hacia +Z, que es la normal del
        // triangulo: el difuso tiene que ser pleno.
        const int locationTipoLuz = backend.uniformLocation(
            programa, nombreDeElemento(Contrato::Uniforme::kLightTypes, 0).c_str());
        const int locationPosLuz = backend.uniformLocation(
            programa,
            nombreDeElemento(Contrato::Uniforme::kLightWorldPos, 0).c_str());
        const int locationAmbienteLuz = backend.uniformLocation(
            programa,
            nombreDeElemento(Contrato::Uniforme::kLightAmbient, 0).c_str());
        const int locationDifusoLuz = backend.uniformLocation(
            programa,
            nombreDeElemento(Contrato::Uniforme::kLightDiffuse, 0).c_str());
        CHECK(locationTipoLuz >= 0 && locationPosLuz >= 0 &&
                  locationAmbienteLuz >= 0 && locationDifusoLuz >= 0,
              "el programa de malla expone los uniformes de la luz");
        if (locationTipoLuz >= 0) backend.setUniformInt(locationTipoLuz, 0);
        if (locationPosLuz >= 0)
            backend.setUniformVec3(locationPosLuz, glm::vec3(0.0f, 0.0f, -1.0f));
        if (locationAmbienteLuz >= 0)
            backend.setUniformVec3(locationAmbienteLuz, glm::vec3(0.0f));
        if (locationDifusoLuz >= 0)
            backend.setUniformVec3(locationDifusoLuz, glm::vec3(1.0f));
        if (locationLuces >= 0) backend.setUniformInt(locationLuces, 1);

        backend.drawMesh(malla, 3);
        backend.endFrame();

        std::vector<unsigned char> pixelesLuz;
        const bool leidoLuz = backend.leerPixeles(objetivo, pixelesLuz);
        CHECK(leidoLuz, "se leen los pixeles del render target con luz");

        if (leidoLuz && pixelesLuz.size() == esperadoTamano) {
            // La normal es (0, 0, 1) y la luz apunta a (0, 0, 1): el difuso
            // es 1 y el pixel queda blanco.
            const int esperadoConLuz[3] = {255, 255, 255};
            for (int y = 0; y < kAlto; y += 8) {
                for (int x = 0; x < kAncho; x += 8) {
                    const unsigned char* pixel =
                        &pixelesLuz[(static_cast<std::size_t>(y) * kAncho + x) *
                                    4];
                    for (int canal = 0; canal < 3; ++canal) {
                        CHECK(std::abs(static_cast<int>(pixel[canal]) -
                                       esperadoConLuz[canal]) <= 2,
                              "la luz ilumina con la normal que dice la matriz");
                    }
                }
            }
        }
    }

    // El batch de lineas se puede reescribir.
    //
    // El batch se reescribe en cada frame y a menudo con la misma cantidad de
    // vertices (la grilla cambia de tamano, los gizmos no). Su memoria tiene
    // que ser visible para la CPU y estar mapeada, porque la actualizacion
    // escribe en ella. Con memoria DEVICE_LOCAL el mapeo no es valido: el
    // batch se queda con la geometria inicial y el log se llena de fallos.
    {
        Handle programaLineas =
            backend.createProgram(Contrato::ShaderKind::Lineas);
        CHECK(programaLineas != Backend::kInvalidHandle,
              "se crea el programa de lineas");

        if (programaLineas != Backend::kInvalidHandle) {
            // Un segmento son dos triangulos: cada vertice lleva el inicio y
            // el fin del segmento, el lado (+1/-1), el avance a lo largo del
            // segmento y el color (12 floats por vertice).
            auto llenarSegmento = [](float* destino, float ax, float ay,
                                     float bx, float by) {
                const float lados[6] = {-1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f};
                const float avances[6] = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f};
                for (int i = 0; i < 6; ++i) {
                    float* vertice = destino + i * 12;
                    vertice[0] = ax;
                    vertice[1] = ay;
                    vertice[2] = -1.0f;
                    vertice[3] = bx;
                    vertice[4] = by;
                    vertice[5] = -1.0f;
                    vertice[6] = lados[i];
                    vertice[7] = avances[i];
                    vertice[8] = 1.0f;
                    vertice[9] = 1.0f;
                    vertice[10] = 1.0f;
                    vertice[11] = 1.0f;
                }
            };

            // Dibuja el batch en el render target y devuelve la imagen.
            auto renderizarLineas = [&](Handle batch, unsigned int vertices) {
                backend.beginFrame();
                backend.setViewport(0, 0, kAncho, kAlto);
                backend.applyBaseState();
                backend.bindRenderTarget(objetivo);
                backend.clearScreen(negro);
                backend.useProgram(programaLineas);

                const glm::mat4 identidad(1.0f);
                escribirMatriz(backend, programaLineas, Contrato::Uniforme::kModel,
                               identidad);
                escribirMatriz(backend, programaLineas, Contrato::Uniforme::kView,
                               identidad);
                escribirMatriz(backend, programaLineas,
                               Contrato::Uniforme::kProjection, identidad);

                const int locationViewport =
                    backend.uniformLocation(programaLineas,
                                            Contrato::Uniforme::kViewport);
                CHECK(locationViewport >= 0,
                      "el programa de lineas expone el viewport");
                if (locationViewport >= 0) {
                    const float viewport[4] = {static_cast<float>(kAncho),
                                               static_cast<float>(kAlto), 0.0f,
                                               0.0f};
                    backend.setUniformVec4(locationViewport, viewport);
                }
                const int locationWidth =
                    backend.uniformLocation(programaLineas, Contrato::Uniforme::kWidth);
                if (locationWidth >= 0) backend.setUniformFloat(locationWidth, 2.0f);
                const int locationNear =
                    backend.uniformLocation(programaLineas, Contrato::Uniforme::kNear);
                if (locationNear >= 0) backend.setUniformFloat(locationNear, 0.1f);
                const int locationCamara = backend.uniformLocation(
                    programaLineas, Contrato::Uniforme::kCameraPosition);
                if (locationCamara >= 0)
                    backend.setUniformVec3(locationCamara, glm::vec3(0.0f, 0.0f, 0.0f));
                const int locationFadeInicio = backend.uniformLocation(
                    programaLineas, Contrato::Uniforme::kDistanceFadeStart);
                const int locationFadeFin = backend.uniformLocation(
                    programaLineas, Contrato::Uniforme::kDistanceFadeEnd);
                if (locationFadeInicio >= 0)
                    backend.setUniformFloat(locationFadeInicio, 1000.0f);
                if (locationFadeFin >= 0)
                    backend.setUniformFloat(locationFadeFin, 2000.0f);

                backend.drawLineBatch(batch, vertices);
                backend.endFrame();

                std::vector<unsigned char> pixeles;
                const bool leido = backend.leerPixeles(objetivo, pixeles);
                CHECK(leido, "se leen los pixeles del render target con lineas");
                return pixeles;
            };

            float vertices[6 * 12];
            llenarSegmento(vertices, -0.8f, -0.8f, 0.8f, 0.8f);

            Handle batch = backend.createLineBatch(vertices, 6);
            CHECK(batch != Backend::kInvalidHandle, "se crea el batch de lineas");

            if (batch != Backend::kInvalidHandle) {
                const std::vector<unsigned char> primera =
                    renderizarLineas(batch, 6);

                // Se reescribe el batch con la misma cantidad de vertices y
                // otra geometria (la diagonal contraria). Si la actualizacion no
                // llega a la GPU, la segunda imagen es igual que la primera.
                llenarSegmento(vertices, -0.8f, 0.8f, 0.8f, -0.8f);
                backend.updateLineBatch(batch, vertices, 6);

                const std::vector<unsigned char> segunda =
                    renderizarLineas(batch, 6);

                CHECK(primera.size() == esperadoTamano,
                      "la imagen de la primera diagonal tiene el tamano esperado");
                CHECK(segunda.size() == esperadoTamano,
                      "la imagen de la segunda diagonal tiene el tamano esperado");

                if (primera.size() == esperadoTamano &&
                    segunda.size() == esperadoTamano) {
                    int iluminados = 0;
                    int distintos = 0;
                    for (std::size_t i = 0; i < esperadoTamano; i += 4) {
                        if (primera[i] > 8 || primera[i + 1] > 8 ||
                            primera[i + 2] > 8) {
                            ++iluminados;
                        }
                        if (primera[i] != segunda[i] ||
                            primera[i + 1] != segunda[i + 1] ||
                            primera[i + 2] != segunda[i + 2]) {
                            ++distintos;
                        }
                    }
                    CHECK(iluminados > 0, "la diagonal se dibuja en la imagen");
                    CHECK(distintos > 10,
                          "al reescribir el batch se dibuja otra geometria");
                }

                backend.destroyLineBatch(batch);
            }
            backend.destroyProgram(programaLineas);
        }
    }

    backend.destroyMesh(malla);
    backend.destroyProgram(programa);
    backend.destroyRenderTarget(objetivo);

    std::cout << (fallos == 0 ? "OK" : "FALLOS") << ": " << total
              << " comprobaciones" << std::endl;
    return fallos == 0 ? 0 : 1;
}
