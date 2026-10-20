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
#ifndef BACKENDVULKAN_H
#define BACKENDVULKAN_H

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <vulkan/vulkan.h>

#include "../Backend/IRenderBackend.h"
#include "LayoutUniformes.h"

namespace Rendering {
namespace Backend {
namespace Vulkan {

// ---------------------------------------------------------------------------
// Funciones de dispositivo
// ---------------------------------------------------------------------------

// Las funciones de nivel de instancia se llaman directo (el loader esta
// enlazado). Las de nivel de dispositivo se cargan con vkGetDeviceProcAddr,
// que es lo que exige la especificacion: una funcion de dispositivo pertenece
// a un dispositivo concreto, y el loader puede tener varios.
#define VULKAN_FUNCIONES_DISPOSITIVO(F)                                        \
    F(vkDestroyDevice)                                                        \
    F(vkDeviceWaitIdle)                                                       \
    F(vkGetDeviceQueue)                                                       \
    F(vkCreateCommandPool)                                                    \
    F(vkDestroyCommandPool)                                                   \
    F(vkResetCommandPool)                                                     \
    F(vkAllocateCommandBuffers)                                               \
    F(vkFreeCommandBuffers)                                                   \
    F(vkBeginCommandBuffer)                                                   \
    F(vkEndCommandBuffer)                                                     \
    F(vkResetCommandBuffer)                                                   \
    F(vkQueueSubmit)                                                          \
    F(vkQueueWaitIdle)                                                        \
    F(vkCreateSemaphore)                                                      \
    F(vkDestroySemaphore)                                                     \
    F(vkCreateFence)                                                          \
    F(vkDestroyFence)                                                         \
    F(vkGetFenceStatus)                                                       \
    F(vkWaitForFences)                                                        \
    F(vkResetFences)                                                          \
    F(vkCreateBuffer)                                                         \
    F(vkDestroyBuffer)                                                        \
    F(vkGetBufferMemoryRequirements)                                          \
    F(vkAllocateMemory)                                                       \
    F(vkFreeMemory)                                                           \
    F(vkBindBufferMemory)                                                     \
    F(vkMapMemory)                                                            \
    F(vkUnmapMemory)                                                          \
    F(vkFlushMappedMemoryRanges)                                              \
    F(vkCreateImage)                                                          \
    F(vkDestroyImage)                                                         \
    F(vkGetImageMemoryRequirements)                                           \
    F(vkBindImageMemory)                                                      \
    F(vkCreateImageView)                                                      \
    F(vkDestroyImageView)                                                     \
    F(vkGetImageSubresourceLayout)                                            \
    F(vkCreateSampler)                                                        \
    F(vkDestroySampler)                                                       \
    F(vkCreateShaderModule)                                                   \
    F(vkDestroyShaderModule)                                                  \
    F(vkCreateDescriptorSetLayout)                                            \
    F(vkDestroyDescriptorSetLayout)                                           \
    F(vkCreateDescriptorPool)                                                 \
    F(vkDestroyDescriptorPool)                                                \
    F(vkResetDescriptorPool)                                                  \
    F(vkAllocateDescriptorSets)                                               \
    F(vkUpdateDescriptorSets)                                                 \
    F(vkCreatePipelineLayout)                                                 \
    F(vkDestroyPipelineLayout)                                                \
    F(vkCreateGraphicsPipelines)                                              \
    F(vkDestroyPipeline)                                                      \
    F(vkCreateRenderPass)                                                     \
    F(vkDestroyRenderPass)                                                    \
    F(vkCreateFramebuffer)                                                    \
    F(vkDestroyFramebuffer)                                                   \
    F(vkCmdBindPipeline)                                                      \
    F(vkCmdSetViewport)                                                       \
    F(vkCmdSetScissor)                                                        \
    F(vkCmdBindDescriptorSets)                                                \
    F(vkCmdPushConstants)                                                     \
    F(vkCmdBindVertexBuffers)                                                 \
    F(vkCmdBindIndexBuffer)                                                   \
    F(vkCmdDraw)                                                              \
    F(vkCmdDrawIndexed)                                                       \
    F(vkCmdPipelineBarrier)                                                   \
    F(vkCmdBeginRenderPass)                                                   \
    F(vkCmdEndRenderPass)                                                     \
    F(vkCmdClearAttachments)                                                  \
    F(vkCmdCopyBuffer)                                                        \
    F(vkCmdCopyBufferToImage)                                                 \
    F(vkCmdCopyImageToBuffer)                                                  \
    F(vkCmdBlitImage)

#define VULKAN_DECLARAR_FUNCION(NOMBRE) PFN_##NOMBRE NOMBRE = nullptr;

struct FuncionesDispositivo {
    VULKAN_FUNCIONES_DISPOSITIVO(VULKAN_DECLARAR_FUNCION)
};

#undef VULKAN_DECLARAR_FUNCION

// ---------------------------------------------------------------------------
// Estado por tipo de programa
// ---------------------------------------------------------------------------

// Destino de lo que se esta dibujando.
enum class DestinoActual {
    Ninguno,
    Swapchain,
    RenderTarget,
    Interfaz
};

// Clave del estado que el pipeline tiene que tener: un pipeline distinto por
// combinacion, porque en Vulkan el estado de profundidad y de blend es del
// pipeline y no se puede cambiar entre draws.
struct ClaveEstado {
    bool testProfundidad = true;
    bool escrituraProfundidad = true;
    bool blend = false;
    DepthFunc funcion = kDepthFuncLess;

