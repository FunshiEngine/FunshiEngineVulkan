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
#include "LayoutUniformes.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace Rendering {
namespace Backend {
namespace Vulkan {

namespace {

// Alinea un valor a un multiplo de la alineacion.
int alinear(int valor, int alineacion) {
    const int resto = valor % alineacion;
    return resto == 0 ? valor : valor + (alineacion - resto);
}

// Tamano en bytes que ocupa un valor del tipo dado en std140.
//
// Es el tamaño con el que se avanza al siguiente miembro. Para un vec3 son 12
// bytes (3 componentes), aunque el miembro reserva 16: el relleno extra de un
// vec3 se lo come el miembro siguiente si cabe, porque el offset del siguiente
// se calcula como "offset actual + tamaño actual, alineado a la alineacion
// base del siguiente". Un float (alineacion 4) puede vivir justo despues de un
// vec3, en el hueco de 4 bytes que deja.
//
// El paso (stride) de un array es el tamaño del elemento redondeado a 16
// bytes, asi que un array de vec3 avanza de a 16 bytes por elemento.
int tamanoDe(Contrato::TipoUniforme tipo) {
    switch (tipo) {
        case Contrato::TipoUniforme::Mat4:
            return 64;
        case Contrato::TipoUniforme::Mat3:
            return 48;
        case Contrato::TipoUniforme::Vec3:
            return 12;
        case Contrato::TipoUniforme::Vec4:
            return 16;
        case Contrato::TipoUniforme::Float:
        case Contrato::TipoUniforme::Int:
            return 4;
        case Contrato::TipoUniforme::Sampler2D:
        case Contrato::TipoUniforme::SamplerCube:
            return 0;
    }
    return 0;
}

// Alineacion en bytes de un valor del tipo dado en std140.
int alineacionDe(Contrato::TipoUniforme tipo) {
    switch (tipo) {
        case Contrato::TipoUniforme::Mat4:
        case Contrato::TipoUniforme::Mat3:
        case Contrato::TipoUniforme::Vec3:
        case Contrato::TipoUniforme::Vec4:
            return 16;
        case Contrato::TipoUniforme::Float:
        case Contrato::TipoUniforme::Int:
            return 4;
        case Contrato::TipoUniforme::Sampler2D:
        case Contrato::TipoUniforme::SamplerCube:
            return 0;
    }
    return 0;
}

bool esSampler(Contrato::TipoUniforme tipo) {
    return tipo == Contrato::TipoUniforme::Sampler2D ||
           tipo == Contrato::TipoUniforme::SamplerCube;
}

} // namespace

LayoutUniformes::LayoutUniformes() = default;

LayoutUniformes::LayoutUniformes(Contrato::ShaderKind kind) {
    int cantidad = 0;
    const Contrato::UniformeRequerido* tabla =
        Contrato::uniformesDe(kind, cantidad);

    int offset = 0;
    for (int i = 0; i < cantidad; ++i) {
        const Contrato::UniformeRequerido& u = tabla[i];
        EntradaUniforme entrada;
        entrada.nombre = u.nombre;
        entrada.tipo = u.tipo;
        entrada.elementos = u.elementos;
        entrada.esSampler = esSampler(u.tipo);

        if (entrada.esSampler) {
            // Los samplers no van en el bloque: son descriptors. Su token es
            // el indice en la lista de samplers, con signo negativo.
            entrada.offset = kPrimerTokenSampler -
                             static_cast<int>(samplers_.size());
            samplers_.push_back(entrada);
        } else {
            const int alineacion = alineacionDe(u.tipo);
            const int tamano = tamanoDe(u.tipo);
            if (u.elementos > 1) {
                // Array: el paso de std140 es el tamano del elemento alineado a
                // 16 bytes.
                const int paso = alinear(tamano, 16);
                offset = alinear(offset, 16);
                entrada.paso = paso;
                entrada.offset = offset;
                offset += paso * u.elementos;
            } else {
                offset = alinear(offset, alineacion);
                entrada.offset = offset;
                offset += tamano;
            }
        }

        indice_[entrada.nombre] = static_cast<int>(entradas_.size());
        entradas_.push_back(entrada);
    }

    tamano_ = alinear(offset, 16);
}

int LayoutUniformes::resolver(const char* nombre) const {
    if (!nombre || !nombre[0]) return kTokenInvalido;

    // El nombre puede traer el indice de un array: "uLightTypes[3]".
    std::string base(nombre);
    int indice = 0;
    const std::size_t corchete = base.find('[');
    if (corchete != std::string::npos) {
        const std::size_t cierre = base.find(']', corchete);
        if (cierre == std::string::npos) return kTokenInvalido;
        const std::string texto = base.substr(corchete + 1, cierre - corchete - 1);
        if (texto.empty()) return kTokenInvalido;
        for (char c : texto) {
            if (c < '0' || c > '9') return kTokenInvalido;
        }
        indice = std::atoi(texto.c_str());
        base = base.substr(0, corchete);
    }

    const auto it = indice_.find(base);
    if (it == indice_.end()) return kTokenInvalido;
    const EntradaUniforme& entrada = entradas_[it->second];

    if (indice != 0) {
        // Solo los arrays tienen elementos.
        if (entrada.elementos <= 1 || entrada.paso == 0)
            return kTokenInvalido;
        if (indice < 0 || indice >= entrada.elementos) return kTokenInvalido;
    }

    if (entrada.esSampler) return entrada.offset;

    return entrada.offset + indice * entrada.paso;
}

int LayoutUniformes::offsetDe(const char* nombre) const {
    std::string base(nombre ? nombre : "");
    const std::size_t corchete = base.find('[');
    if (corchete != std::string::npos) base = base.substr(0, corchete);
    const auto it = indice_.find(base);
    if (it == indice_.end()) return -1;
    const EntradaUniforme& entrada = entradas_[it->second];
    if (entrada.esSampler) return -1;
    return entrada.offset;
}

bool LayoutUniformes::consultar(const char* nombre, Contrato::TipoUniforme& tipo,
                                int& elementos) const {
    std::string base(nombre ? nombre : "");
    const std::size_t corchete = base.find('[');
    if (corchete != std::string::npos) base = base.substr(0, corchete);
    const auto it = indice_.find(base);
    if (it == indice_.end()) return false;
    tipo = entradas_[it->second].tipo;
    elementos = entradas_[it->second].elementos;
    return true;
}

} // namespace Vulkan
} // namespace Backend
} // namespace Rendering
