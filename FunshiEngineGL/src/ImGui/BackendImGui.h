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
#ifndef BACKENDIMGUI_H
#define BACKENDIMGUI_H

#include <imgui.h>

// Fachada del backend de la interfaz.
//
// Dear ImGui separa la interfaz (imgui.cpp, que calcula los widgets) del
// backend que la dibuja (imgui_impl_opengl3, imgui_impl_vulkan, ...) y del
// backend de plataforma que le entrega los eventos (imgui_impl_glfw). El
// backend de render es tan especifico de la API grafica como el del motor, y
// el motor no implementa ninguna API: por eso el motor no llama a
// ImGui_ImplOpenGL3_* ni a ImGui_ImplVulkan_* directamente, sino a esta
// fachada.
//
// El nucleo de ImGui (crear contexto, NewFrame, Render, destruir contexto) lo
// sigue manejando el motor: son llamadas genericas, no dependen de la API.

namespace MotorImGui {

// Prepara los backends de la interfaz.
//
// ventana es el GLFWwindow de la ventana del motor; instalarCallbacks indica si
// se enganchan los callbacks de teclado y raton de GLFW.
//
// Devuelve false si la plataforma no quedo lista. El backend de render puede
// no existir (hayRenderer() lo dice): en ese caso la interfaz se calcula igual
// (los widgets responden y el estado avanza) pero no se dibuja, que es lo que
// pasa cuando el motor corre sin API grafica implementada.
bool init(void* ventana, bool instalarCallbacks);

// Cierra el backend de render y el de plataforma. Se llama antes de destruir el
// contexto de ImGui.
void shutdown();

// Frame del backend de render y de la plataforma. Se llama justo antes de
// ImGui::NewFrame().
void newFrame();

// Envia los draw lists de la interfaz al backend de render.
void render(const ImDrawData* datos);

// Si hay backend de render que pueda dibujar la interfaz.
bool hayRenderer();

// Nombre del backend de render, para los logs de arranque.
const char* nombreRenderer();

} // namespace MotorImGui

#endif