    bool operator==(const ClaveEstado& otro) const {
        return testProfundidad == otro.testProfundidad &&
               escrituraProfundidad == otro.escrituraProfundidad &&
               blend == otro.blend && funcion == otro.funcion;
    }
};

struct ClaveEstadoHasher {
    std::size_t operator()(const ClaveEstado& c) const {
        std::size_t h = static_cast<std::size_t>(c.testProfundidad);
        h = h * 31 + static_cast<std::size_t>(c.escrituraProfundidad);
        h = h * 31 + static_cast<std::size_t>(c.blend);
        h = h * 31 + static_cast<std::size_t>(c.funcion);
        return h;
    }
};

// Clave del pipeline: el estado y el destino.
//
// El destino es parte de la clave porque un pipeline se crea contra un render
// pass y dos render passes no son compatibles entre si: el del swapchain
// dibuja sobre una imagen con multisample y resuelve contra la del swapchain,
// y el de los render targets dibuja sobre una imagen de una sola muestra. Un
// pipeline creado contra uno de ellos no se puede usar en el otro.
struct ClavePipeline {
    ClaveEstado estado;
    DestinoActual destino = DestinoActual::Swapchain;

    bool operator==(const ClavePipeline& otro) const {
        return estado == otro.estado && destino == otro.destino;
    }
};

struct ClavePipelineHasher {
    std::size_t operator()(const ClavePipeline& c) const {
        std::size_t h = static_cast<std::size_t>(c.destino);
        h = h * 31 + static_cast<std::size_t>(c.estado.testProfundidad);
        h = h * 31 + static_cast<std::size_t>(c.estado.escrituraProfundidad);
        h = h * 31 + static_cast<std::size_t>(c.estado.blend);
        h = h * 31 + static_cast<std::size_t>(c.estado.funcion);
        return h;
    }
};

// Pipeline de un tipo de programa con una combinacion de estado.
struct PipelineVulkan {
    VkPipeline pipeline = VK_NULL_HANDLE;
    ClaveEstado estado;
};

// Estado compartido por todos los programas del mismo tipo. El motor crea un
// programa por renderer, pero el pipeline, el layout y el bloque de uniforms
// son los mismos: lo que cambia entre un renderer y otro son los valores de
// los uniforms, y para eso esta el anillo de slices.
struct ProgramaVulkan {
    Contrato::ShaderKind kind = Contrato::ShaderKind::Malla;
    LayoutUniformes layout;
    // Cuantos programas del motor usan este. El motor crea un programa por
    // renderer, pero el pipeline, el layout y el bloque de uniforms son los
    // mismos: lo que cambia entre un renderer y otro son los valores de los
    // uniforms, y para eso esta el anillo de slices.
    int refs = 0;

