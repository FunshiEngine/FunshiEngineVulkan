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
#ifndef DIMENSION_PROYECTO_H
#define DIMENSION_PROYECTO_H

// Dimension de un proyecto: se elige al crearlo y no cambia despues. El entero
// que se persiste es el que representa cada caso (2 = plano, 3 = profundidad),
// para que el JSON sea legible sin una tabla de mapeo aparte.
enum class DimensionProyecto {
    DosD = 2,   // Plano XY, camara ortografica.
    TresD = 3   // Mundo con profundidad: comportamiento historico del motor.
};

// Entero persistido de una dimension.
inline int dimensionProyectoAEntero(DimensionProyecto d) {
    return static_cast<int>(d);
}

// Dimension a partir del entero persistido. Un valor ausente o desconocido
// (proyecto previo a la dimension, o JSON editado a mano) se interpreta como
// 3D: es el comportamiento con el que se crearon todos los proyectos
// anteriores, asi que la lectura es retrocompatible sin migracion.
inline DimensionProyecto dimensionProyectoDesdeEntero(int valor) {
    return valor == static_cast<int>(DimensionProyecto::DosD)
               ? DimensionProyecto::DosD
               : DimensionProyecto::TresD;
}

#endif // DIMENSION_PROYECTO_H
