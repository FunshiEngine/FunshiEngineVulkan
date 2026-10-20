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
#include "BackendVulkan.h"

#include "../../Configuracion/NombreProducto.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace Rendering {
namespace Backend {
namespace Vulkan {

namespace {

// Numero de conjuntos de descriptors que el pool puede entregar en un frame.
// Cada draw necesita el suyo: el bloque de uniforms se lee con offset
// dinamico (una slice por draw) y las texturas pueden cambiar entre draws.
constexpr uint32_t kMaxConjuntos = 4096;

// Numero maximo de samplers por programa (el de la malla: difusa, especular,
// emision y normal). El pool reserva los descriptores de imagen para el
// peor caso.
constexpr uint32_t kMaxSamplers = 4;

// El backend de Vulkan activo (lo consulta la fachada de la interfaz) y la
// conversion de texturas que la fachada registra.
BackendVulkan* gBackendActivo = nullptr;
ConversionTextura gConversionTextura = nullptr;
void* gContextoConversion = nullptr;

// Registra un fallo de Vulkan con su codigo.
void reportarError(const char* donde, VkResult resultado) {
    std::cerr << "[Vulkan] " << donde << ": fallo VkResult " << resultado
              << std::endl;
}

bool ok(VkResult resultado, const char* donde) {
    if (resultado != VK_SUCCESS) {
        reportarError(donde, resultado);
        return false;
    }
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// Registro del backend y de la interfaz
// ---------------------------------------------------------------------------

BackendVulkan* backendVulkanActivo() {
    return gBackendActivo;
}

void registrarConversionTextura(ConversionTextura conversion, void* contexto) {
    gConversionTextura = conversion;
    gContextoConversion = contexto;
}

ConversionTextura conversionTexturaActiva() {
    return gConversionTextura;
}

void* contextoConversionTextura() {
    return gContextoConversion;
}

BackendVulkan::BackendVulkan() {
    gBackendActivo = this;
}

BackendVulkan::~BackendVulkan() {
    if (gBackendActivo == this) gBackendActivo = nullptr;
    if (dispositivo_) {
        vkDeviceWaitIdle(dispositivo_);

        for (auto& par : programasPorTipo_) {
            ProgramaVulkan& p = *par.second;
            if (p.mapaUniformes)
                funciones_.vkUnmapMemory(dispositivo_, p.memoriaUniformes);
            if (p.bufferUniformes)
                funciones_.vkDestroyBuffer(dispositivo_, p.bufferUniformes, nullptr);
            if (p.memoriaUniformes)
                funciones_.vkFreeMemory(dispositivo_, p.memoriaUniformes, nullptr);
            for (auto& par2 : p.pipelines)
                if (par2.second.pipeline)
                    funciones_.vkDestroyPipeline(dispositivo_, par2.second.pipeline, nullptr);
            if (p.layoutPipeline)
                funciones_.vkDestroyPipelineLayout(dispositivo_, p.layoutPipeline, nullptr);
            if (p.layoutDescriptores)
                funciones_.vkDestroyDescriptorSetLayout(dispositivo_, p.layoutDescriptores, nullptr);
            if (p.moduloVertex)
                funciones_.vkDestroyShaderModule(dispositivo_, p.moduloVertex, nullptr);
            if (p.moduloFragment)
                funciones_.vkDestroyShaderModule(dispositivo_, p.moduloFragment, nullptr);
        }
        programasPorTipo_.clear();

        for (auto& par : mallas_) {
            MallaVulkan& m = par.second;
            if (m.vertices)
                funciones_.vkDestroyBuffer(dispositivo_, m.vertices, nullptr);
            if (m.memoriaVertices)
                funciones_.vkFreeMemory(dispositivo_, m.memoriaVertices, nullptr);
            if (m.indices)
                funciones_.vkDestroyBuffer(dispositivo_, m.indices, nullptr);
            if (m.memoriaIndices)
                funciones_.vkFreeMemory(dispositivo_, m.memoriaIndices, nullptr);
        }
        mallas_.clear();

        for (auto& par : batches_) {
            if (par.second.vertices)
                funciones_.vkDestroyBuffer(dispositivo_, par.second.vertices, nullptr);
            if (par.second.memoriaVertices)
                funciones_.vkFreeMemory(dispositivo_, par.second.memoriaVertices, nullptr);
        }
        batches_.clear();

        for (auto& par : texturas_) {
            TexturaVulkan& t = par.second;
            if (t.vista)
                funciones_.vkDestroyImageView(dispositivo_, t.vista, nullptr);
            if (t.imagen)
                funciones_.vkDestroyImage(dispositivo_, t.imagen, nullptr);
            if (t.memoria)
                funciones_.vkFreeMemory(dispositivo_, t.memoria, nullptr);
            if (t.sampler)
                funciones_.vkDestroySampler(dispositivo_, t.sampler, nullptr);
        }
        texturas_.clear();

        for (auto& par : objetivos_) {
            RenderTargetVulkan& t = par.second;
            if (t.framebuffer)
                funciones_.vkDestroyFramebuffer(dispositivo_, t.framebuffer, nullptr);
            if (t.vistaColor)
                funciones_.vkDestroyImageView(dispositivo_, t.vistaColor, nullptr);
            if (t.sampler)
                funciones_.vkDestroySampler(dispositivo_, t.sampler, nullptr);
            if (t.color)
                funciones_.vkDestroyImage(dispositivo_, t.color, nullptr);
            if (t.memoriaColor)
                funciones_.vkFreeMemory(dispositivo_, t.memoriaColor, nullptr);
            if (t.vistaProfundidad)
                funciones_.vkDestroyImageView(dispositivo_, t.vistaProfundidad, nullptr);
            if (t.profundidad)
                funciones_.vkDestroyImage(dispositivo_, t.profundidad, nullptr);
            if (t.memoriaProfundidad)
                funciones_.vkFreeMemory(dispositivo_, t.memoriaProfundidad, nullptr);
        }
        objetivos_.clear();

        if (texturaBlanca_.vista)
            funciones_.vkDestroyImageView(dispositivo_, texturaBlanca_.vista, nullptr);
        if (texturaBlanca_.imagen)
            funciones_.vkDestroyImage(dispositivo_, texturaBlanca_.imagen, nullptr);
        if (texturaBlanca_.memoria)
            funciones_.vkFreeMemory(dispositivo_, texturaBlanca_.memoria, nullptr);
        if (texturaBlanca_.sampler)
            funciones_.vkDestroySampler(dispositivo_, texturaBlanca_.sampler, nullptr);

        for (VkBuffer buffer : buffersRetirados_)
            funciones_.vkDestroyBuffer(dispositivo_, buffer, nullptr);
        for (VkDeviceMemory memoria : memoriasRetiradas_)
            funciones_.vkFreeMemory(dispositivo_, memoria, nullptr);
        buffersRetirados_.clear();
        memoriasRetiradas_.clear();

        destruirSwapchain();
        for (VkFramebuffer framebuffer : framebuffersInterfaz_)
            if (framebuffer)
                funciones_.vkDestroyFramebuffer(dispositivo_, framebuffer, nullptr);
        framebuffersInterfaz_.clear();
        if (renderPassSwapchain_)
            funciones_.vkDestroyRenderPass(dispositivo_, renderPassSwapchain_, nullptr);
        if (renderPassInterfaz_)
            funciones_.vkDestroyRenderPass(dispositivo_, renderPassInterfaz_, nullptr);
        if (renderPassObjetivo_)
            funciones_.vkDestroyRenderPass(dispositivo_, renderPassObjetivo_, nullptr);
        if (imagenMSAA_)
            funciones_.vkDestroyImage(dispositivo_, imagenMSAA_, nullptr);
        if (memoriaMSAA_)
            funciones_.vkFreeMemory(dispositivo_, memoriaMSAA_, nullptr);
        if (vistaMSAA_)
            funciones_.vkDestroyImageView(dispositivo_, vistaMSAA_, nullptr);
        if (imagenProfundidad_)
            funciones_.vkDestroyImage(dispositivo_, imagenProfundidad_, nullptr);
        if (memoriaProfundidad_)
            funciones_.vkFreeMemory(dispositivo_, memoriaProfundidad_, nullptr);
        if (vistaProfundidad_)
            funciones_.vkDestroyImageView(dispositivo_, vistaProfundidad_, nullptr);

        if (poolDescriptores_)
            funciones_.vkDestroyDescriptorPool(dispositivo_, poolDescriptores_, nullptr);
        if (poolComandos_)
            funciones_.vkDestroyCommandPool(dispositivo_, poolComandos_, nullptr);
        if (semaforoAdquirido_)
            funciones_.vkDestroySemaphore(dispositivo_, semaforoAdquirido_, nullptr);
        if (semaforoPresentado_)
            funciones_.vkDestroySemaphore(dispositivo_, semaforoPresentado_, nullptr);
        if (fenceFrame_)
            funciones_.vkDestroyFence(dispositivo_, fenceFrame_, nullptr);
    }

    if (instancia_) {
        if (superficie_) vkDestroySurfaceKHR(instancia_, superficie_, nullptr);
        vkDestroyInstance(instancia_, nullptr);
    }
}

// Libera los buffers de uniforms que quedaron retirados: los del frame
// anterior, que ya no estan en vuelo.
void BackendVulkan::liberarBuffersRetirados() {
    for (VkBuffer buffer : buffersRetirados_)
        if (buffer) funciones_.vkDestroyBuffer(dispositivo_, buffer, nullptr);
    for (VkDeviceMemory memoria : memoriasRetiradas_)
        if (memoria) funciones_.vkFreeMemory(dispositivo_, memoria, nullptr);
    buffersRetirados_.clear();
    memoriasRetiradas_.clear();
}

// ---------------------------------------------------------------------------
// Instancia, dispositivo y superficie
// ---------------------------------------------------------------------------

bool BackendVulkan::crearInstancia() {
    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = NombreProducto::kNombre;
    app.applicationVersion = VK_MAKE_VERSION(0, 5, 4);
    app.pEngineName = NombreProducto::kNombre;
    app.engineVersion = VK_MAKE_VERSION(0, 5, 4);
    // Se pide 1.1: es la version minima con la que el motor funciona (con
    // swapchain y render passes clasicos) y la que tiene mas cobertura de
    // dispositivos.
    app.apiVersion = VK_API_VERSION_1_1;

    std::vector<const char*> extensiones;
    // GLFW sabe que extension de superficie necesita la plataforma: la
    // ventana del motor es de GLFW, asi que la superficie tambien.
    uint32_t cantidadExt = 0;
    const char** reqExt = glfwGetRequiredInstanceExtensions(&cantidadExt);
    if (reqExt) {
        for (uint32_t i = 0; i < cantidadExt; ++i)
            extensiones.push_back(reqExt[i]);
    }
    bool tieneSuperficie = false;
    for (const char* e : extensiones)
        if (std::strcmp(e, VK_KHR_SURFACE_EXTENSION_NAME) == 0) tieneSuperficie = true;
    if (!tieneSuperficie && superficie_)
        extensiones.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

    VkInstanceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo = &app;
    info.enabledExtensionCount = static_cast<uint32_t>(extensiones.size());
    info.ppEnabledExtensionNames = extensiones.empty() ? nullptr : extensiones.data();

    if (!ok(vkCreateInstance(&info, nullptr, &instancia_), "vkCreateInstance"))
        return false;
    return true;
}

bool BackendVulkan::crearDispositivo() {
    uint32_t cantidad = 0;
    vkEnumeratePhysicalDevices(instancia_, &cantidad, nullptr);
    if (cantidad == 0) {
        std::cerr << "[Vulkan] No hay ninguna GPU con Vulkan en la maquina."
                  << std::endl;
        return false;
    }
    std::vector<VkPhysicalDevice> gpus(cantidad);
    vkEnumeratePhysicalDevices(instancia_, &cantidad, gpus.data());

    // Se elige la primera GPU que tenga una cola grafica y que pueda presentar
    // en la superficie (si la hay).
    //
    // La variable de entorno FUNSHI_VULKAN_GPU fuerza una GPU concreta (por
    // su indice en la lista del Vulkan). Es para las maquinas donde la GPU
    // por defecto no sirve (un driver roto, por ejemplo) y para comparar el
    // comportamiento entre GPUs.
    VkPhysicalDevice elegido = VK_NULL_HANDLE;
    uint32_t familiaElegida = 0;

    const char* gpuForzada = std::getenv("FUNSHI_VULKAN_GPU");
    const bool hayGpuForzada = gpuForzada && gpuForzada[0];
    const uint32_t indiceGpuForzada =
        hayGpuForzada ? static_cast<uint32_t>(std::atoi(gpuForzada)) : 0;

    for (uint32_t indice = 0; indice < gpus.size(); ++indice) {
        if (hayGpuForzada && indice != indiceGpuForzada) continue;
        VkPhysicalDevice gpu = gpus[indice];
        uint32_t familias = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(gpu, &familias, nullptr);
        std::vector<VkQueueFamilyProperties> props(familias);
        vkGetPhysicalDeviceQueueFamilyProperties(gpu, &familias, props.data());
        for (uint32_t i = 0; i < familias; ++i) {
            if (!(props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) continue;
            if (superficie_) {
                VkBool32 puedePresentar = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, superficie_,
                                                     &puedePresentar);
                if (!puedePresentar) continue;
            }
            elegido = gpu;
            familiaElegida = i;
            break;
        }
        if (elegido) break;
    }

    if (!elegido) {
        std::cerr << "[Vulkan] Ninguna GPU soporta la cola grafica necesaria."
                  << std::endl;
        return false;
    }

    gpuFisico_ = elegido;
    familiaGrafica_ = familiaElegida;
    vkGetPhysicalDeviceMemoryProperties(gpuFisico_, &memoriaGpu_);

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(gpuFisico_, &props);
    std::ostringstream diag;
    diag << "[Vulkan] GPU: " << props.deviceName << " | API "
         << VK_VERSION_MAJOR(props.apiVersion) << "."
         << VK_VERSION_MINOR(props.apiVersion) << "."
         << VK_VERSION_PATCH(props.apiVersion) << " | driver "
         << VK_VERSION_MAJOR(props.driverVersion) << "."
         << VK_VERSION_MINOR(props.driverVersion);
    diagnostico_ = diag.str();

    // Multisample: la version de OpenGL pedia un framebuffer de 4 muestras.
    // En Vulkan casi ningun swapchain expone un formato multisampleado, asi
    // que se dibuja en una imagen multisampleada fuera de pantalla y se
    // resuelve contra la imagen del swapchain.
    muestras_ = 1;
    if (props.limits.framebufferColorSampleCounts & VK_SAMPLE_COUNT_4_BIT)
        muestras_ = 4;

    // Alineacion del offset dinamico de un uniform buffer: define el tamano
    // de cada slice del anillo de uniforms.
    alineacionUniformes_ = props.limits.minUniformBufferOffsetAlignment;
    if (alineacionUniformes_ == 0) alineacionUniformes_ = 256;

    float prioridad = 1.0f;
    VkDeviceQueueCreateInfo infoCola{};
    infoCola.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    infoCola.queueFamilyIndex = familiaElegida;
    infoCola.queueCount = 1;
    infoCola.pQueuePriorities = &prioridad;

    VkPhysicalDeviceFeatures funciones{};
    VkDeviceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    info.queueCreateInfoCount = 1;
    info.pQueueCreateInfos = &infoCola;
    info.pEnabledFeatures = &funciones;

    // Con ventana hay que habilitar la extension del swapchain en el
    // dispositivo: sin ella el swapchain no se puede crear (la extension de
    // la superficie es de nivel de instancia y ya la pidio GLFW).
    std::vector<const char*> extensionesDispositivo;
    if (superficie_) {
        uint32_t cantidadExtensiones = 0;
        vkEnumerateDeviceExtensionProperties(gpuFisico_, nullptr,
                                             &cantidadExtensiones, nullptr);
        std::vector<VkExtensionProperties> disponibles(cantidadExtensiones);
        vkEnumerateDeviceExtensionProperties(gpuFisico_, nullptr,
                                             &cantidadExtensiones,
                                             disponibles.data());
        bool haySwapchain = false;
        for (const VkExtensionProperties& ext : disponibles) {
            if (std::strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) ==
                0) {
                haySwapchain = true;
                break;
            }
        }
        if (!haySwapchain) {
            std::cerr << "[Vulkan] La GPU no soporta la extension del swapchain."
                      << std::endl;
            return false;
        }
        extensionesDispositivo.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }
    info.enabledExtensionCount =
        static_cast<uint32_t>(extensionesDispositivo.size());
    info.ppEnabledExtensionNames = extensionesDispositivo.empty()
                                       ? nullptr
                                       : extensionesDispositivo.data();

    if (!ok(vkCreateDevice(gpuFisico_, &info, nullptr, &dispositivo_),
            "vkCreateDevice"))
        return false;

    vkGetDeviceQueue(dispositivo_, familiaGrafica_, 0, &colaGrafica_);

    // Funciones de dispositivo.
#define VULKAN_CARGAR_FUNCION(NOMBRE)                                         \
    funciones_.NOMBRE = reinterpret_cast<PFN_##NOMBRE>(                        \
        vkGetDeviceProcAddr(dispositivo_, #NOMBRE));
    VULKAN_FUNCIONES_DISPOSITIVO(VULKAN_CARGAR_FUNCION)
#undef VULKAN_CARGAR_FUNCION

    const bool hayFunciones =
        funciones_.vkCreateCommandPool && funciones_.vkAllocateCommandBuffers &&
        funciones_.vkBeginCommandBuffer && funciones_.vkEndCommandBuffer &&
        funciones_.vkQueueSubmit && funciones_.vkCreateBuffer &&
        funciones_.vkAllocateMemory && funciones_.vkCreateImage &&
        funciones_.vkCreateGraphicsPipelines && funciones_.vkCmdDraw &&
        funciones_.vkCreateRenderPass && funciones_.vkCreateFramebuffer;
    if (!hayFunciones) {
        std::cerr << "[Vulkan] El driver no expone las funciones necesarias."
                  << std::endl;
        return false;
    }

    // Pool de command buffers: uno para el frame y otro para las transferencias
    // que se hacen fuera del frame (lectura de pixeles).
    VkCommandPoolCreateInfo infoPool{};
    infoPool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    infoPool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    infoPool.queueFamilyIndex = familiaGrafica_;
    if (!ok(funciones_.vkCreateCommandPool(dispositivo_, &infoPool, nullptr,
                                            &poolComandos_),
            "vkCreateCommandPool"))
        return false;

    VkCommandBufferAllocateInfo infoCmd{};
    infoCmd.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    infoCmd.commandPool = poolComandos_;
    infoCmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    infoCmd.commandBufferCount = 1;
    if (!ok(funciones_.vkAllocateCommandBuffers(dispositivo_, &infoCmd, &comando_),
            "vkAllocateCommandBuffers"))
        return false;
    if (!ok(funciones_.vkAllocateCommandBuffers(dispositivo_, &infoCmd,
                                                &comandoTransferencia_),
            "vkAllocateCommandBuffers"))
        return false;

    // Pool de descriptors: uno por draw (cada draw liga su propio conjunto
    // porque el offset dinamico del bloque de uniforms cambia y las texturas
    // pueden cambiar entre draws).
    VkDescriptorPoolSize tamanos[2]{};
    tamanos[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    tamanos[0].descriptorCount = kMaxConjuntos;
    tamanos[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    tamanos[1].descriptorCount = kMaxConjuntos * kMaxSamplers;

    VkDescriptorPoolCreateInfo infoDescriptores{};
    infoDescriptores.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    infoDescriptores.maxSets = kMaxConjuntos;
    infoDescriptores.poolSizeCount = 2;
    infoDescriptores.pPoolSizes = tamanos;
    if (!ok(funciones_.vkCreateDescriptorPool(dispositivo_, &infoDescriptores,
                                              nullptr, &poolDescriptores_),
            "vkCreateDescriptorPool"))
        return false;

    // Fence del frame: espera a que terminen los comandos del frame anterior.
    // No depende del swapchain (tampoco hay frame sin ventana).
    VkFenceCreateInfo infoFence{};
    infoFence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (!ok(funciones_.vkCreateFence(dispositivo_, &infoFence, nullptr,
                                     &fenceFrame_),
            "vkCreateFence"))
        return false;

    return true;
}

bool BackendVulkan::crearSuperficie(void* ventanaNativa) {
    if (!ventanaNativa) return true;  // modo sin ventana: no hay superficie
    GLFWwindow* ventana = static_cast<GLFWwindow*>(ventanaNativa);
    if (!ok(glfwCreateWindowSurface(instancia_, ventana, nullptr, &superficie_),
            "glfwCreateWindowSurface"))
        return false;
    return true;
}

// ---------------------------------------------------------------------------
// Swapchain
// ---------------------------------------------------------------------------

bool BackendVulkan::crearSwapchain() {
    if (!superficie_) return true;  // sin superficie no hay swapchain

    VkSurfaceCapabilitiesKHR capacidades{};
    if (!ok(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                gpuFisico_, superficie_, &capacidades),
            "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"))
        return false;

    uint32_t cantidadFormatos = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpuFisico_, superficie_,
                                         &cantidadFormatos, nullptr);
    std::vector<VkSurfaceFormatKHR> formatos(cantidadFormatos);
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpuFisico_, superficie_,
                                         &cantidadFormatos, formatos.data());
    if (formatos.empty()) {
        std::cerr << "[Vulkan] La superficie no ofrece ningun formato."
                  << std::endl;
        return false;
    }

    VkSurfaceFormatKHR elegido = formatos[0];
    for (const VkSurfaceFormatKHR& f : formatos) {
        // Sin conversion sRGB: el motor trabaja en valores lineales, que es lo
        // que hacia el framebuffer por defecto de OpenGL.
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            elegido = f;
            break;
        }
    }