    VkDescriptorSetLayout layoutDescriptores = VK_NULL_HANDLE;
    VkPipelineLayout layoutPipeline = VK_NULL_HANDLE;
    VkShaderModule moduloVertex = VK_NULL_HANDLE;
    VkShaderModule moduloFragment = VK_NULL_HANDLE;

    // Bloque de uniforms: un anillo de slices, cada una de un tamano multiple
    // del minUniformBufferOffsetAlignment. Cada draw usa una slice propia, con
    // offset dinamico.
    VkBuffer bufferUniformes = VK_NULL_HANDLE;
    VkDeviceMemory memoriaUniformes = VK_NULL_HANDLE;
    void* mapaUniformes = nullptr;
    int tamanoSlice = 0;
    int slicesTotales = 0;
    int siguienteSlice = 0;

    // Valores actuales (los que escribe el motor). Se copian a la slice del
    // draw en curso.
    std::vector<std::uint8_t> valores;

    // Samplers: que textura hay en cada unidad de texturizado.
    std::vector<Handle> texturasPorUnidad;
    std::vector<VkDescriptorSet> conjuntos;
    std::vector<bool> conjuntoValido;

    // Unidad de texturizado que el motor le asigna a cada sampler. El motor
    // la escribe con setUniformInt sobre el token del sampler (en OpenGL era
    // la unidad a la que apuntania el sampler). Por defecto es el indice del
    // sampler, que es la convencion del contrato (difusa 0, especular 1,
    // emision 2, normal 3).
    std::vector<int> unidadesSampler;

    // Un pipeline por combinacion de estado y de destino. La clave es la
    // combinacion y el hash el functor que la convierte.
    std::unordered_map<ClavePipeline, PipelineVulkan, ClavePipelineHasher>
        pipelines;
};

// ---------------------------------------------------------------------------
// Recursos
// ---------------------------------------------------------------------------

struct MallaVulkan {
    VkBuffer vertices = VK_NULL_HANDLE;
    VkDeviceMemory memoriaVertices = VK_NULL_HANDLE;
    VkBuffer indices = VK_NULL_HANDLE;
    VkDeviceMemory memoriaIndices = VK_NULL_HANDLE;
    std::uint32_t indicesCount = 0;
};

struct BatchLineasVulkan {
    VkBuffer vertices = VK_NULL_HANDLE;
    VkDeviceMemory memoriaVertices = VK_NULL_HANDLE;
    // El batch se reescribe en cada frame, asi que su memoria se deja mapeada
    // de forma permanente: la actualizacion es un memcpy al puntero mapeado,
    // sin buffer de paso ni copia de transferencia por frame.
    void* mapaVertices = nullptr;
    std::uint32_t verticesCount = 0;
    std::size_t capacidad = 0;
};

struct TexturaVulkan {
    VkImage imagen = VK_NULL_HANDLE;
    VkDeviceMemory memoria = VK_NULL_HANDLE;
    VkImageView vista = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    int ancho = 0;
    int alto = 0;
    int niveles = 1;
    bool esCubemap = false;
};

struct RenderTargetVulkan {
    VkImage color = VK_NULL_HANDLE;
    VkDeviceMemory memoriaColor = VK_NULL_HANDLE;
    VkImageView vistaColor = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VkImage profundidad = VK_NULL_HANDLE;
    VkDeviceMemory memoriaProfundidad = VK_NULL_HANDLE;
    VkImageView vistaProfundidad = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    int ancho = 0;
    int alto = 0;
    // La imagen de color esta en SHADER_READ_ONLY_OPTIMAL (se puede
    // muestrear) y no en COLOR_ATTACHMENT_OPTIMAL (se puede dibujar en ella).
    bool colorEnShaderRead = false;
};

// ---------------------------------------------------------------------------
// El backend
// ---------------------------------------------------------------------------

// Implementacion del contrato de backend grafico sobre Vulkan.
class BackendVulkan : public IRenderBackend {
public:
    BackendVulkan();
    ~BackendVulkan() override;

