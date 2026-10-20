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

#include "BackendVulkanUtil.h"

namespace Rendering {
namespace Backend {
namespace Vulkan {

// ---------------------------------------------------------------------------
// Pasadas de render
// ---------------------------------------------------------------------------

// Abre la pasada de render del destino indicado.
//
// En Vulkan no se puede dibujar sin una pasada abierta: la pasada es la que
// define los attachments (color, resolucion y profundidad) y en que layout
// quedan. La pasada del swapchain tiene tres attachments cuando hay
// multisample (el color multisampleado, la resolucion y la profundidad) y dos
// cuando no (el color y la profundidad).
void BackendVulkan::abrirRenderPass(DestinoActual destino) {
    if (!comandoAbierto_) return;
    if (destino_ != DestinoActual::Ninguno) return;

    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    std::vector<VkImageView> adjuntos;

    if (destino == DestinoActual::RenderTarget) {
        auto it = objetivos_.find(destinoObjetivo_);
        if (it == objetivos_.end()) {
            destino = DestinoActual::Swapchain;
        } else {
            renderPass = renderPassObjetivo_;
            framebuffer = it->second.framebuffer;
            adjuntos = {it->second.vistaColor, it->second.vistaProfundidad};
            indiceAttachmentColor_ = 0;
            indiceAttachmentProfundidad_ = 1;
        }
    }

    if (destino == DestinoActual::Swapchain) {
        if (!swapchainValido_ || framebuffersSwapchain_.empty()) return;
        renderPass = renderPassSwapchain_;
        framebuffer = framebuffersSwapchain_[indiceSwapchain_];
        if (muestras_ > 1) {
            adjuntos = {vistaMSAA_, vistasSwapchain_[indiceSwapchain_],
                        vistaProfundidad_};
            indiceAttachmentColor_ = 0;
            indiceAttachmentProfundidad_ = 2;
        } else {
            adjuntos = {vistasSwapchain_[indiceSwapchain_], vistaProfundidad_};
            indiceAttachmentColor_ = 0;
            indiceAttachmentProfundidad_ = 1;
        }
    }

    if (!renderPass || !framebuffer) return;

    VkClearValue colores[3]{};
    colores[0].color = {{colorLimpieza_[0], colorLimpieza_[1], colorLimpieza_[2],
                         1.0f}};
    colores[1].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    colores[2].depthStencil = {1.0f, 0};

    // El numero de valores de limpieza es el de attachments de la pasada: con
    // multisample son tres (color, resolucion y profundidad) y sin el son dos
    // (color y profundidad).
    const uint32_t cantidadClear =
        (destino == DestinoActual::Swapchain && muestras_ > 1) ? 3 : 2;

    VkRenderPassBeginInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass = renderPass;
    info.framebuffer = framebuffer;
    VkExtent2D area = extensionDestino(destino);
    info.renderArea = {{0, 0}, area};
    info.clearValueCount = cantidadClear;
    info.pClearValues = colores;

    funciones_.vkCmdBeginRenderPass(comando_, &info,
                                    VK_SUBPASS_CONTENTS_INLINE);
    destino_ = destino;
}

// Cierra la pasada de render abierta.
//
// Al cerrar la pasada de un render target, su imagen de color queda en
// SHADER_READ_ONLY_OPTIMAL: asi la interfaz puede muestrearla como textura
// sin tener que cambiar el layout en cada uso.
void BackendVulkan::cerrarRenderPass() {
    if (destino_ == DestinoActual::Ninguno) return;

    funciones_.vkCmdEndRenderPass(comando_);

    if (destino_ == DestinoActual::RenderTarget) {
        auto it = objetivos_.find(destinoObjetivo_);
        if (it != objetivos_.end() && !it->second.colorEnShaderRead) {
            VkImageMemoryBarrier barrera{};
            barrera.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrera.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrera.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrera.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrera.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrera.image = it->second.color;
            barrera.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrera.subresourceRange.levelCount = 1;
            barrera.subresourceRange.layerCount = 1;
            barrera.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrera.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            funciones_.vkCmdPipelineBarrier(
                comando_, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                nullptr, 1, &barrera);
            it->second.colorEnShaderRead = true;
        }
    }

    destino_ = DestinoActual::Ninguno;
}

void BackendVulkan::bindRenderTarget(Handle target) {
    if (destino_ == DestinoActual::RenderTarget && destinoObjetivo_ == target)
        return;

    cerrarRenderPass();

    auto it = objetivos_.find(target);
    if (it == objetivos_.end()) return;

    // La imagen de color tiene que estar en COLOR_ATTACHMENT_OPTIMAL para
    // poder ser attachment de la pasada.
    if (it->second.colorEnShaderRead) {
        VkImageMemoryBarrier barrera{};
        barrera.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrera.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrera.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrera.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrera.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrera.image = it->second.color;
        barrera.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrera.subresourceRange.levelCount = 1;
        barrera.subresourceRange.layerCount = 1;
        barrera.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrera.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        funciones_.vkCmdPipelineBarrier(
            comando_, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0,
            nullptr, 1, &barrera);
        it->second.colorEnShaderRead = false;
    }

    destinoObjetivo_ = target;
    abrirRenderPass(DestinoActual::RenderTarget);
}

void BackendVulkan::bindDefaultFramebuffer() {
    if (destino_ == DestinoActual::Swapchain) return;
    cerrarRenderPass();
    destinoObjetivo_ = kInvalidHandle;
    abrirRenderPass(DestinoActual::Swapchain);
}

// ---------------------------------------------------------------------------
// Interfaz
// ---------------------------------------------------------------------------

// Abre la pasada de la interfaz.
//
// La interfaz se dibuja al final del frame, sobre la imagen del swapchain ya
// resuelta, en una pasada propia sin profundidad: el pipeline que arma ImGui
// no lleva estado de profundidad y no puede usarse en la pasada de la escena.
//
// La pasada deja la imagen en el layout de presentacion, asi que el frame no
// la vuelve a transicionar.
void BackendVulkan::abrirPasadaInterfaz() {
    if (!comandoAbierto_ || !swapchainAdquirido_) return;
    // Si la pasada de la escena sigue abierta, se cierra: la interfaz se
    // dibuja en una pasada propia, encima.
    cerrarRenderPass();
    if (destino_ != DestinoActual::Ninguno) return;
    if (!renderPassInterfaz_ || framebuffersInterfaz_.empty()) return;
    if (indiceSwapchain_ >= framebuffersInterfaz_.size()) return;

    VkClearValue colores[1]{};
    colores[0].color = {{0.0f, 0.0f, 0.0f, 0.0f}};

    VkRenderPassBeginInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass = renderPassInterfaz_;
    info.framebuffer = framebuffersInterfaz_[indiceSwapchain_];
    info.renderArea = {{0, 0}, extensionSwapchain_};
    info.clearValueCount = 1;
    info.pClearValues = colores;

    funciones_.vkCmdBeginRenderPass(comando_, &info,
                                    VK_SUBPASS_CONTENTS_INLINE);
    destino_ = DestinoActual::Interfaz;
}

void BackendVulkan::cerrarPasadaInterfaz() {
    if (destino_ != DestinoActual::Interfaz) return;

    funciones_.vkCmdEndRenderPass(comando_);
    destino_ = DestinoActual::Ninguno;
    // La pasada deja la imagen en el layout de presentacion.
    imagenEnPresentacion_ = true;
}

// ---------------------------------------------------------------------------
// Estado del pipeline
// ---------------------------------------------------------------------------

// Tamano de lo que se esta dibujando: el del render target si se dibuja en
// uno, y el del swapchain si se dibuja en la pantalla.
//
// El destino se pasa como parametro porque, al abrir una pasada, todavia no es
// el destino actual: el area de render se calcula antes de fijarlo.
VkExtent2D BackendVulkan::extensionDestino(DestinoActual destino) const {
    if (destino == DestinoActual::RenderTarget) {
        auto it = objetivos_.find(destinoObjetivo_);
        if (it != objetivos_.end()) {
            return {static_cast<uint32_t>(it->second.ancho),
                    static_cast<uint32_t>(it->second.alto)};
        }
    }
    return extensionSwapchain_;
}

// Liga el pipeline que corresponde al programa en uso y al estado actual.
//
// En Vulkan el estado de profundidad y de blend es del pipeline, no del draw:
// por eso hay un pipeline por combinacion de estado, y el que se usa depende
// de lo que el motor haya pedido con setDepthTestEnabled, setDepthMask,
// setDepthFunc y setBlendEnabled.
void BackendVulkan::ligarPipeline(Contrato::ShaderKind kind) {
    if (!programaEnUso_ || !comandoAbierto_) return;

    VkPipeline pipeline = pipelinePara(*programaEnUso_, estado_, destino_);
    if (!pipeline) return;

    funciones_.vkCmdBindPipeline(comando_, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 pipeline);

    // Viewport y scissor: son dinamicos (el motor cambia de encuadre entre la
    // pasada principal y las vistas previas).
    VkExtent2D area = extensionDestino(destino_);
    float ancho = viewport_[2] > 0 ? static_cast<float>(viewport_[2])
                                   : static_cast<float>(area.width);
    float alto = viewport_[3] > 0 ? static_cast<float>(viewport_[3])
                                  : static_cast<float>(area.height);
    VkViewport viewport{};
    viewport.x = static_cast<float>(viewport_[0]);
    viewport.y = static_cast<float>(viewport_[1]);
    viewport.width = ancho;
    viewport.height = alto;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    funciones_.vkCmdSetViewport(comando_, 0, 1, &viewport);

    VkRect2D scissors{};
    scissors.offset = {viewport_[0], viewport_[1]};
    scissors.extent = {static_cast<uint32_t>(ancho), static_cast<uint32_t>(alto)};
    funciones_.vkCmdSetScissor(comando_, 0, 1, &scissors);
}

// ---------------------------------------------------------------------------
// Dibujado
// ---------------------------------------------------------------------------

void BackendVulkan::drawMesh(Handle mesh, unsigned int indexCount) {
    if (!comandoAbierto_ || !hayProgramaEnUso_ || indexCount == 0) return;
    auto encontrado = mallas_.find(mesh);
    if (encontrado == mallas_.end()) return;

    if (destino_ == DestinoActual::Ninguno)
        abrirRenderPass(DestinoActual::Swapchain);
    if (destino_ == DestinoActual::Ninguno) return;

    ligarPipeline(kindEnUso_);

    // Los valores de los uniforms van a su propia slice del anillo: el motor
    // escribe, dibuja, reescribe y vuelve a dibujar, y cada draw necesita su
    // copia de los valores.
    escribirSlice(*programaEnUso_);
    VkDescriptorSet conjunto = prepararDescriptorSet(*programaEnUso_);
    if (!conjunto) return;
    const uint32_t offsetDinamico = static_cast<uint32_t>(
        (programaEnUso_->siguienteSlice - 1) * programaEnUso_->tamanoSlice);

    funciones_.vkCmdBindDescriptorSets(
        comando_, VK_PIPELINE_BIND_POINT_GRAPHICS,
        programaEnUso_->layoutPipeline, 0, 1, &conjunto, 1, &offsetDinamico);

    const VkBuffer bufferVertices = encontrado->second.vertices;
    const VkDeviceSize offset = 0;
    funciones_.vkCmdBindVertexBuffers(comando_, 0, 1, &bufferVertices, &offset);
    funciones_.vkCmdBindIndexBuffer(comando_, encontrado->second.indices, 0,
                                    VK_INDEX_TYPE_UINT32);
    funciones_.vkCmdDrawIndexed(comando_, indexCount, 1, 0, 0, 0);
}

void BackendVulkan::drawLineBatch(Handle batch, unsigned int vertexCount) {
    if (!comandoAbierto_ || !hayProgramaEnUso_ || vertexCount == 0) return;
    auto encontrado = batches_.find(batch);
    if (encontrado == batches_.end()) return;

    if (destino_ == DestinoActual::Ninguno)
        abrirRenderPass(DestinoActual::Swapchain);
    if (destino_ == DestinoActual::Ninguno) return;

    ligarPipeline(kindEnUso_);

    escribirSlice(*programaEnUso_);
    VkDescriptorSet conjunto = prepararDescriptorSet(*programaEnUso_);
    if (!conjunto) return;
    const uint32_t offsetDinamico = static_cast<uint32_t>(
        (programaEnUso_->siguienteSlice - 1) * programaEnUso_->tamanoSlice);

    funciones_.vkCmdBindDescriptorSets(
        comando_, VK_PIPELINE_BIND_POINT_GRAPHICS,
        programaEnUso_->layoutPipeline, 0, 1, &conjunto, 1, &offsetDinamico);

    const VkBuffer bufferVertices = encontrado->second.vertices;
    // El batch puede quedarse sin buffer si su recreacion fallo: ligar un
    // buffer nulo no es una llamada valida.
    if (bufferVertices == VK_NULL_HANDLE) return;
    const VkDeviceSize offset = 0;
    funciones_.vkCmdBindVertexBuffers(comando_, 0, 1, &bufferVertices, &offset);
    funciones_.vkCmdDraw(comando_, vertexCount, 1, 0, 0);
}

void BackendVulkan::drawFullscreenTriangle() {
    if (!comandoAbierto_ || !hayProgramaEnUso_) return;

    if (destino_ == DestinoActual::Ninguno)
        abrirRenderPass(DestinoActual::Swapchain);
    if (destino_ == DestinoActual::Ninguno) return;

    ligarPipeline(kindEnUso_);

    escribirSlice(*programaEnUso_);
    VkDescriptorSet conjunto = prepararDescriptorSet(*programaEnUso_);
    if (!conjunto) return;
    const uint32_t offsetDinamico = static_cast<uint32_t>(
        (programaEnUso_->siguienteSlice - 1) * programaEnUso_->tamanoSlice);

    funciones_.vkCmdBindDescriptorSets(
        comando_, VK_PIPELINE_BIND_POINT_GRAPHICS,
        programaEnUso_->layoutPipeline, 0, 1, &conjunto, 1, &offsetDinamico);

    // El triangulo a pantalla completa no lee atributos: la geometria la arma
    // el vertex shader con el indice del vertice. Se desata el buffer de
    // vertices anterior, porque un pipeline sin bindings de vertice no puede
    // tener ninguno ligado.
    funciones_.vkCmdBindVertexBuffers(comando_, 0, 0, nullptr, nullptr);
    funciones_.vkCmdDraw(comando_, 3, 1, 0, 0);
}

} // namespace Vulkan
} // namespace Backend
} // namespace Rendering