    VkExtent2D extension = capacidades.currentExtent;
    if (extension.width == UINT32_MAX) {
        extension.width = 1280;
        extension.height = 720;
    }

    uint32_t cantidadModos = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(gpuFisico_, superficie_,
                                               &cantidadModos, nullptr);
    std::vector<VkPresentModeKHR> modos(cantidadModos);
    vkGetPhysicalDeviceSurfacePresentModesKHR(gpuFisico_, superficie_,
                                               &cantidadModos, modos.data());
    VkPresentModeKHR modo = VK_PRESENT_MODE_FIFO_KHR;
    for (VkPresentModeKHR m : modos) {
        // MAILBOX no espera la senal vertical: el frame se muestra en cuanto
        // la GPU termina. Es lo que se siente fluido en un editor.
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) {
            modo = m;
            break;
        }
    }

    uint32_t imagenes = capacidades.minImageCount + 1;
    if (capacidades.maxImageCount > 0)
        imagenes = std::min(imagenes, capacidades.maxImageCount);

    VkSwapchainCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface = superficie_;
    info.minImageCount = imagenes;
    info.imageFormat = elegido.format;
    info.imageColorSpace = elegido.colorSpace;
    info.imageExtent = extension;
    info.imageArrayLayers = 1;
    // TRANSFER_SRC para que se pueda copiar la imagen a un buffer (lectura de
    // pixeles del frame).
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                      VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.preTransform = capacidades.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode = modo;
    info.clipped = VK_TRUE;
    info.oldSwapchain = VK_NULL_HANDLE;

