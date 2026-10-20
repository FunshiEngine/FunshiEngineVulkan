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
#include "IRenderBackend.h"

#include "BackendNulo.h"

#include <memory>

namespace Rendering {
namespace Backend {
namespace {

// Backend activo. Por defecto es el nulo: el motor no implementa ninguna API
// grafica, y el backend que se instale lo reemplaza.
//
// El acceso no es thread-safe a proposito: el render corre en el hilo principal
// y el backend se instala antes de arrancar el bucle.
std::unique_ptr<IRenderBackend>& backendActivo() {
    static std::unique_ptr<IRenderBackend> backend =
        std::make_unique<BackendNulo>();
    return backend;
}

} // namespace

IRenderBackend& activeBackend() {
    return *backendActivo();
}

void instalarBackend(std::unique_ptr<IRenderBackend> backend) {
    if (!backend) backend = std::make_unique<BackendNulo>();
    backendActivo() = std::move(backend);
}

} // namespace Backend
} // namespace Rendering