    BackendVulkan(const BackendVulkan&) = delete;
    BackendVulkan& operator=(const BackendVulkan&) = delete;

    // --- Ciclo de vida -----------------------------------------------------
    bool init(void* ventanaNativa) override;
    bool available() const override;
    void beginFrame() override;
    void endFrame() override;

    // --- Malla GPU ---------------------------------------------------------
    Handle createMesh(const MeshData& data) override;
    void destroyMesh(Handle mesh) override;
    void drawMesh(Handle mesh, unsigned int indexCount) override;

    // --- Batch de lineas ---------------------------------------------------
    Handle createLineBatch(const float* vertices,
                           std::size_t vertexCount) override;
    void destroyLineBatch(Handle batch) override;
    void updateLineBatch(Handle batch, const float* vertices,
                         std::size_t vertexCount) override;
    void drawLineBatch(Handle batch, unsigned int vertexCount) override;

    // --- Texturas ----------------------------------------------------------
    Handle createTexture2D(const Image2D& image) override;
    void destroyTexture2D(Handle texture) override;
    void bindTexture2D(Handle texture, int unit) override;
    void updateTexture2D(Handle texture, const Image2D& image) override;
    Handle createTextureCube(const ImageCube& image) override;
    void destroyTextureCube(Handle texture) override;
    void bindTextureCube(Handle texture, int unit) override;

    // --- Programas ---------------------------------------------------------
    Handle createProgram(Contrato::ShaderKind kind) override;
    void destroyProgram(Handle program) override;
    void useProgram(Handle program) override;
    int uniformLocation(Handle program, const char* name) override;
    void setUniformMat4(int location, const glm::mat4& value) override;
    void setUniformMat3(int location, const glm::mat3& value) override;
    void setUniformVec3(int location, const glm::vec3& value) override;
    void setUniformVec4(int location, const float* value) override;
    void setUniformFloat(int location, float value) override;
    void setUniformInt(int location, int value) override;

    // --- Render targets ----------------------------------------------------
    Handle createRenderTarget(int width, int height) override;
    void destroyRenderTarget(Handle target) override;
    void bindRenderTarget(Handle target) override;
    void bindDefaultFramebuffer() override;
    Handle renderTargetColorTexture(Handle target) const override;
    void* imguiTextureId(Handle texture) const override;

    // --- Lectura de pixeles ------------------------------------------------
    // Copia la imagen de color de un render target y devuelve sus pixeles en
    // RGBA de 8 bits por pixel (cuatro bytes por pixel, en orden de filas de
    // arriba abajo). Se llama fuera de un frame: la copia se graba y se
    // espera por separado.
    bool leerPixeles(Handle target, std::vector<unsigned char>& salida);

    // --- Estado ------------------------------------------------------------
    void setViewport(int x, int y, int width, int height) override;    void clearScreen(const float color[3]) override;
    void applyBaseState() override;
    void setClearColor(const float color[3]) override;
    const char* diagnosticoGPU() const override;
    void setBlendEnabled(bool enabled) override;
    void setDepthTestEnabled(bool enabled) override;
    void setDepthMask(bool enabled) override;
    void setDepthFunc(DepthFunc func) override;
    void drawFullscreenTriangle() override;

    // Acceso a lo que necesita la interfaz (backend de ImGui para Vulkan).
    VkInstance instancia() const { return instancia_; }
    VkDevice dispositivo() const { return dispositivo_; }
    VkPhysicalDevice gpuFisico() const { return gpuFisico_; }
    VkQueue colaGrafica() const { return colaGrafica_; }
    uint32_t indiceFamiliaGrafica() const { return familiaGrafica_; }
    const FuncionesDispositivo& funciones() const { return funciones_; }
    VkCommandPool poolComandos() const { return poolComandos_; }
    VkPhysicalDeviceMemoryProperties memoriaGpu() const { return memoriaGpu_; }
    VkRenderPass renderPassInterfaz() const { return renderPassInterfaz_; }
    uint32_t cantidadImagenesSwapchain() const { return cantidadImagenesSwapchain_; }

