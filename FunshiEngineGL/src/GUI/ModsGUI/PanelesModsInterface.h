/*
    FunshiEngineGL - Motor de juegos 3D con OpenGL e ImGui
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
#ifndef PANELESMODSINTERFACE_H
#define PANELESMODSINTERFACE_H

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "PanelModInterface.h"

// Coleccion de paneles que registraron los mods cargados.
//
// Se sincroniza con el registro de paneles cada frame: un mod puede registrar
// un panel en cualquier momento (tambien en caliente, desde su update), y al
// descargar un mod sus paneles desaparecen.
class PanelesModsInterface {
public:
    PanelesModsInterface();

    // Dibuja todos los paneles registrados.
    void printGUI();

    // Etiquetas de los paneles para el menu "Ventanas": clave de la ventana y
    // texto visible.
    std::vector<std::pair<std::string, std::string>> etiquetas() const;

private:
    // Crea los envoltorios de los paneles nuevos y quita los de los mods que
    // ya no estan cargados.
    void sincronizar();

    std::vector<std::unique_ptr<PanelModInterface>> paneles_;
};
#endif
