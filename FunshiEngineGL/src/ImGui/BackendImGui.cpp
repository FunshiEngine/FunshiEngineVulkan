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
#include "BackendImGui.h"

#include "imgui_impl_glfw.h"

#if FUNSHI_VULKAN
#include "imgui_impl_vulkan.h"

#include "../Rendering/BackendVulkan/BackendVulkan.h"

#include <unordered_map>
#endif

// Fachada del backend de la interfaz.
//
// El backend de plataforma es el de GLFW, que no depende de la API grafica:
// se inicializa con InitForOther, que engancha teclado, raton y foco sin
// tocar el portapapeles.
//
// El backend de render es el de la API grafica activa. En este repositorio la
// API es Vulkan, asi que el render es el de upstream (imgui_impl_vulkan) y
// la interfaz se dibuja en una pasada propia del backend grafico.
//
// Cuando no hay backend de render (la maquina no tiene Vulkan, o el backend
// no se pudo inicializar) la interfaz se calcula igual (los widgets existen,
// responden y el estado del editor avanza) pero no se dibuja.
namespace {

bool gPlataformaLista = false;
bool gRendererListo = false;

#if FUNSHI_VULKAN

// Backend de Vulkan activo: lo publica el propio backend, porque es el que
// sabe crear la superficie, el render pass y los recursos.
Rendering::Backend::Vulkan::BackendVulkan* gBackend = nullptr;

// Descriptor de cada textura que muestra la interfaz. En Vulkan el
// identificador de una textura es un conjunto de descriptores, no la vista de
// la imagen: lo crea el backend de ImGui.
std::unordered_map<Rendering::Backend::Handle, VkDescriptorSet> gTexturas;

// Convierte una textura del motor en el identificador que usa ImGui. La
// registra el backend grafico, que es el que sabe que textura se pide.
void* convertirTextura(Rendering::Backend::Handle textura, void* contexto) {
    auto* backend =
        static_cast<Rendering::Backend::Vulkan::BackendVulkan*>(contexto);
    Rendering::Backend::Vulkan::BackendVulkan::TexturaInterfaz datos;
    if (!backend->texturaParaInterfaz(textura, datos)) return nullptr;

    auto it = gTexturas.find(textura);
    if (it != gTexturas.end()) return it->second;

    VkDescriptorSet conjunto =
        ImGui_ImplVulkan_AddTexture(datos.sampler, datos.vista, datos.layout);
    if (!conjunto) return nullptr;
    gTexturas[textura] = conjunto;
    return conjunto;
}

// Libera los descriptores de las texturas que ya no existen: el motor
// destruye las texturas cuando dejan de usarse y el descriptor tiene que
// liberarse con ellas (si no, el pool de ImGui se llena).
void soltarTexturasMuertas() {
    if (!gBackend) return;
    for (auto it = gTexturas.begin(); it != gTexturas.end();) {
        Rendering::Backend::Vulkan::BackendVulkan::TexturaInterfaz datos;
        if (gBackend->texturaParaInterfaz(it->first, datos)) {
            ++it;
            continue;
        }
        if (it->second) ImGui_ImplVulkan_RemoveTexture(it->second);
        it = gTexturas.erase(it);
    }
}

#endif // FUNSHI_VULKAN

} // namespace

bool MotorImGui::init(void* ventana, bool instalarCallbacks) {
    gRendererListo = false;
    gPlataformaLista = ImGui_ImplGlfw_InitForOther(
        static_cast<GLFWwindow*>(ventana), instalarCallbacks);

#if FUNSHI_VULKAN
    gBackend = Rendering::Backend::Vulkan::backendVulkanActivo();
    if (gBackend && gBackend->available() && gBackend->renderPassInterfaz()) {
        ImGui_ImplVulkan_InitInfo info{};
        info.ApiVersion = VK_API_VERSION_1_1;
        info.Instance = gBackend->instancia();
        info.PhysicalDevice = gBackend->gpuFisico();
        info.Device = gBackend->dispositivo();
        info.QueueFamily = gBackend->indiceFamiliaGrafica();
        info.Queue = gBackend->colaGrafica();
        info.RenderPass = gBackend->renderPassInterfaz();
        info.MinImageCount = 2;
        info.ImageCount = gBackend->cantidadImagenesSwapchain();
        info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        // El pool de descriptores lo crea el propio backend de ImGui.
        info.DescriptorPoolSize = 1024;
        if (gBackend->cantidadImagenesSwapchain() < info.MinImageCount) {
            info.MinImageCount = gBackend->cantidadImagenesSwapchain();
        }
        gRendererListo = ImGui_ImplVulkan_Init(&info);
        if (gRendererListo) {
            // El backend grafico necesita saber convertir una textura del
            // motor en el identificador que usa ImGui.
            Rendering::Backend::Vulkan::registrarConversionTextura(
                convertirTextura, gBackend);
        } else {
            gBackend = nullptr;
        }
    }
#endif

    return gPlataformaLista;
}

void MotorImGui::shutdown() {
#if FUNSHI_VULKAN
    if (gRendererListo) {
        Rendering::Backend::Vulkan::registrarConversionTextura(nullptr,
                                                               nullptr);
        for (auto& par : gTexturas) {
            if (par.second) ImGui_ImplVulkan_RemoveTexture(par.second);
        }
        gTexturas.clear();
        ImGui_ImplVulkan_Shutdown();
    }
    gBackend = nullptr;
#endif
    if (gPlataformaLista) {
        ImGui_ImplGlfw_Shutdown();
        gPlataformaLista = false;
    }
    gRendererListo = false;
}

void MotorImGui::newFrame() {
#if FUNSHI_VULKAN
    if (gRendererListo) {
        soltarTexturasMuertas();
        ImGui_ImplVulkan_NewFrame();
    }
#endif
    // Sin backend de render no hay frame de render que preparar.
    if (gPlataformaLista) ImGui_ImplGlfw_NewFrame();
}

void MotorImGui::render(const ImDrawData* datos) {
#if FUNSHI_VULKAN
    if (!gRendererListo || !gBackend || !datos) return;
    if (datos->TotalVtxCount == 0) return;

    // La interfaz se dibuja en una pasada propia del backend grafico, sobre
    // la imagen del swapchain ya resuelta: el pipeline de ImGui no lleva
    // estado de profundidad y no puede usarse en la pasada de la escena.
    gBackend->abrirPasadaInterfaz();
    ImGui_ImplVulkan_RenderDrawData(const_cast<ImDrawData*>(datos),
                                    gBackend->comandoActual());
    gBackend->cerrarPasadaInterfaz();
#endif
}

bool MotorImGui::hayRenderer() {
    return gRendererListo;
}

const char* MotorImGui::nombreRenderer() {
    return gRendererListo ? "vulkan" : "ninguno";
}
