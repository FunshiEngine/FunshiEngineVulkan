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

    backend.destroyMesh(malla);
    backend.destroyProgram(programa);
    backend.destroyRenderTarget(objetivo);

    std::cout << (fallos == 0 ? "OK" : "FALLOS") << ": " << total
              << " comprobaciones" << std::endl;
    return fallos == 0 ? 0 : 1;
}
