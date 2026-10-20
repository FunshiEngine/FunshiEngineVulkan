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
#ifndef BACKENDCEF_H
#define BACKENDCEF_H

#include <cstdint>
#include <string>

#include "IWebUIBackend.h"
#include "VistaWebUI.h"

// Estado interno del backend CEF (navegador OSR, textura, buffers, cliente de
// CEF). Se define en BackendCef.cpp; aca solo se lo nombra para no arrastrar los
// headers de CEF a esta cabecera.
struct BackendCefImpl;

// Backend real de interfaces web sobre CEF (Chromium Embedded Framework).
//
// Este encabezado NO incluye ningun header de CEF a proposito: el motor compila
// en C++17 y CEF 154 exige C++20. Toda la maquinaria vive en BackendCef.cpp,
// que se compila aparte con C++20 y los headers de CEF; aca solo se declara la
// clase con un pimpl. El resto del motor (WebUIRuntime, GameScene, la GUI)
// conversa con esta clase por la interfaz IWebUIBackend y nunca ve a CEF.
//
// Cada vista monta un navegador off-screen (OSR): CEF entrega el frame como un
// bitmap BGRA que se sube a una textura del backend grafico del motor. El
// bucle de mensajes de CEF se bombea desde el hilo principal con
// bombear() (external_message_pump), asi que los callbacks (OnPaint, JS) corren
// en el hilo del motor y no hace falta sincronizacion cruzada.
class BackendCefWebUI : public IWebUIBackend {
public:
    BackendCefWebUI();
    ~BackendCefWebUI() override;

    // Rol de subproceso (CefExecuteProcess) e inicializacion del motor CEF.
    int ejecutarSubproceso(int argc, char** argv) override;
    bool inicializar(int argc, char** argv) override;
    void bombear() override;

    const char* nombre() const override;

    bool cargar(VistaWebUI& vista, const std::string& rutaHtml, int ancho,
                int alto, std::string& error) override;
    void redimensionar(VistaWebUI& vista, int ancho, int alto) override;
    void actualizar(VistaWebUI& vista, float deltaTime) override;
    void establecerEstado(VistaWebUI& vista, const std::string& jsonEstado) override;
    std::string leerSalida(VistaWebUI& vista) override;
    std::uint64_t render(VistaWebUI& vista) override;

    void ratonMover(VistaWebUI& vista, int x, int y) override;
    void ratonBoton(VistaWebUI& vista, BotonRatonWebUI boton, bool abajo) override;
    void ratonRueda(VistaWebUI& vista, float delta) override;
    void tecla(VistaWebUI& vista, int codigo, bool abajo, bool repetir) override;
    void foco(VistaWebUI& vista, bool activo) override;

    void descargar(VistaWebUI& vista) override;
    void apagar() override;

private:
    BackendCefImpl* impl_;  // estado interno (definido en BackendCef.cpp)

    // Convierte un `manejador` opaco de vista en su estado interno, o nullptr si
    // la vista no es de este backend.
    BackendCefImpl* vistaDe(const VistaWebUI& vista) const;
};

#endif // BACKENDCEF_H
