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
#ifndef LAYOUTUNIFORMES_H
#define LAYOUTUNIFORMES_H

#include <string>
#include <unordered_map>
#include <vector>

#include "../ContratoRender.h"

namespace Rendering {
namespace Backend {
namespace Vulkan {

// Token opaco que el backend devuelve como localizador de un uniform.
//
// El motor solo lo reenvia a los setUniform*, asi que su valor concreto no
// importa. El backend lo usa como:
//
//   - offset en bytes dentro del bloque de uniforms, para los valores que
//     viajan en el bloque (0 o mas);
//   - indice de sampler, con signo negativo, para los que viajan como
//     descriptor.
//
// El valor -1 queda reservado: es lo que el motor entiende como "este
// programa no expone ese uniform".
constexpr int kTokenInvalido = -1;
constexpr int kPrimerTokenSampler = -2;

struct EntradaUniforme {
    // Nombre canonico (sin el indice del array).
    std::string nombre;
    // Offset en bytes dentro del bloque (los samplers no van en el bloque).
    int offset = 0;
    Contrato::TipoUniforme tipo = Contrato::TipoUniforme::Float;
    // Cantidad de elementos: 1, o la longitud del array.
    int elementos = 1;
    // Paso (stride) en bytes de un array. 0 si no es un array.
    int paso = 0;
    // Los samplers no viajan en el bloque: van como descriptor.
    bool esSampler = false;
};

// Layout de un bloque de uniforms en la convencion std140.
//
// El layout no se escribe a mano: se deriva de la tabla del contrato
// (Contrato::Detalle), que es la unica fuente de verdad sobre que expone cada
// tipo de programa. Asi, si el contrato agrega un uniform, el bloque lo refleja
// sin que haya que tocar el backend.
//
// std140: las matrices y los vectores de 4 componentes ocupan 16 bytes y se
// alinean a 16; un vec3 ocupa 16 bytes (12 usados); los escalares se alinean a
// 4; y los arrays tienen un paso de 16 bytes por elemento.
class LayoutUniformes {
public:
    // Layout vacio: no expone ningun uniform. Sirve para un programa que todavia
    // no tiene un tipo asignado.
    LayoutUniformes();

    // Layout del bloque del tipo de programa indicado.
    explicit LayoutUniformes(Contrato::ShaderKind kind);

    // Tamano del bloque en bytes (multiple de 16).
    int tamano() const { return tamano_; }

    // Localizador del uniform con ese nombre.
    //
    // Acepta el nombre canonico ("uModel") o el de un elemento de un array
    // ("uLightTypes[3]"). Devuelve kTokenInvalido si el programa no expone ese
    // uniform o si el indice esta fuera del array.
    int resolver(const char* nombre) const;

    // Offset de un uniform del bloque, o -1 si no esta.
    int offsetDe(const char* nombre) const;

    // Tipo y cantidad de elementos del uniform (0 si no existe).
    bool consultar(const char* nombre, Contrato::TipoUniforme& tipo,
                   int& elementos) const;

    // Los samplers del tipo de programa, en el orden en que aparecen en la
    // tabla del contrato. Su indice es el que usa el token.
    const std::vector<EntradaUniforme>& samplers() const { return samplers_; }

    // Todas las entradas (bloque y samplers), en el orden del contrato.
    const std::vector<EntradaUniforme>& entradas() const { return entradas_; }

private:
    std::vector<EntradaUniforme> entradas_;
    std::vector<EntradaUniforme> samplers_;
    std::unordered_map<std::string, int> indice_;
    int tamano_ = 0;
};

} // namespace Vulkan
} // namespace Backend
} // namespace Rendering

#endif
