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
#ifndef BACKENDVULKANUTIL_H
#define BACKENDVULKANUTIL_H

#include <vulkan/vulkan.h>

#include <iostream>

// Utilidades internas del backend de Vulkan: el reporte de un VkResult que no
// es VK_SUCCESS. Viven en un header propio (y no en un .cpp) porque el backend
// se reparte en varias unidades de traduccion (nucleo, recursos, shaders y
// dibujo) y todas necesitan la misma comprobacion.

namespace Rendering {
namespace Backend {
namespace Vulkan {

inline void reportarErrorVulkan(const char* donde, VkResult resultado) {
    std::cerr << "[Vulkan] " << donde << ": fallo VkResult " << resultado
              << std::endl;
}

inline bool okVulkan(VkResult resultado, const char* donde) {
    if (resultado != VK_SUCCESS) {
        reportarErrorVulkan(donde, resultado);
        return false;
    }
    return true;
}

} // namespace Vulkan
} // namespace Backend
} // namespace Rendering

#endif
