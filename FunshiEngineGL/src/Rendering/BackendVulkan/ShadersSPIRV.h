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
#ifndef SHADERSSPIRV_H
#define SHADERSSPIRV_H

#include <cstddef>
#include <cstdint>

// SPIR-V de los programas del motor, embebido en el ejecutable.
//
// El fuente GLSL vive en Shaders/ y se compila a SPIR-V en tiempo de build
// (con glslc o glslangValidator). El resultado se convierte en un .cpp con los
// datos del binario, de modo que el runtime no depende de un compilador de
// shaders ni de leer archivos: el motor arranca igual en una maquina sin
// herramientas de shaders instaladas.
//
// El dialecto es GLSL 450 con las reglas de Vulkan (UBO en std140, samplers
// como descriptors, Y hacia abajo y profundidad en [0,1]). El texto de los
// shaders es del backend: el motor no sabe nada de este dialecto.

namespace Rendering {
namespace Backend {
namespace Vulkan {
namespace Shaders {

extern const std::uint32_t kMallaVert[];
extern const std::size_t kMallaVertBytes;
extern const std::uint32_t kMallaFrag[];
extern const std::size_t kMallaFragBytes;

extern const std::uint32_t kLineasVert[];
extern const std::size_t kLineasVertBytes;
extern const std::uint32_t kLineasFrag[];
extern const std::size_t kLineasFragBytes;

extern const std::uint32_t kSpritesVert[];
extern const std::size_t kSpritesVertBytes;
extern const std::uint32_t kSpritesFrag[];
extern const std::size_t kSpritesFragBytes;

extern const std::uint32_t kCieloVert[];
extern const std::size_t kCieloVertBytes;
extern const std::uint32_t kCieloFrag[];
extern const std::size_t kCieloFragBytes;

extern const std::uint32_t kCubemapVert[];
extern const std::size_t kCubemapVertBytes;
extern const std::uint32_t kCubemapFrag[];
extern const std::size_t kCubemapFragBytes;

} // namespace Shaders
} // namespace Vulkan
} // namespace Backend
} // namespace Rendering

#endif
