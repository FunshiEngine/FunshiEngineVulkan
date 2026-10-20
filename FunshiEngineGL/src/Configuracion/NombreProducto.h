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
#ifndef NOMBRE_PRODUCTO_H
#define NOMBRE_PRODUCTO_H

// Nombre del producto: el que ve el usuario. Aparece en el titulo de la
// ventana, en el nombre de la instancia de Vulkan, en los ficheros de log y de
// diagnosticos y en la carpeta de datos por usuario.
//
// En este repositorio la aplicacion es la variante con Vulkan, asi que se
// llama FunshiEngineVK. El arbol de fuentes se sigue llamando FunshiEngineGL
// porque viene del core y las rutas de assets apuntan ahi: el nombre del
// arbol y el del producto son cosas distintas.
//
// Vive en una constante para que el nombre no pueda quedar a medias (un
// titulo nuevo con un log viejo, o dos logs con nombres distintos). CMake y
// los scripts de empaquetado no pueden leer una constante de C++, asi que en
// esos el literal esta escrito a mano.
namespace NombreProducto {

inline constexpr const char* kNombre = "FunshiEngineVK";

}  // namespace NombreProducto

#endif
