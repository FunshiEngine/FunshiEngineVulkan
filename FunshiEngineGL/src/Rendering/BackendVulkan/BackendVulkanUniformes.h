/*
    FunshiEngineGL - Motor de juegos 3D con API grafica intercambiable e ImGui
    Copyright 2026 Gianfranco Ivan Enrique

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS-IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.

    SPDX-License-Identifier: Apache-2.0
*/
#ifndef BACKENDVULKANUNIFORMES_H
#define BACKENDVULKANUNIFORMES_H

#include <cstddef>
#include <cstring>

namespace Rendering {
namespace Backend {
namespace Vulkan {

// Escritura de matrices en el bloque de uniformes (un UBO en std140).
//
// El bloque es un bloque de bytes y todos los limites se cuentan en bytes: en
// std140 una matriz 4x4 ocupa cuatro columnas de 16 bytes (64 bytes) y una 3x3
// ocupa tres columnas de 16 bytes (48 bytes), aunque los datos de la 3x3 sean
// 36 bytes (tres filas de tres). Son las unicas funciones que escriben una
// matriz en el bloque: al vivir aqui, el limite de cada escritura se comprueba
// contra el tamano real del bloque y no se puede copiar de mas.

// Escribe una matriz 4x4 (16 floats) en el bloque. Devuelve false si el
// bloque no tiene espacio para los 64 bytes de la matriz, y en ese caso no
// escribe nada.
inline bool escribirMatriz4(unsigned char* bloque, std::size_t tamanoBloque,
                            std::size_t offset, const float* valores) {
    if (!bloque || !valores) return false;
    const std::size_t bytes = 16 * sizeof(float);
    if (offset + bytes > tamanoBloque) return false;
    std::memcpy(bloque + offset, valores, bytes);
    return true;
}

// Escribe una matriz 3x3 (9 floats en orden de columna) en el bloque.
//
// En std140 una matriz 3x3 son tres columnas de 16 bytes: cada columna
// guarda sus tres floats y los cuatro bytes siguientes son relleno. El
// shader la lee asi (MatrixStride 16), asi que las columnas no van seguidas:
// la segunda arranca 16 bytes despues de la primera y la tercera, 16 mas.
// Los datos de glm vienen en columna pero sin relleno, asi que hay que
// separarlos al escribirlos. El bloque reserva los 48 bytes del slot (tres
// columnas de 16) y el relleno de cada columna queda a cero.
//
// Devuelve false si el bloque no tiene espacio para los 48 bytes, y en ese
// caso no escribe nada.
inline bool escribirMatriz3(unsigned char* bloque, std::size_t tamanoBloque,
                            std::size_t offset, const float* valores) {
    if (!bloque || !valores) return false;
    const std::size_t paso = 16;
    const std::size_t datos = 3 * sizeof(float);
    if (offset + 3 * paso > tamanoBloque) return false;
    for (std::size_t columna = 0; columna < 3; ++columna) {
        unsigned char* destino = bloque + offset + columna * paso;
        std::memcpy(destino, valores + columna * 3, datos);
        std::memset(destino + datos, 0, paso - datos);
    }
    return true;
}

} // namespace Vulkan
} // namespace Backend
} // namespace Rendering

#endif // BACKENDVULKANUNIFORMES_H