    // --- Interfaz ----------------------------------------------------------
    // La interfaz se dibuja en una pasada propia, sobre la imagen del
    // swapchain y sin profundidad: el pipeline que arma ImGui no lleva
    // estado de profundidad y no puede usarse en la pasada de la escena.
    //
    // La pasada se abre y se cierra desde la fachada de la interfaz, que es
    // la que sabe enviar los draw lists al backend de ImGui.
    void abrirPasadaInterfaz();
    void cerrarPasadaInterfaz();
    VkCommandBuffer comandoActual() const { return comando_; }

    // Datos de la textura que la interfaz muestra: la vista, el sampler y el
    // layout de la imagen. El backend de ImGui necesita los tres para crear
    // su descriptor.
    struct TexturaInterfaz {
        VkImageView vista = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    };
    bool texturaParaInterfaz(Handle textura, TexturaInterfaz& salida) const;

private:
    // --- Creacion de recursos ---------------------------------------------
    bool crearInstancia();
    bool crearDispositivo();
    bool crearSuperficie(void* ventanaNativa);
    bool crearSwapchain();
    void destruirSwapchain();
    bool crearRenderPasses();
    bool crearRenderPassSwapchain();
    bool crearRecursosDeFrame();
    bool crearPrograma(Contrato::ShaderKind kind);
    VkPipeline crearPipeline(ProgramaVulkan& programa, const ClavePipeline& clave);
    VkPipeline pipelinePara(ProgramaVulkan& programa, const ClaveEstado& estado,
                            DestinoActual destino);
    bool crearTextura(const unsigned char* pixeles, int ancho, int alto,
                       bool cubemap, bool mipmaps, TexturaVulkan& salida);
    bool subirPixeles(VkImage imagen, const unsigned char* pixeles, int ancho,
                      int alto, bool cubemap, int niveles, VkImageLayout origen);
    bool crearBufferVacio(std::size_t bytes, VkBufferUsageFlags uso,
                          VkBuffer& buffer, VkDeviceMemory& memoria,
                          VkMemoryPropertyFlags propiedades =
                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    bool crearBufferSubiendo(const void* datos, std::size_t bytes,
                              VkBufferUsageFlags uso, VkBuffer& buffer,
                              VkDeviceMemory& memoria);
    bool crearBatchLineas(BatchLineasVulkan& batch, std::size_t bytes);
    void liberarBatchLineas(BatchLineasVulkan& batch);
    bool crearRenderTarget(int ancho, int alto, RenderTargetVulkan& salida);
    VkShaderModule moduloDesdeSpirv(const std::uint32_t* codigo,
                                     std::size_t palabras);

    // --- Grabacion ---------------------------------------------------------
    void abrirRenderPass(DestinoActual destino);
    void cerrarRenderPass();
    VkExtent2D extensionDestino(DestinoActual destino) const;
    void ligarPipeline(Contrato::ShaderKind kind);
    VkDescriptorSet prepararDescriptorSet(ProgramaVulkan& programa);
    void escribirSlice(ProgramaVulkan& programa);
    void ajustarCapacidadUniformes(ProgramaVulkan& programa, int slices);

    // --- Utilidades --------------------------------------------------------
    bool hayFrame() const { return comandoAbierto_; }
    uint32_t indiceMemoryType(uint32_t bits, VkMemoryPropertyFlags props) const;
    bool asignarMemoria(VkMemoryRequirements req,
                        VkMemoryPropertyFlags props,
                        VkDeviceMemory& memoria);
    Handle registrarMalla(MallaVulkan& malla);
    Handle registrarBatch(BatchLineasVulkan& batch);
    Handle registrarTextura(TexturaVulkan& textura);
    Handle registrarRenderTarget(RenderTargetVulkan& target);
    Handle registrarPrograma(Contrato::ShaderKind kind);
    void copiarImagenABuffer(VkImage imagen, VkImageLayout origen,
                              VkBuffer buffer, uint32_t ancho, uint32_t alto);
    void transicionarImagen(VkImage imagen, VkImageLayout origen,
                             VkImageLayout destino,
                             VkAccessFlags accesoOrigen,
                             VkAccessFlags accesoDestino,
                             VkPipelineStageFlags etapaOrigen,
                             VkPipelineStageFlags etapaDestino,
                             VkImageAspectFlags aspecto =
                                 VK_IMAGE_ASPECT_COLOR_BIT);
    void liberarBuffersRetirados();
    // Aplica la conversion de convencion del motor (OpenGL) a la de Vulkan
    // sobre una matriz de la proyeccion o de la view-projection inversa.
    void convertirMatriz(Contrato::ShaderKind kind, const char* nombre,
                         float* destino);

    // --- Estado de Vulkan --------------------------------------------------
    VkInstance instancia_ = VK_NULL_HANDLE;
    VkPhysicalDevice gpuFisico_ = VK_NULL_HANDLE;
    VkDevice dispositivo_ = VK_NULL_HANDLE;
    VkSurfaceKHR superficie_ = VK_NULL_HANDLE;
    VkQueue colaGrafica_ = VK_NULL_HANDLE;
    uint32_t familiaGrafica_ = 0;
    VkPhysicalDeviceMemoryProperties memoriaGpu_ = {};
    FuncionesDispositivo funciones_;

    VkCommandPool poolComandos_ = VK_NULL_HANDLE;
    VkCommandBuffer comando_ = VK_NULL_HANDLE;
    VkCommandBuffer comandoTransferencia_ = VK_NULL_HANDLE;
    bool comandoAbierto_ = false;

    // Swapchain.
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat formatoSwapchain_ = VK_FORMAT_UNDEFINED;
    VkColorSpaceKHR espacioSwapchain_ = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    VkExtent2D extensionSwapchain_ = {0, 0};
    uint32_t cantidadImagenesSwapchain_ = 0;
    std::vector<VkImage> imagenesSwapchain_;
    std::vector<VkImageView> vistasSwapchain_;
    // Un framebuffer por imagen del swapchain: cada imagen tiene su vista y
    // el render pass se resuelve contra la que se dibujo en este frame.
    std::vector<VkFramebuffer> framebuffersSwapchain_;
    VkSemaphore semaforoAdquirido_ = VK_NULL_HANDLE;
    VkSemaphore semaforoPresentado_ = VK_NULL_HANDLE;
    VkFence fenceFrame_ = VK_NULL_HANDLE;
    uint32_t indiceSwapchain_ = 0;
    bool swapchainValido_ = false;
    bool swapchainAdquirido_ = false;
    // El swapchain hay que rehacerlo (cambio de tamano o de formato): se
    // hace al principio del frame siguiente.
    bool recrearSwapchain_ = false;
    bool hayFramePendiente_ = false;

    // Render passes.
    VkRenderPass renderPassSwapchain_ = VK_NULL_HANDLE;
    VkRenderPass renderPassObjetivo_ = VK_NULL_HANDLE;
    // Render pass de la interfaz: un solo attachment de color (la imagen del
    // swapchain) y sin profundidad.
    VkRenderPass renderPassInterfaz_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffersInterfaz_;
    VkImage imagenMSAA_ = VK_NULL_HANDLE;
    VkDeviceMemory memoriaMSAA_ = VK_NULL_HANDLE;
    VkImageView vistaMSAA_ = VK_NULL_HANDLE;
    VkImage imagenProfundidad_ = VK_NULL_HANDLE;
    VkDeviceMemory memoriaProfundidad_ = VK_NULL_HANDLE;
    VkImageView vistaProfundidad_ = VK_NULL_HANDLE;
    uint32_t muestras_ = 1;
    // Indice del attachment de color y del de profundidad del render pass
    // abierto. Cambia con el destino (y con el multisample) y lo necesitan la
    // limpieza y la resolucion.
    int indiceAttachmentColor_ = 0;
    int indiceAttachmentProfundidad_ = 1;

    // Estado de la pasada.
    DestinoActual destino_ = DestinoActual::Ninguno;
    Handle destinoObjetivo_ = kInvalidHandle;
    // La imagen del swapchain ya quedo en el layout de presentacion (la
    // pasada de la interfaz la deja ahi), asi que el frame no la cambia.
    bool imagenEnPresentacion_ = false;
    float colorLimpieza_[3] = {0.0f, 0.0f, 0.0f};
    bool pendienteLimpiar_ = false;
    int viewport_[4] = {0, 0, 0, 0};
    ClaveEstado estado_;
    ClaveEstado estadoBase_;

    // Programa en uso y valores.
    Contrato::ShaderKind kindEnUso_ = Contrato::ShaderKind::Malla;
    bool hayProgramaEnUso_ = false;
    ProgramaVulkan* programaEnUso_ = nullptr;

    // Pool de descriptors: uno global, con un conjunto por draw.
    VkDescriptorPool poolDescriptores_ = VK_NULL_HANDLE;
    // Alineacion minima del offset dinamico de un uniform buffer (limite del
    // dispositivo): define el tamano de cada slice del anillo de uniforms.
    uint32_t alineacionUniformes_ = 256;
    // Buffers de uniforms retirados: cuando el anillo crece en medio de un
    // frame, el buffer viejo sigue vivo hasta el frame siguiente (los comandos
    // ya grabados lo referencian).
    std::vector<VkBuffer> buffersRetirados_;
    std::vector<VkDeviceMemory> memoriasRetiradas_;

    // Tablas de texturas por unidad.
    std::vector<Handle> textura2DPorUnidad_;
    std::vector<Handle> cubemapPorUnidad_;

    // Recursos.
    std::unordered_map<Handle, MallaVulkan> mallas_;
    std::unordered_map<Handle, BatchLineasVulkan> batches_;
    std::unordered_map<Handle, TexturaVulkan> texturas_;
    std::unordered_map<Handle, RenderTargetVulkan> objetivos_;
    std::unordered_map<Handle, Contrato::ShaderKind> programas_;
    std::unordered_map<Contrato::ShaderKind, std::unique_ptr<ProgramaVulkan>>
        programasPorTipo_;
    // Contador de recursos (el handle es opaco y unico para todos los tipos,
    // para que un handle de un tipo nunca se confunda con el de otro).
    Handle contadorRecursos_ = 0;

    // Textura blanca de 1x1: la que se liga en un sampler sin textura, para
    // que el descriptor sea valido.
    TexturaVulkan texturaBlanca_;

    // Texto del diagnostico de arranque.
    std::string diagnostico_;
};

// ---------------------------------------------------------------------------
// Registro del backend y de la interfaz
// ---------------------------------------------------------------------------

// El backend de Vulkan activo.
//
// La fachada de la interfaz lo usa para engancharse: el backend de ImGui
// necesita la instancia, el dispositivo, el render pass y los descriptores
// del backend grafico, y el motor no habla con el backend de ImGui.
BackendVulkan* backendVulkanActivo();

// Conversion de una textura del motor en el identificador que usa la
// interfaz.
//
// En Vulkan el identificador de una textura es un conjunto de descriptores (lo
// crea el backend de ImGui) y no la vista de la imagen, asi que es la fachada
// de la interfaz la que sabe convertirlo. La fachada registra la conversion
// cuando se inicializa; si no hay ninguna, el backend devuelve la vista de
// la imagen.
using ConversionTextura = void* (*)(Handle textura, void* contexto);
void registrarConversionTextura(ConversionTextura conversion, void* contexto);
ConversionTextura conversionTexturaActiva();
void* contextoConversionTextura();

} // namespace Vulkan
} // namespace Backend
} // namespace Rendering

#endif
