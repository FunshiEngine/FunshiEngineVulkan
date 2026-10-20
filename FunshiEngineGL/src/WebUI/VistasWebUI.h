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
#ifndef VISTASWEBUI_H
#define VISTASWEBUI_H

#include <map>
#include <vector>

#include "VistaWebUI.h"

class InterfaceComponent;
class EstadoInterfaces;

// Conjunto de vistas web vivas de una escena, una por interfaz activa. Lleva el
// ciclo de vida frente al runtime web: carga la vista de cada interfaz que
// aparece, recarga si cambia su .html o si el archivo cambia en disco (mtime),
// ajusta el area al contenedor, la avanza por frame (actualizar + render) y
// descarga la de las interfaces que ya no estan. Es logica sin GL ni ImGui:
// con el backend nulo corre entera en headless, y por eso se puede probar sin
// hardware.
class VistasWebUI {
public:
    // Sincroniza con la lista de interfaces activas (ya ordenadas por z). Crea,
    // recarga, redimensiona, avanza y descarga segun corresponda. Las medidas
    // del contenedor (ancho x alto, en pixeles) son el area de las vistas con
    // tamano automatico; las de tamano fijo usan el suyo. Si `estado` no es
    // nulo, entrega a cada vista el estado pendiente de su interfaz y vuelca en
    // el estado la salida que el JS haya publicado.
    void sincronizar(const std::vector<InterfaceComponent*>& interfaces,
                     int anchoContenedor, int altoContenedor, float deltaTime,
                     EstadoInterfaces* estado = nullptr);

    // Vista de una interfaz, o nullptr si no tiene vista viva.
    const VistaWebUI* buscar(const InterfaceComponent* interfaz) const;
    // Variante mutable: para reenviar input a la vista viva (la preview del
    // editor). No crea la vista si no existe.
    VistaWebUI* buscar(const InterfaceComponent* interfaz);

    // Descarga todas las vistas.
    void vaciar();

private:
    std::map<const InterfaceComponent*, VistaWebUI> vistas_;
};

#endif // VISTASWEBUI_H
