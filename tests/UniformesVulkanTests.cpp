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
// Pruebas headless de la escritura de matrices en el bloque de uniformes.
//
// El bloque de uniformes es un UBO en std140: la matriz 4x4 ocupa 64 bytes
// (cuatro columnas de 16) y la 3x3 ocupa 48 bytes (tres columnas de 16, con
// 12 bytes de padding que el shader lee como ceros). Escribir de mas en ese
// bloque no lo rompe por la via de Vulkan: pisa la memoria del proceso, y el
// fallo aparece mucho despues y en otro sitio (el mapa de programas del
// backend queda con basura y la busqueda siguiente revienta).
//
// Por eso el test mide el efecto exacto: escribe la matriz en un bloque con
// un canario detras y comprueba que el canario quedo intacto. Es la unica
// forma de que la regresion quede cubierta sin depender de un sanitizer.
//
// No hay GPU en este test: solo se ejercita la escritura en el bloque.

#include <cstddef>
#include <iostream>

#include "Rendering/BackendVulkan/BackendVulkanUniformes.h"

namespace {

int fallos = 0;
int total = 0;

void comprobar(bool condicion, const char* mensaje) {
    ++total;
    if (!condicion) {
        ++fallos;
        std::cout << "FALLO: " << mensaje << std::endl;
    }
}

// Floats de canario que se ponen detras del bloque para detectar una
// escritura que se pasa del limite. El buffer se reserva mas grande que el
// bloque declarado para que una escritura que se pasa caiga en la memoria de
// la prueba y no reviente la pil: el fallo tiene que ser una comprobacion
// fallida, no un crash.
constexpr int kFloatsCanario = 8;
constexpr int kFloatsHolgura = 64;
constexpr float kPatronCanario = -12345.0f;

} // namespace

int main() {
    using Rendering::Backend::Vulkan::escribirMatriz3;
    using Rendering::Backend::Vulkan::escribirMatriz4;

    // 1. La matriz 4x4 se copia completa y no toca el canario.
    {
        float buffer[16 + kFloatsCanario + kFloatsHolgura];
        for (int i = 0; i < 16 + kFloatsCanario + kFloatsHolgura; ++i)
            buffer[i] = kPatronCanario;
        float matriz[16];
        for (int i = 0; i < 16; ++i) matriz[i] = static_cast<float>(i) + 0.5f;

        const bool ok = escribirMatriz4(
            reinterpret_cast<unsigned char*>(buffer), 16 * sizeof(float), 0,
            matriz);

        comprobar(ok, "la matriz 4x4 se escribe en un bloque que la entra");
        bool valoresCorrectos = true;
        for (int i = 0; i < 16; ++i) {
            if (buffer[i] != matriz[i]) valoresCorrectos = false;
        }
        comprobar(valoresCorrectos,
                  "la matriz 4x4 se copia con los valores que tiene");
        bool canarioIntacto = true;
        for (int i = 16; i < 16 + kFloatsCanario; ++i) {
            if (buffer[i] != kPatronCanario) canarioIntacto = false;
        }
        comprobar(canarioIntacto,
                  "la matriz 4x4 no escribe fuera del bloque de uniformes");
    }

    // 2. La matriz 3x3 deja a cero los 12 bytes de padding de std140 y no
    // toca el canario.
    {
        float buffer[12 + kFloatsCanario + kFloatsHolgura];
        for (int i = 0; i < 12 + kFloatsCanario + kFloatsHolgura; ++i)
            buffer[i] = kPatronCanario;
        float matriz[9];
        for (int i = 0; i < 9; ++i) matriz[i] = static_cast<float>(i) + 0.25f;

        const bool ok = escribirMatriz3(
            reinterpret_cast<unsigned char*>(buffer), 12 * sizeof(float), 0,
            matriz);

        comprobar(ok, "la matriz 3x3 se escribe en un bloque que la entra");
        bool valoresCorrectos = true;
        for (int i = 0; i < 9; ++i) {
            if (buffer[i] != matriz[i]) valoresCorrectos = false;
        }
        comprobar(valoresCorrectos,
                  "la matriz 3x3 se copia con los valores que tiene");
        bool paddingACero = true;
        for (int i = 9; i < 12; ++i) {
            if (buffer[i] != 0.0f) paddingACero = false;
        }
        comprobar(paddingACero,
                  "la matriz 3x3 deja a cero el padding de std140");
        bool canarioIntacto = true;
        for (int i = 12; i < 12 + kFloatsCanario; ++i) {
            if (buffer[i] != kPatronCanario) canarioIntacto = false;
        }
        comprobar(canarioIntacto,
                  "la matriz 3x3 no escribe fuera del bloque de uniformes");
    }

    // 3. Una escritura que no entra en el bloque se rechaza y no escribe nada.
    {
        float buffer[16 + kFloatsCanario + kFloatsHolgura];
        for (int i = 0; i < 16 + kFloatsCanario + kFloatsHolgura; ++i)
            buffer[i] = kPatronCanario;
        float matriz[16];
        for (int i = 0; i < 16; ++i) matriz[i] = 7.0f;

        const bool ok = escribirMatriz4(
            reinterpret_cast<unsigned char*>(buffer), 16 * sizeof(float), 8,
            matriz);

        comprobar(!ok, "una matriz que no entra en el bloque se rechaza");
        bool bloqueIntacto = true;
        for (int i = 0; i < 16 + kFloatsCanario; ++i) {
            if (buffer[i] != kPatronCanario) bloqueIntacto = false;
        }
        comprobar(bloqueIntacto,
                  "una escritura rechazada no modifica el bloque");
    }

    std::cout << (fallos == 0 ? "OK" : "FALLOS") << ": " << total
              << " comprobaciones" << std::endl;
    return fallos == 0 ? 0 : 1;
}