    if (!ok(vkCreateSwapchainKHR(dispositivo_, &info, nullptr, &swapchain_),
            "vkCreateSwapchainKHR"))
        return false;

    formatoSwapchain_ = elegido.format;
    espacioSwapchain_ = elegido.colorSpace;
    extensionSwapchain_ = extension;
    cantidadImagenesSwapchain_ = imagenes;

    {
        std::ostringstream diag;
        diag << " | swapchain " << extension.width << "x" << extension.height
             << " | " << imagenes << " imagenes | formato "
             << static_cast<int>(elegido.format) << " | "
             << muestras_ << " muestras";
        diagnostico_ += diag.str();
    }
    return true;
}

void BackendVulkan::destruirSwapchain() {
    for (VkFramebuffer framebuffer : framebuffersSwapchain_)
        if (framebuffer)
            funciones_.vkDestroyFramebuffer(dispositivo_, framebuffer, nullptr);
    framebuffersSwapchain_.clear();
    for (VkImageView vista : vistasSwapchain_)
        if (vista) funciones_.vkDestroyImageView(dispositivo_, vista, nullptr);
    vistasSwapchain_.clear();
    if (swapchain_) {
        vkDestroySwapchainKHR(dispositivo_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
    swapchainValido_ = false;
}

// ---------------------------------------------------------------------------
// Render passes
// ---------------------------------------------------------------------------

// Crea el render pass del swapchain.
//
// Solo hay swapchain cuando hay ventana: sin ventana (modo sin superficie) el
// backend dibuja unicamente sobre render targets y este render pass no hace
// falta.
bool BackendVulkan::crearRenderPassSwapchain() {
    // Render pass del swapchain.
    //
    // Con multisample: attachment 0 = color multisampleado (el que se dibuja),
    // attachment 1 = resolve (la imagen del swapchain), attachment 2 = depth.
    // Sin multisample: attachment 0 = color (la imagen del swapchain),
    // attachment 1 = depth.
    //
    // El loadOp es LOAD y el storeOp es STORE: el motor puede dibujar en el
    // swapchain, cambiar a un render target y volver a dibujar, y el contenido
    // de lo ya dibujado tiene que seguir ahi. Por eso el layout inicial de los
    // attachments es el mismo que el final (y no UNDEFINED): con UNDEFINED el
    // contenido se descarta al reabrir la pasada.
    //
    // La limpieza se hace con vkCmdClearAttachments, con el color que el motor
    // pidio.
    VkAttachmentDescription color{};
    color.format = formatoSwapchain_;
    color.samples = static_cast<VkSampleCountFlagBits>(muestras_);
    color.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription resolucion{};
    resolucion.format = formatoSwapchain_;
    resolucion.samples = VK_SAMPLE_COUNT_1_BIT;
    resolucion.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    resolucion.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    resolucion.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    resolucion.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    resolucion.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    resolucion.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription profundidad{};
    profundidad.format = VK_FORMAT_D32_SFLOAT;
    profundidad.samples = static_cast<VkSampleCountFlagBits>(muestras_);
    profundidad.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    profundidad.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    profundidad.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    profundidad.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    profundidad.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    profundidad.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription adjuntos[3];
    uint32_t cantidadAdjuntos = 0;
    if (muestras_ > 1) {
        adjuntos[0] = color;
        adjuntos[1] = resolucion;
        adjuntos[2] = profundidad;
        cantidadAdjuntos = 3;
    } else {
        adjuntos[0] = resolucion;  // el color es la imagen del swapchain
        adjuntos[1] = profundidad;
        cantidadAdjuntos = 2;
    }

    VkAttachmentReference refColor{};
    refColor.attachment = 0;
    refColor.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference refResolucion{};
    refResolucion.attachment = 1;
    refResolucion.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference refProfundidad{};
    refProfundidad.attachment = cantidadAdjuntos - 1;
    refProfundidad.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &refColor;
    if (muestras_ > 1) subpass.pResolveAttachments = &refResolucion;
    subpass.pDepthStencilAttachment = &refProfundidad;

    VkRenderPassCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = cantidadAdjuntos;
    info.pAttachments = adjuntos;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;

    if (!ok(funciones_.vkCreateRenderPass(dispositivo_, &info, nullptr,
                                          &renderPassSwapchain_),
            "vkCreateRenderPass (swapchain)"))
        return false;

    // Render pass de la interfaz: un solo attachment de color (la imagen del
    // swapchain, ya resuelta) y sin profundidad. El pipeline que arma ImGui no
    // lleva estado de profundidad, asi que no puede usarse en la pasada de la
    // escena (que si lo tiene).
    VkAttachmentDescription colorInterfaz{};
    colorInterfaz.format = formatoSwapchain_;
    colorInterfaz.samples = VK_SAMPLE_COUNT_1_BIT;
    colorInterfaz.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorInterfaz.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorInterfaz.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorInterfaz.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorInterfaz.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorInterfaz.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference refColorInterfaz{};
    refColorInterfaz.attachment = 0;
    refColorInterfaz.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpassInterfaz{};
    subpassInterfaz.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassInterfaz.colorAttachmentCount = 1;
    subpassInterfaz.pColorAttachments = &refColorInterfaz;

    VkRenderPassCreateInfo infoInterfaz{};
    infoInterfaz.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    infoInterfaz.attachmentCount = 1;
    infoInterfaz.pAttachments = &colorInterfaz;
    infoInterfaz.subpassCount = 1;
    infoInterfaz.pSubpasses = &subpassInterfaz;

    if (!ok(funciones_.vkCreateRenderPass(dispositivo_, &infoInterfaz, nullptr,
                                          &renderPassInterfaz_),
            "vkCreateRenderPass (interfaz)"))
        return false;

    return true;
}

bool BackendVulkan::crearRenderPasses() {
    // Render pass de los render targets fuera de pantalla: una sola muestra
    // (en OpenGL el FBO de la vista previa tampoco era multisampleado).
    VkAttachmentDescription colorObjetivo{};
    colorObjetivo.format = VK_FORMAT_R8G8B8A8_UNORM;
    colorObjetivo.samples = VK_SAMPLE_COUNT_1_BIT;
    colorObjetivo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorObjetivo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorObjetivo.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorObjetivo.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorObjetivo.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorObjetivo.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription profundidadObjetivo{};
    profundidadObjetivo.format = VK_FORMAT_D32_SFLOAT;
    profundidadObjetivo.samples = VK_SAMPLE_COUNT_1_BIT;
    profundidadObjetivo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    profundidadObjetivo.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    profundidadObjetivo.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    profundidadObjetivo.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    profundidadObjetivo.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    profundidadObjetivo.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription adjuntosObjetivo[2] = {colorObjetivo,
                                                    profundidadObjetivo};

    VkAttachmentReference refColorObjetivo{};
    refColorObjetivo.attachment = 0;
    refColorObjetivo.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference refProfundidadObjetivo{};
    refProfundidadObjetivo.attachment = 1;
    refProfundidadObjetivo.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpassObjetivo{};
    subpassObjetivo.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassObjetivo.colorAttachmentCount = 1;
    subpassObjetivo.pColorAttachments = &refColorObjetivo;
    subpassObjetivo.pDepthStencilAttachment = &refProfundidadObjetivo;

    VkRenderPassCreateInfo infoObjetivo{};
    infoObjetivo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    infoObjetivo.attachmentCount = 2;
    infoObjetivo.pAttachments = adjuntosObjetivo;
    infoObjetivo.subpassCount = 1;
    infoObjetivo.pSubpasses = &subpassObjetivo;

    if (!ok(funciones_.vkCreateRenderPass(dispositivo_, &infoObjetivo, nullptr,
                                          &renderPassObjetivo_),
            "vkCreateRenderPass (render target)"))
        return false;

    // Sin ventana no hay swapchain: el render pass del swapchain no se crea.
    if (formatoSwapchain_ == VK_FORMAT_UNDEFINED) return true;
    return crearRenderPassSwapchain();
}

// ---------------------------------------------------------------------------
// Recursos de imagen del frame
// ---------------------------------------------------------------------------

bool BackendVulkan::crearRecursosDeFrame() {
    if (!swapchain_) return true;

    uint32_t cantidad = 0;
    vkGetSwapchainImagesKHR(dispositivo_, swapchain_, &cantidad, nullptr);
    imagenesSwapchain_.resize(cantidad);
    vkGetSwapchainImagesKHR(dispositivo_, swapchain_, &cantidad,
                             imagenesSwapchain_.data());

    vistasSwapchain_.clear();
    for (VkImage imagen : imagenesSwapchain_) {
        VkImageViewCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.image = imagen;
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = formatoSwapchain_;
        info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.layerCount = 1;
        VkImageView vista = VK_NULL_HANDLE;
        if (!ok(funciones_.vkCreateImageView(dispositivo_, &info, nullptr, &vista),
                "vkCreateImageView (swapchain)"))
            return false;
        vistasSwapchain_.push_back(vista);
    }

    // Imagen multisampleada (color) y de profundidad, del tamano del
    // swapchain.
    if (muestras_ > 1) {
        VkImageCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = formatoSwapchain_;
        info.extent = {extensionSwapchain_.width, extensionSwapchain_.height, 1};
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.samples = VK_SAMPLE_COUNT_4_BIT;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (!ok(funciones_.vkCreateImage(dispositivo_, &info, nullptr, &imagenMSAA_),
                "vkCreateImage (MSAA)"))
            return false;
        VkMemoryRequirements req{};
        funciones_.vkGetImageMemoryRequirements(dispositivo_, imagenMSAA_, &req);
        if (!asignarMemoria(req, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                            memoriaMSAA_))
            return false;
        funciones_.vkBindImageMemory(dispositivo_, imagenMSAA_, memoriaMSAA_, 0);

        VkImageViewCreateInfo infoVista{};
        infoVista.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        infoVista.image = imagenMSAA_;
        infoVista.viewType = VK_IMAGE_VIEW_TYPE_2D;
        infoVista.format = formatoSwapchain_;
        infoVista.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        infoVista.subresourceRange.levelCount = 1;
        infoVista.subresourceRange.layerCount = 1;
        if (!ok(funciones_.vkCreateImageView(dispositivo_, &infoVista, nullptr,
                                             &vistaMSAA_),
                "vkCreateImageView (MSAA)"))
            return false;
    }

    // Profundidad.
    {
        VkImageCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = VK_FORMAT_D32_SFLOAT;
        info.extent = {extensionSwapchain_.width, extensionSwapchain_.height, 1};
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.samples = static_cast<VkSampleCountFlagBits>(muestras_);
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (!ok(funciones_.vkCreateImage(dispositivo_, &info, nullptr,
                                         &imagenProfundidad_),
                "vkCreateImage (profundidad)"))
            return false;
        VkMemoryRequirements req{};
        funciones_.vkGetImageMemoryRequirements(dispositivo_, imagenProfundidad_,
                                                &req);
        if (!asignarMemoria(req, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                            memoriaProfundidad_))
            return false;
        funciones_.vkBindImageMemory(dispositivo_, imagenProfundidad_,
                                     memoriaProfundidad_, 0);

        VkImageViewCreateInfo infoVista{};
        infoVista.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        infoVista.image = imagenProfundidad_;
        infoVista.viewType = VK_IMAGE_VIEW_TYPE_2D;
        infoVista.format = VK_FORMAT_D32_SFLOAT;
        infoVista.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        infoVista.subresourceRange.levelCount = 1;
        infoVista.subresourceRange.layerCount = 1;
        if (!ok(funciones_.vkCreateImageView(dispositivo_, &infoVista, nullptr,
                                             &vistaProfundidad_),
                "vkCreateImageView (profundidad)"))
            return false;
    }

    // Las imagenes del frame arrancan en UNDEFINED y el render pass las
    // espera en el layout de attachment: se dejan ahi antes del primer uso.
    if (muestras_ > 1) {
        transicionarImagen(imagenMSAA_, VK_IMAGE_LAYOUT_UNDEFINED,
                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
                           VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                           VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    }
    transicionarImagen(imagenProfundidad_, VK_IMAGE_LAYOUT_UNDEFINED,
                       VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0,
                       VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                       VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                       VK_IMAGE_ASPECT_DEPTH_BIT);

    // Framebuffer del swapchain: uno por imagen. Cada imagen del swapchain
    // tiene su propia vista, y el render pass se resuelve contra la que se
    // dibujo en este frame.
    framebuffersSwapchain_.clear();
    for (VkImageView vistaImagen : vistasSwapchain_) {
        VkImageView adjuntosFB[3];
        uint32_t cantidadFB = 0;
        if (muestras_ > 1) {
            adjuntosFB[0] = vistaMSAA_;
            adjuntosFB[1] = vistaImagen;
            adjuntosFB[2] = vistaProfundidad_;
            cantidadFB = 3;
        } else {
            adjuntosFB[0] = vistaImagen;
            adjuntosFB[1] = vistaProfundidad_;
            cantidadFB = 2;
        }

        VkFramebufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = renderPassSwapchain_;
        info.attachmentCount = cantidadFB;
        info.pAttachments = adjuntosFB;
        info.width = extensionSwapchain_.width;
        info.height = extensionSwapchain_.height;
        info.layers = 1;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        if (!ok(funciones_.vkCreateFramebuffer(dispositivo_, &info, nullptr,
                                               &framebuffer),
                "vkCreateFramebuffer (swapchain)"))
            return false;
        framebuffersSwapchain_.push_back(framebuffer);
    }

    // Framebuffers de la interfaz: uno por imagen del swapchain, con la sola
    // vista de color (la imagen ya resuelta por la pasada de la escena).
    for (VkFramebuffer framebuffer : framebuffersInterfaz_)
        if (framebuffer)
            funciones_.vkDestroyFramebuffer(dispositivo_, framebuffer, nullptr);
    framebuffersInterfaz_.clear();
    for (VkImageView vistaImagen : vistasSwapchain_) {
        VkFramebufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = renderPassInterfaz_;
        info.attachmentCount = 1;
        info.pAttachments = &vistaImagen;
        info.width = extensionSwapchain_.width;
        info.height = extensionSwapchain_.height;
        info.layers = 1;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        if (!ok(funciones_.vkCreateFramebuffer(dispositivo_, &info, nullptr,
                                               &framebuffer),
                "vkCreateFramebuffer (interfaz)"))
            return false;
        framebuffersInterfaz_.push_back(framebuffer);
    }

    // Semaforos del frame (adquirir y presentar). El fence del frame se crea
    // con el dispositivo: no depende del swapchain.
    VkSemaphoreCreateInfo infoSema{};
    infoSema.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    if (!ok(funciones_.vkCreateSemaphore(dispositivo_, &infoSema, nullptr,
                                         &semaforoAdquirido_),
            "vkCreateSemaphore"))
        return false;
    if (!ok(funciones_.vkCreateSemaphore(dispositivo_, &infoSema, nullptr,
                                         &semaforoPresentado_),
            "vkCreateSemaphore"))
        return false;

    swapchainValido_ = true;
    return true;
}

// ---------------------------------------------------------------------------
// Memoria
// ---------------------------------------------------------------------------

uint32_t BackendVulkan::indiceMemoryType(uint32_t bits,
                                          VkMemoryPropertyFlags props) const {
    for (uint32_t i = 0; i < memoriaGpu_.memoryTypeCount; ++i) {
        if ((bits & (1u << i)) &&
            (memoriaGpu_.memoryTypes[i].propertyFlags & props) == props)
            return i;
    }
    return UINT32_MAX;
}

bool BackendVulkan::asignarMemoria(VkMemoryRequirements req,
                                   VkMemoryPropertyFlags props,
                                   VkDeviceMemory& memoria) {
    VkMemoryAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    info.allocationSize = req.size;
    info.memoryTypeIndex = indiceMemoryType(req.memoryTypeBits, props);
    if (info.memoryTypeIndex == UINT32_MAX) {
        std::cerr << "[Vulkan] No hay un tipo de memoria con las propiedades "
                     "necesarias."
                  << std::endl;
        return false;
    }
    return ok(funciones_.vkAllocateMemory(dispositivo_, &info, nullptr, &memoria),
              "vkAllocateMemory");
}

// ---------------------------------------------------------------------------
// Ciclo de vida
// ---------------------------------------------------------------------------

bool BackendVulkan::init(void* ventanaNativa) {
    if (!crearInstancia()) return false;
    if (!crearSuperficie(ventanaNativa)) return false;
    if (!crearDispositivo()) return false;
    if (!crearSwapchain()) return false;
    if (!crearRenderPasses()) return false;
    if (!crearRecursosDeFrame()) return false;

    // Textura blanca de 1x1: la que se liga en un sampler sin textura, para
    // que el descriptor sea valido (un descriptor sin imagen no es valido).
    unsigned char blanco[4] = {255, 255, 255, 255};
    if (!crearTextura(blanco, 1, 1, false, false, texturaBlanca_))
        return false;

    return true;
}

bool BackendVulkan::available() const {
    return dispositivo_ != VK_NULL_HANDLE;
}

// ---------------------------------------------------------------------------
// Frame
// ---------------------------------------------------------------------------

void BackendVulkan::beginFrame() {
    if (!dispositivo_) return;

    // Se espera el frame anterior: el motor es un editor y la serializacion
    // CPU/GPU no se nota, y elimina toda una clase de carreras (recursos en
    // vuelo, descriptor sets reutilizados).
    if (hayFramePendiente_ && fenceFrame_) {
        vkWaitForFences(dispositivo_, 1, &fenceFrame_, VK_TRUE, UINT64_MAX);
        hayFramePendiente_ = false;
    }

    // Command buffer del frame.
    funciones_.vkResetCommandBuffer(comando_, 0);
    VkCommandBufferBeginInfo infoBegin{};
    infoBegin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    infoBegin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    funciones_.vkBeginCommandBuffer(comando_, &infoBegin);
    comandoAbierto_ = true;

    // Los buffers de uniforms que quedaron retiring se liberan ahora: el
    // frame anterior ya termino y nadie los referencia.
    liberarBuffersRetirados();

    // El pool de descriptors se vacia en cada frame: los conjuntos del frame
    // anterior ya no estan en vuelo (el fence se espero arriba).
    if (poolDescriptores_)
        funciones_.vkResetDescriptorPool(dispositivo_, poolDescriptores_, 0);
    for (auto& par : programasPorTipo_) {
        ProgramaVulkan& p = *par.second;
        p.conjuntos.clear();
        p.conjuntoValido.clear();
        p.siguienteSlice = 0;
    }

    // Si la ventana cambio de tamano (o el swapchain quedo desactualizado),
    // se rehace.
    if (superficie_) {
        VkSurfaceCapabilitiesKHR capacidades{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpuFisico_, superficie_,
                                                 &capacidades);
        const bool cambioTamano =
            capacidades.currentExtent.width != extensionSwapchain_.width ||
            capacidades.currentExtent.height != extensionSwapchain_.height;
        if (cambioTamano || recrearSwapchain_) {
            recrearSwapchain_ = false;
            vkDeviceWaitIdle(dispositivo_);
            destruirSwapchain();
            if (vistaMSAA_) {
                funciones_.vkDestroyImageView(dispositivo_, vistaMSAA_, nullptr);
                vistaMSAA_ = VK_NULL_HANDLE;
            }
            if (imagenMSAA_) {
                funciones_.vkDestroyImage(dispositivo_, imagenMSAA_, nullptr);
                imagenMSAA_ = VK_NULL_HANDLE;
            }
            if (memoriaMSAA_) {
                funciones_.vkFreeMemory(dispositivo_, memoriaMSAA_, nullptr);
                memoriaMSAA_ = VK_NULL_HANDLE;
            }
            if (vistaProfundidad_) {
                funciones_.vkDestroyImageView(dispositivo_, vistaProfundidad_,
                                              nullptr);
                vistaProfundidad_ = VK_NULL_HANDLE;
            }
            if (imagenProfundidad_) {
                funciones_.vkDestroyImage(dispositivo_, imagenProfundidad_, nullptr);
                imagenProfundidad_ = VK_NULL_HANDLE;
            }
            if (memoriaProfundidad_) {
                funciones_.vkFreeMemory(dispositivo_, memoriaProfundidad_, nullptr);
                memoriaProfundidad_ = VK_NULL_HANDLE;
            }
            if (renderPassSwapchain_) {
                funciones_.vkDestroyRenderPass(dispositivo_, renderPassSwapchain_,
                                               nullptr);
                renderPassSwapchain_ = VK_NULL_HANDLE;
            }
            if (crearSwapchain() && crearRenderPasses() && crearRecursosDeFrame())
                swapchainValido_ = true;
        }
    }

    // Adquirir la imagen del swapchain.
    swapchainAdquirido_ = false;
    if (swapchainValido_) {
        VkResult resultado = vkAcquireNextImageKHR(
            dispositivo_, swapchain_, UINT64_MAX, semaforoAdquirido_, nullptr,
            &indiceSwapchain_);
        if (resultado == VK_SUCCESS || resultado == VK_SUBOPTIMAL_KHR) {
            swapchainAdquirido_ = true;
            if (resultado == VK_SUBOPTIMAL_KHR) recrearSwapchain_ = true;
        } else if (resultado == VK_ERROR_OUT_OF_DATE_KHR) {
            swapchainValido_ = false;
        } else {
            reportarError("vkAcquireNextImageKHR", resultado);
            swapchainValido_ = false;
        }
    }

    // La imagen adquirida pasa al layout de attachment de color: el render
    // pass la espera ahi. La transicion se graba con la etapa de color, que
    // es la misma en la que espera el semaforo de la adquisicion, asi que no
    // se adelanta a la espera.
    if (swapchainAdquirido_ && !imagenesSwapchain_.empty()) {
        VkImageMemoryBarrier barrera{};
        barrera.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrera.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrera.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrera.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrera.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrera.image = imagenesSwapchain_[indiceSwapchain_];
        barrera.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrera.subresourceRange.levelCount = 1;
        barrera.subresourceRange.layerCount = 1;
        barrera.srcAccessMask = 0;
        barrera.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        funciones_.vkCmdPipelineBarrier(
            comando_, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0,
            nullptr, 1, &barrera);
    }

    destino_ = DestinoActual::Ninguno;
    destinoObjetivo_ = kInvalidHandle;
    pendienteLimpiar_ = false;
    imagenEnPresentacion_ = false;
}

void BackendVulkan::endFrame() {
    if (!comandoAbierto_) return;

    cerrarRenderPass();

    // La imagen del swapchain vuelve al layout de presentacion antes de
    // mostrarse: el render pass la deja en COLOR_ATTACHMENT_OPTIMAL. Si la
    // pasada de la interfaz ya se abrio, la imagen quedo en el layout de
    // presentacion y no hay que volver a transicionarla.
    if (swapchainAdquirido_ && !imagenesSwapchain_.empty() &&
        !imagenEnPresentacion_) {
        VkImageMemoryBarrier barrera{};
        barrera.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrera.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrera.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrera.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrera.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrera.image = imagenesSwapchain_[indiceSwapchain_];
        barrera.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrera.subresourceRange.levelCount = 1;
        barrera.subresourceRange.layerCount = 1;
        barrera.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrera.dstAccessMask = 0;
        funciones_.vkCmdPipelineBarrier(
            comando_, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1,
            &barrera);
    }

    funciones_.vkEndCommandBuffer(comando_);

    VkSubmitInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.commandBufferCount = 1;
    info.pCommandBuffers = &comando_;

    VkPipelineStageFlags etapa = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    if (swapchainAdquirido_) {
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &semaforoAdquirido_;
        info.pWaitDstStageMask = &etapa;
        info.signalSemaphoreCount = 1;
        info.pSignalSemaphores = &semaforoPresentado_;
    }

    funciones_.vkResetFences(dispositivo_, 1, &fenceFrame_);
    VkResult resultado = funciones_.vkQueueSubmit(colaGrafica_, 1, &info, fenceFrame_);
    if (resultado != VK_SUCCESS) reportarError("vkQueueSubmit", resultado);
    hayFramePendiente_ = true;
    comandoAbierto_ = false;

    if (swapchainAdquirido_) {
        VkPresentInfoKHR infoPresentacion{};
        infoPresentacion.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        infoPresentacion.waitSemaphoreCount = 1;
        infoPresentacion.pWaitSemaphores = &semaforoPresentado_;
        infoPresentacion.swapchainCount = 1;
        infoPresentacion.pSwapchains = &swapchain_;
        infoPresentacion.pImageIndices = &indiceSwapchain_;
        resultado = vkQueuePresentKHR(colaGrafica_, &infoPresentacion);
        if (resultado == VK_ERROR_OUT_OF_DATE_KHR ||
            resultado == VK_SUBOPTIMAL_KHR) {
            // El swapchain hay que rehacerlo: se marca para el proximo frame.
            recrearSwapchain_ = true;
        } else if (resultado != VK_SUCCESS) {
            reportarError("vkQueuePresentKHR", resultado);
        }
    }
}

// ---------------------------------------------------------------------------
// Estado
// ---------------------------------------------------------------------------

void BackendVulkan::setViewport(int x, int y, int width, int height) {
    viewport_[0] = x;
    viewport_[1] = y;
    viewport_[2] = width;
    viewport_[3] = height;
}

void BackendVulkan::clearScreen(const float color[3]) {
    if (color) {
        colorLimpieza_[0] = color[0];
        colorLimpieza_[1] = color[1];
        colorLimpieza_[2] = color[2];
    }
    pendienteLimpiar_ = true;

    if (!comandoAbierto_) return;

    // Si todavia no se abrio ninguna pasada, se abre la del swapchain (el
    // destino por defecto del frame).
    if (destino_ == DestinoActual::Ninguno)
        abrirRenderPass(DestinoActual::Swapchain);
    if (destino_ == DestinoActual::Ninguno) return;

    // La limpieza va dentro de la pasada: en Vulkan no se puede limpiar un
    // attachment fuera de ella.
    VkClearAttachment adjuntos[2]{};
    adjuntos[0].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    adjuntos[0].colorAttachment = indiceAttachmentColor_;
    adjuntos[0].clearValue.color = {{colorLimpieza_[0], colorLimpieza_[1],
                                     colorLimpieza_[2], 1.0f}};
    adjuntos[1].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    adjuntos[1].colorAttachment = indiceAttachmentProfundidad_;
    adjuntos[1].clearValue.depthStencil = {1.0f, 0};

    VkClearRect rect{};
    rect.baseArrayLayer = 0;
    rect.layerCount = 1;
    VkExtent2D area = extensionDestino(destino_);
    rect.rect = {{0, 0},
                 {viewport_[2] > 0 ? viewport_[2] : area.width,
                  viewport_[3] > 0 ? viewport_[3] : area.height}};

    funciones_.vkCmdClearAttachments(comando_, 2, adjuntos, 1, &rect);
    pendienteLimpiar_ = false;
}

void BackendVulkan::applyBaseState() {
    estadoBase_ = ClaveEstado();
    estado_ = estadoBase_;
}

void BackendVulkan::setClearColor(const float color[3]) {
    if (!color) return;
    colorLimpieza_[0] = color[0];
    colorLimpieza_[1] = color[1];
    colorLimpieza_[2] = color[2];
}

const char* BackendVulkan::diagnosticoGPU() const {
    return diagnostico_.c_str();
}

void BackendVulkan::setBlendEnabled(bool enabled) {
    estado_.blend = enabled;
}

void BackendVulkan::setDepthTestEnabled(bool enabled) {
    estado_.testProfundidad = enabled;
}

void BackendVulkan::setDepthMask(bool enabled) {
    estado_.escrituraProfundidad = enabled;
}

void BackendVulkan::setDepthFunc(DepthFunc func) {
    estado_.funcion = func;
}

} // namespace Vulkan
} // namespace Backend
} // namespace Rendering
