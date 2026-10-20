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
#include "WebUIRuntime.h"

#include <map>
#include <memory>

#include "BackendNull.h"
#include "IWebUIBackend.h"

#if defined(FUNSHI_CEF)
// Backend CEF real. Se compila en su propia unidad (C++20, con los headers de
// CEF); este encabezado no incluye CEF, solo declara la clase (pimpl).
#include "BackendCef.h"
#endif

namespace {

// Instancias unicas de backend, compartidas por el motor y la GUI (mismo rol
// que el registro de ScriptRuntime). El backend nulo siempre esta: es el
// fallback que garantiza que el runtime nunca quede sin backend.
std::map<std::string, std::unique_ptr<IWebUIBackend>>& registro() {
    static std::map<std::string, std::unique_ptr<IWebUIBackend>> registro;
    if (registro.empty()) {
        registro.insert({"null", std::make_unique<BackendNullWebUI>()});
#if defined(FUNSHI_CEF)
        registro.insert({"cef", std::make_unique<BackendCefWebUI>()});
#endif
    }
    return registro;
}

// Nombre del backend elegido al arrancar: el primer backend real disponible o,
// si no hay, el nulo. Se calcula una vez.
std::string& backendSeleccionado() {
    static std::string seleccionado = [] {
        for (const auto& par : registro())
            if (par.first != "null") return par.first;
        return std::string("null");
    }();
    return seleccionado;
}

} // namespace

bool WebUIRuntime::soporteBackendReal() {
    for (const auto& par : registro())
        if (par.first != "null") return true;
    return false;
}

const char* WebUIRuntime::nombreBackendActivo() {
    return backendSeleccionado().c_str();
}

bool WebUIRuntime::seleccionarBackend(const std::string& nombre) {
    if (registro().find(nombre) == registro().end()) return false;
    backendSeleccionado() = nombre;
    return true;
}

IWebUIBackend& WebUIRuntime::backendActivo() {
    auto& r = registro();
    auto it = r.find(backendSeleccionado());
    if (it == r.end()) it = r.find("null"); // el nulo siempre existe
    return *it->second;
}

VistaWebUI WebUIRuntime::cargar(const std::string& rutaHtml, int ancho, int alto,
                                std::string& error) {
    VistaWebUI vista;
    backendActivo().cargar(vista, rutaHtml, ancho, alto, error);
    return vista;
}

void WebUIRuntime::redimensionar(VistaWebUI& vista, int ancho, int alto) {
    backendActivo().redimensionar(vista, ancho, alto);
}

void WebUIRuntime::actualizar(VistaWebUI& vista, float deltaTime) {
    backendActivo().actualizar(vista, deltaTime);
}

void WebUIRuntime::establecerEstado(VistaWebUI& vista,
                                    const std::string& jsonEstado) {
    backendActivo().establecerEstado(vista, jsonEstado);
}

std::string WebUIRuntime::leerSalida(VistaWebUI& vista) {
    return backendActivo().leerSalida(vista);
}

std::uint64_t WebUIRuntime::render(VistaWebUI& vista) {
    return backendActivo().render(vista);
}

void WebUIRuntime::ratonMover(VistaWebUI& vista, int x, int y) {
    backendActivo().ratonMover(vista, x, y);
}

void WebUIRuntime::ratonBoton(VistaWebUI& vista, int boton, bool abajo) {
    BotonRatonWebUI b = BotonRatonWebUI::Izquierdo;
    switch (boton) {
    case 1: b = BotonRatonWebUI::Derecho; break;
    case 2: b = BotonRatonWebUI::Medio; break;
    default: b = BotonRatonWebUI::Izquierdo; break;
    }
    backendActivo().ratonBoton(vista, b, abajo);
}

void WebUIRuntime::ratonRueda(VistaWebUI& vista, float delta) {
    backendActivo().ratonRueda(vista, delta);
}

void WebUIRuntime::tecla(VistaWebUI& vista, int codigo, bool abajo,
                         bool repetir) {
    backendActivo().tecla(vista, codigo, abajo, repetir);
}

void WebUIRuntime::foco(VistaWebUI& vista, bool activo) {
    backendActivo().foco(vista, activo);
}

void WebUIRuntime::descargar(VistaWebUI& vista) {
    backendActivo().descargar(vista);
}

int WebUIRuntime::ejecutarSubproceso(int argc, char** argv) {
    // Todos los backends registrados prueban si este proceso es un subproceso
    // suyo. El nulo devuelve -1 (no tiene). El primero que reconozca el proceso
    // se queda con el.
    for (auto& par : registro()) {
        const int codigo = par.second->ejecutarSubproceso(argc, argv);
        if (codigo >= 0) return codigo;
    }
    return -1;
}

bool WebUIRuntime::inicializar(int argc, char** argv) {
    IWebUIBackend& backend = backendActivo();
    if (backend.inicializar(argc, argv)) return true;

    // El backend elegido no pudo arrancar (p. ej. falta el runtime de CEF):
    // se cae al nulo para no dejar el motor sin backend. El ciclo sigue y las
    // vistas quedan sin textura; la GUI muestra el motivo en vista.error.
    if (backendSeleccionado() != "null") {
        backendSeleccionado() = "null";
        return backendActivo().inicializar(argc, argv);
    }
    return false;
}

void WebUIRuntime::bombear() {
    backendActivo().bombear();
}

void WebUIRuntime::apagar() {
    for (auto& par : registro()) par.second->apagar();
}
