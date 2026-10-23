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
#ifndef PANELMODINTERFACE_H
#define PANELMODINTERFACE_H

#include <string>

#include "../GeneralUserInterface.h"

// Envoltorio de un panel que registro un mod.
//
// El motor no puede instanciar las clases del mod (el binario del mod no
// enlaza contra el motor), asi que la ventana la abre el motor y dentro se
// invoca el callback que el mod entrego al registrarse. El titulo visible y la
// clave de la ventana son cosas distintas: ImGui toma como identificador lo
// que va detras de "##", y ese identificador es el nombre con el que se
// persiste el estado (unico por mod y por panel).
class PanelModInterface : public GeneralUserInterface {
public:
    PanelModInterface(const std::string& nombreVentana,
                      const std::string& titulo);

    // Abre la ventana del panel y dibuja su contenido.
    void printGUI() override;
    void contentGUI() override;

    const std::string& getTitulo() const { return titulo_; }

private:
    // Clave de la ventana en el registro de paneles.
    std::string nombreVentana_;
    // Titulo visible de la ventana.
    std::string titulo_;
};
#endif
