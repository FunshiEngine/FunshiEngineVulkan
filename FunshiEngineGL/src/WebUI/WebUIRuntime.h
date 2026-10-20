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
#ifndef WEBUIRUNTIME_H
#define WEBUIRUNTIME_H

#include <cstdint>
#include <string>

#include "VistaWebUI.h"

class IWebUIBackend;

// Punto unico de acceso al subsistema web (mismo rol que ScriptRuntime, que
// centraliza los backends de scripts). Mantiene un registro de backends, elige
// el activo (por configuracion/compilacion) y despacha TODAS las operaciones de
// una vista al backend elegido. Si no hay un backend real compilado, cae al
// backend nulo: la API nunca queda sin backend, el ciclo corre igual y el
// overlay no dibuja nada.
class WebUIRuntime {
public:
    // true si el motor se compilo con un backend web real (con dependencias,
    // p. ej. CEF via FUNSHI_WEBUI). Sin backend real solo esta el nulo.
    static bool soporteBackendReal();

    // Nombre del backend activo ("null" si solo esta el nulo).
    static const char* nombreBackendActivo();

    // Selecciona el backend activo por nombre. Devuelve false y deja el activo
    // actual si el nombre no esta registrado (fallback: nunca deja el motor sin
    // backend).
    static bool seleccionarBackend(const std::string& nombre);

    // Backend activo (nunca nullptr: cae al nulo).
    static IWebUIBackend& backendActivo();

    // Rol de subproceso: si el proceso actual fue relanzado como subproceso del
    // backend web (CEF), ejecuta ese rol y devuelve su codigo de salida (>=0).
    // Devuelve -1 si no es un subproceso: el motor debe seguir arrancando. Se
    // invoca al comienzo de main, antes de tocar ventana/log/GL.
    static int ejecutarSubproceso(int argc, char** argv);

    // Arranca el motor web global del backend activo. Devuelve false si no pudo;
    // el motor sigue corriendo con las vistas sin textura (nunca aborta por
    // esto). Idempotente. Se llama una vez, ya con el contexto grafico listo.
    static bool inicializar(int argc, char** argv);

    // Bombea el bucle de mensajes del backend activo (una vez por frame, hilo
    // principal). Sin backend real (o con uno que corra su propio bucle) no
    // hace nada.
    static void bombear();

    // Despacho: toda la aplicacion pasa por aca. Reenvia al backend activo.
    static VistaWebUI cargar(const std::string& rutaHtml, int ancho, int alto,
                             std::string& error);
    static void redimensionar(VistaWebUI& vista, int ancho, int alto);
    static void actualizar(VistaWebUI& vista, float deltaTime);
    static void establecerEstado(VistaWebUI& vista,
                                 const std::string& jsonEstado);
    static std::string leerSalida(VistaWebUI& vista);
    static std::uint64_t render(VistaWebUI& vista);
    static void ratonMover(VistaWebUI& vista, int x, int y);
    static void ratonBoton(VistaWebUI& vista, int boton, bool abajo);
    static void ratonRueda(VistaWebUI& vista, float delta);
    static void tecla(VistaWebUI& vista, int codigo, bool abajo, bool repetir);
    static void foco(VistaWebUI& vista, bool activo);
    static void descargar(VistaWebUI& vista);

    // Cierre de la aplicacion: apaga todos los backends registrados.
    static void apagar();
};

#endif // WEBUIRUNTIME_H
