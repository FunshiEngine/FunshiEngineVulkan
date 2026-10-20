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
#ifndef ESTADOINTERFACES_H
#define ESTADOINTERFACES_H

#include <memory>
#include <string>

class InterfaceComponent;

// Estado en tiempo de ejecucion de las interfaces web de una escena. Por cada
// interfaz viva guarda DOS arboles JSON:
//   - estado: lo que el MOTOR entrega a la interfaz. El backend web lo pasa al
//     script de la pagina (`FunshiUI.actualizar(estado)`).
//   - salida: lo que la interfaz devuelve al MOTOR (`FunshiUI.salida`). El
//     backend lo vuelca aca despues de correr el JS.
// Ambos se manejan por rutas estilo JSON Pointer (RFC 6901): "/jugador/vida".
//
// Es logica sin GL, backend ni ImGui: con el backend nulo corre entera en
// headless y se puede probar sin hardware. Vive una por escena; GameScene la
// crea y se la inyecta a la escena y al ciclo de vistas.
class EstadoInterfaces {
public:
    EstadoInterfaces();
    ~EstadoInterfaces();

    EstadoInterfaces(const EstadoInterfaces&) = delete;
    EstadoInterfaces& operator=(const EstadoInterfaces&) = delete;

    // --- estado (motor -> interfaz) ---
    // Reemplaza el arbol de estado completo por `json`. Devuelve false si el
    // texto no es JSON valido.
    bool establecerEstado(InterfaceComponent* interfaz, const std::string& json);

    // Fija un valor en la ruta indicada (crea los objetos intermedios que
    // falten). `valorJson` es un literal JSON de cualquier tipo. Devuelve false
    // si el literal es invalido o la ruta no se puede resolver.
    bool establecerValor(InterfaceComponent* interfaz, const std::string& ruta,
                         const std::string& valorJson);

    // Estado serializado pendiente de enviar a la interfaz, o nullptr si no
    // hay cambios desde el ultimo envio. El puntero vale hasta la proxima
    // mutacion del estado de esa interfaz.
    const std::string* estadoPendiente(const InterfaceComponent* interfaz);
    // Marca el estado como ya enviado (limpia el pendiente).
    void marcarEnviado(const InterfaceComponent* interfaz);

    // --- salida (interfaz -> motor) ---
    // Reemplaza el arbol de salida completo. Lo llama el backend web.
    void definirSalida(InterfaceComponent* interfaz, const std::string& json);

    // Valor en la ruta de la SALIDA, serializado como texto JSON, o nullptr si
    // la ruta no existe. El puntero vale hasta la proxima lectura o mutacion de
    // la salida de esa interfaz.
    const char* valor(InterfaceComponent* interfaz, const std::string& ruta);

    // Descarta el estado y la salida de una interfaz (al descargar su vista).
    void olvidar(const InterfaceComponent* interfaz);

    // Descarta el estado y la salida de todas las interfaces.
    void vaciar();

    // Normaliza una ruta JSON Pointer: vacia => raiz; agrega la barra inicial
    // si falta. La usan el motor y su tabla de servicios para aceptar tanto
    // "vida" como "/jugador/vida".
    static std::string normalizarRuta(const std::string& ruta);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

#endif // ESTADOINTERFACES_H
