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

#include <algorithm>
#include <cstring>
#include <vector>

namespace Rendering {
namespace Backend {
namespace Vulkan {

namespace {

// Floats por vertice de malla en el buffer del backend.
//
// El motor entrega posicion, normal, uv, tangente y bitangente en arrays
// separados; el backend decide como los empaqueta. Se guardan juntos,
// siempre, en el orden del contrato (VerticeMalla): asi el pipeline tiene un
// solo tipo de vertice y el paso (stride) es el mismo para todas las mallas.
// Los campos que la malla no trae se dejan en cero.
constexpr int kFloatsVerticeMalla = 14;

// Floats por vertice del batch de lineas (el formato de LineBuilder):
// inicio, fin, lado, avance y color.
constexpr int kFloatsVerticeLinea = 12;

// Numero de niveles de mipmap de una imagen de ancho x alto.
int nivelesDeMipmap(int ancho, int alto) {
    int mayor = std::max(ancho, alto);
    int niveles = 1;
    while (mayor > 1) {
        mayor /= 2;
        ++niveles;
    }
    return niveles;
}

// Mipmap siguiente por promediado de 2x2. Se genera en la CPU: es
// determinista, no necesita un pipeline extra de Vulkan y el costo es de un
// frame, una vez por textura.
void generarMipmap(const unsigned char* origen, int ancho, int alto,
                   unsigned char* destino) {
    const int anchoDestino = std::max(1, ancho / 2);
    const int altoDestino = std::max(1, alto / 2);
    for (int y = 0; y < altoDestino; ++y) {
        for (int x = 0; x < anchoDestino; ++x) {
            const int x0 = std::min(x * 2, ancho - 1);
            const int y0 = std::min(y * 2, alto - 1);
            const int x1 = std::min(x * 2 + 1, ancho - 1);
            const int y1 = std::min(y * 2 + 1, alto - 1);
            for (int c = 0; c < 4; ++c) {
                const int suma = origen[(y0 * ancho + x0) * 4 + c] +
                                 origen[(y0 * ancho + x1) * 4 + c] +
                                 origen[(y1 * ancho + x0) * 4 + c] +
                                 origen[(y1 * ancho + x1) * 4 + c];
                destino[(y * anchoDestino + x) * 4 + c] =
                    static_cast<unsigned char>(suma / 4);
            }
        }
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Buffers
// ---------------------------------------------------------------------------

bool BackendVulkan::crearBufferVacio(std::size_t bytes, VkBufferUsageFlags uso,
                                      VkBuffer& buffer,
                                      VkDeviceMemory& memoria,
                                      VkMemoryPropertyFlags propiedades) {
    if (bytes == 0) return false;

    VkBufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = bytes;
    info.usage = uso;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (!okVulkan(funciones_.vkCreateBuffer(dispositivo_, &info, nullptr, &buffer),
                  "vkCreateBuffer"))
        return false;

    VkMemoryRequirements req{};
    funciones_.vkGetBufferMemoryRequirements(dispositivo_, buffer, &req);
    if (!asignarMemoria(req, propiedades, memoria)) {
        funciones_.vkDestroyBuffer(dispositivo_, buffer, nullptr);
        return false;
    }
    funciones_.vkBindBufferMemory(dispositivo_, buffer, memoria, 0);
    return true;
}

bool BackendVulkan::crearBufferSubiendo(const void* datos, std::size_t bytes,
                                         VkBufferUsageFlags uso,
                                         VkBuffer& buffer,
                                         VkDeviceMemory& memoria) {
    // El buffer recibe la copia del buffer de paso, asi que necesita el uso de
    // transferencia de destino: sin el, la copia es invalida y el buffer
    // queda sin contenido.
    if (!crearBufferVacio(bytes, uso | VK_BUFFER_USAGE_TRANSFER_DST_BIT, buffer,
                          memoria))
        return false;

    // Buffer de paso: visible para la CPU, se escribe y se copia al buffer
    // final (que vive en memoria de la GPU).
    VkBufferCreateInfo infoStaging{};
    infoStaging.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    infoStaging.size = bytes;
    infoStaging.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    infoStaging.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkBuffer staging = VK_NULL_HANDLE;
    if (!okVulkan(funciones_.vkCreateBuffer(dispositivo_, &infoStaging, nullptr,
                                            &staging),
                  "vkCreateBuffer (paso)"))
        return false;

    VkMemoryRequirements req{};
    funciones_.vkGetBufferMemoryRequirements(dispositivo_, staging, &req);
    VkDeviceMemory memoriaStaging = VK_NULL_HANDLE;
    if (!asignarMemoria(req,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        memoriaStaging)) {
        funciones_.vkDestroyBuffer(dispositivo_, staging, nullptr);
        return false;
    }
    funciones_.vkBindBufferMemory(dispositivo_, staging, memoriaStaging, 0);

    void* mapa = nullptr;
    if (!okVulkan(funciones_.vkMapMemory(dispositivo_, memoriaStaging, 0, bytes,
                                         0, &mapa),
                  "vkMapMemory (paso)")) {
        funciones_.vkDestroyBuffer(dispositivo_, staging, nullptr);
        funciones_.vkFreeMemory(dispositivo_, memoriaStaging, nullptr);
        return false;
    }
    std::memcpy(mapa, datos, bytes);
    funciones_.vkUnmapMemory(dispositivo_, memoriaStaging);

    // La copia se graba en el command buffer de transferencias y se espera a
    // que termine: el recurso tiene que estar listo antes de que se use.
    funciones_.vkResetCommandBuffer(comandoTransferencia_, 0);
    VkCommandBufferBeginInfo infoBegin{};
    infoBegin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    infoBegin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    funciones_.vkBeginCommandBuffer(comandoTransferencia_, &infoBegin);

    VkBufferCopy region{};
    region.size = bytes;
    funciones_.vkCmdCopyBuffer(comandoTransferencia_, staging, buffer, 1,
                               &region);
    VkResult resEnd = funciones_.vkEndCommandBuffer(comandoTransferencia_);
    if (resEnd != VK_SUCCESS)
        reportarErrorVulkan("vkEndCommandBuffer (subida)", resEnd);

    VkSubmitInfo infoSubmit{};
    infoSubmit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    infoSubmit.commandBufferCount = 1;
    infoSubmit.pCommandBuffers = &comandoTransferencia_;
    VkResult resSubmit = funciones_.vkQueueSubmit(colaGrafica_, 1, &infoSubmit,
                                                   VK_NULL_HANDLE);
    if (resSubmit != VK_SUCCESS)
        reportarErrorVulkan("vkQueueSubmit (subida)", resSubmit);
    VkResult resEspera = funciones_.vkQueueWaitIdle(colaGrafica_);
    if (resEspera != VK_SUCCESS)
        reportarErrorVulkan("vkQueueWaitIdle (subida)", resEspera);


    funciones_.vkDestroyBuffer(dispositivo_, staging, nullptr);
    funciones_.vkFreeMemory(dispositivo_, memoriaStaging, nullptr);
    return true;
}

// ---------------------------------------------------------------------------
// Mallas
// ---------------------------------------------------------------------------

Handle BackendVulkan::createMesh(const MeshData& data) {
    if (!data.vertices || data.vertexCount == 0 || !data.indices ||
        data.indexCount == 0) {
        return kInvalidHandle;
    }

    const std::size_t count = data.vertexCount;
    const bool hayNormal = data.normals && data.normalCount == count;
    const bool hayUv = data.uvs && data.uvCount == count;
    const bool hayTangent = data.tangents && data.tangentCount == count &&
                            data.bitangents && data.bitangentCount == count;

    std::vector<float> vertices(count * kFloatsVerticeMalla, 0.0f);
    for (std::size_t i = 0; i < count; ++i) {
        float* v = &vertices[i * kFloatsVerticeMalla];
        std::memcpy(v, data.vertices + i * 3, 3 * sizeof(float));
        if (hayNormal) std::memcpy(v + 3, data.normals + i * 3, 3 * sizeof(float));
        if (hayUv) std::memcpy(v + 6, data.uvs + i * 2, 2 * sizeof(float));
        if (hayTangent) {
            std::memcpy(v + 8, data.tangents + i * 3, 3 * sizeof(float));
            std::memcpy(v + 11, data.bitangents + i * 3, 3 * sizeof(float));
        }
    }

    MallaVulkan malla;
    if (!crearBufferSubiendo(vertices.data(),
                              vertices.size() * sizeof(float),
                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                              malla.vertices, malla.memoriaVertices)) {
        return kInvalidHandle;
    }
    if (!crearBufferSubiendo(data.indices, data.indexCount * sizeof(unsigned int),
                              VK_BUFFER_USAGE_INDEX_BUFFER_BIT, malla.indices,
                              malla.memoriaIndices)) {
        funciones_.vkDestroyBuffer(dispositivo_, malla.vertices, nullptr);
        funciones_.vkFreeMemory(dispositivo_, malla.memoriaVertices, nullptr);
        return kInvalidHandle;
    }
    malla.indicesCount = static_cast<std::uint32_t>(data.indexCount);
    return registrarMalla(malla);
}

void BackendVulkan::destroyMesh(Handle mesh) {
    auto it = mallas_.find(mesh);
    if (it == mallas_.end()) return;
    MallaVulkan& malla = it->second;
    if (malla.vertices)
        funciones_.vkDestroyBuffer(dispositivo_, malla.vertices, nullptr);
    if (malla.memoriaVertices)
        funciones_.vkFreeMemory(dispositivo_, malla.memoriaVertices, nullptr);
    if (malla.indices)
        funciones_.vkDestroyBuffer(dispositivo_, malla.indices, nullptr);
    if (malla.memoriaIndices)
        funciones_.vkFreeMemory(dispositivo_, malla.memoriaIndices, nullptr);
    mallas_.erase(it);
}

// ---------------------------------------------------------------------------
// Batch de lineas
// ---------------------------------------------------------------------------

Handle BackendVulkan::createLineBatch(const float* vertices,
                                       std::size_t vertexCount) {
    if (!vertices || vertexCount == 0) return kInvalidHandle;

    BatchLineasVulkan batch;
    const std::size_t bytes = vertexCount * kFloatsVerticeLinea * sizeof(float);
    if (!crearBufferSubiendo(vertices, bytes,
                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                              batch.vertices, batch.memoriaVertices)) {
        return kInvalidHandle;
    }
    batch.verticesCount = static_cast<std::uint32_t>(vertexCount);
    batch.capacidad = bytes;
    return registrarBatch(batch);
}

void BackendVulkan::destroyLineBatch(Handle batch) {
    auto it = batches_.find(batch);
    if (it == batches_.end()) return;
    if (it->second.vertices)
        funciones_.vkDestroyBuffer(dispositivo_, it->second.vertices, nullptr);
    if (it->second.memoriaVertices)
        funciones_.vkFreeMemory(dispositivo_, it->second.memoriaVertices, nullptr);
    batches_.erase(it);
}

void BackendVulkan::updateLineBatch(Handle batch, const float* vertices,
                                    std::size_t vertexCount) {
    auto it = batches_.find(batch);
    if (it == batches_.end() || !vertices || vertexCount == 0) return;

    BatchLineasVulkan& b = it->second;
    const std::size_t bytes = vertexCount * kFloatsVerticeLinea * sizeof(float);
    if (bytes > b.capacidad) {
        // El batch crecio: se reemplaza el buffer por uno mayor.
        if (b.vertices)
            funciones_.vkDestroyBuffer(dispositivo_, b.vertices, nullptr);
        if (b.memoriaVertices)
            funciones_.vkFreeMemory(dispositivo_, b.memoriaVertices, nullptr);
        b.vertices = VK_NULL_HANDLE;
        b.memoriaVertices = VK_NULL_HANDLE;
        b.capacidad = 0;
        if (!crearBufferSubiendo(vertices, bytes,
                                  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, b.vertices,
                                  b.memoriaVertices)) {
            return;
        }
        b.capacidad = bytes;
    } else {
        // Mismo tamano: solo se reescribe el contenido del buffer.
        if (!funciones_.vkMapMemory || !b.memoriaVertices) return;
        void* mapa = nullptr;
        if (!okVulkan(funciones_.vkMapMemory(dispositivo_, b.memoriaVertices, 0,
                                             bytes, 0, &mapa),
                      "vkMapMemory (batch)"))
            return;
        std::memcpy(mapa, vertices, bytes);
        funciones_.vkUnmapMemory(dispositivo_, b.memoriaVertices);
    }
    b.verticesCount = static_cast<std::uint32_t>(vertexCount);
}

// ---------------------------------------------------------------------------
// Texturas
// ---------------------------------------------------------------------------

bool BackendVulkan::crearTextura(const unsigned char* pixeles, int ancho,
                                  int alto, bool cubemap, bool mipmaps,
                                  TexturaVulkan& salida) {
    if (!pixeles || ancho <= 0 || alto <= 0) return false;

    const int niveles = mipmaps ? nivelesDeMipmap(ancho, alto) : 1;
    const uint32_t capas = cubemap ? 6 : 1;

    VkImageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = VK_FORMAT_R8G8B8A8_UNORM;
    info.extent = {static_cast<uint32_t>(ancho), static_cast<uint32_t>(alto), 1};
    info.mipLevels = static_cast<uint32_t>(niveles);
    info.arrayLayers = capas;
    if (cubemap) info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (!okVulkan(funciones_.vkCreateImage(dispositivo_, &info, nullptr,
                                            &salida.imagen),
                  "vkCreateImage (textura)"))
        return false;

    VkMemoryRequirements req{};
    funciones_.vkGetImageMemoryRequirements(dispositivo_, salida.imagen, &req);
    if (!asignarMemoria(req, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, salida.memoria))
        return false;
    funciones_.vkBindImageMemory(dispositivo_, salida.imagen, salida.memoria, 0);

    VkImageViewCreateInfo infoVista{};
    infoVista.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    infoVista.image = salida.imagen;
    infoVista.viewType = cubemap ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
    infoVista.format = VK_FORMAT_R8G8B8A8_UNORM;
    infoVista.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    infoVista.subresourceRange.baseMipLevel = 0;
    infoVista.subresourceRange.levelCount = static_cast<uint32_t>(niveles);
    infoVista.subresourceRange.baseArrayLayer = 0;
    infoVista.subresourceRange.layerCount = capas;
    if (!okVulkan(funciones_.vkCreateImageView(dispositivo_, &infoVista, nullptr,
                                               &salida.vista),
                  "vkCreateImageView (textura)"))
        return false;

    VkSamplerCreateInfo infoSampler{};
    infoSampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    infoSampler.magFilter = VK_FILTER_LINEAR;
    infoSampler.minFilter = VK_FILTER_LINEAR;
    infoSampler.mipmapMode = mipmaps ? VK_SAMPLER_MIPMAP_MODE_LINEAR
                                     : VK_SAMPLER_MIPMAP_MODE_NEAREST;
    infoSampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    infoSampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    infoSampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    infoSampler.mipLodBias = 0.0f;
    infoSampler.anisotropyEnable = VK_FALSE;
    infoSampler.maxLod = static_cast<float>(niveles);
    infoSampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    if (!okVulkan(funciones_.vkCreateSampler(dispositivo_, &infoSampler, nullptr,
                                             &salida.sampler),
                  "vkCreateSampler"))
        return false;

    salida.ancho = ancho;
    salida.alto = alto;
    salida.niveles = niveles;
    salida.esCubemap = cubemap;

    // --- Subida de los pixeles ---------------------------------------------
    if (!subirPixeles(salida.imagen, pixeles, ancho, alto, cubemap, niveles,
                      VK_IMAGE_LAYOUT_UNDEFINED)) {
        return false;
    }
    return true;
}

bool BackendVulkan::subirPixeles(VkImage imagen, const unsigned char* pixeles,
                                  int ancho, int alto, bool cubemap, int niveles,
                                  VkImageLayout origen) {
    if (!imagen || !pixeles || ancho <= 0 || alto <= 0) return false;

    const uint32_t capas = cubemap ? 6 : 1;

    // El buffer de paso tiene que contener todos los niveles de todas las
    // caras: las copias se graban todas y se ejecutan despues, asi que cada
    // una tiene que leer su propia region del buffer (si el buffer se
    // sobreescribiera con cada nivel, todas las copias leerian el ultimo).
    std::vector<std::size_t> offsetsNivel;
    std::size_t bytesPorCara = 0;
    {
        int anchoNivel = ancho;
        int altoNivel = alto;
        for (int n = 0; n < niveles; ++n) {
            offsetsNivel.push_back(bytesPorCara);
            bytesPorCara += static_cast<std::size_t>(anchoNivel) * altoNivel * 4;
            anchoNivel = std::max(1, anchoNivel / 2);
            altoNivel = std::max(1, altoNivel / 2);
        }
    }
    const std::size_t bytesStaging = bytesPorCara * capas;

    VkBufferCreateInfo infoStaging{};
    infoStaging.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    infoStaging.size = bytesStaging;
    infoStaging.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    infoStaging.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkBuffer staging = VK_NULL_HANDLE;
    if (!okVulkan(funciones_.vkCreateBuffer(dispositivo_, &infoStaging, nullptr,
                                            &staging),
                  "vkCreateBuffer (paso de textura)"))
        return false;
    VkMemoryRequirements reqStaging{};
    funciones_.vkGetBufferMemoryRequirements(dispositivo_, staging, &reqStaging);
    VkDeviceMemory memoriaStaging = VK_NULL_HANDLE;
    if (!asignarMemoria(reqStaging,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        memoriaStaging)) {
        funciones_.vkDestroyBuffer(dispositivo_, staging, nullptr);
        return false;
    }
    funciones_.vkBindBufferMemory(dispositivo_, staging, memoriaStaging, 0);
    void* mapa = nullptr;
    if (!okVulkan(funciones_.vkMapMemory(dispositivo_, memoriaStaging, 0,
                                         bytesStaging, 0, &mapa),
                  "vkMapMemory (textura)")) {
        funciones_.vkDestroyBuffer(dispositivo_, staging, nullptr);
        funciones_.vkFreeMemory(dispositivo_, memoriaStaging, nullptr);
        return false;
    }

    funciones_.vkResetCommandBuffer(comandoTransferencia_, 0);
    VkCommandBufferBeginInfo infoBegin{};
    infoBegin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    infoBegin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    funciones_.vkBeginCommandBuffer(comandoTransferencia_, &infoBegin);

    // La imagen pasa al layout de destino de las copias.
    VkImageMemoryBarrier barrera{};
    barrera.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrera.oldLayout = origen;
    barrera.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrera.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrera.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrera.image = imagen;
    barrera.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrera.subresourceRange.baseMipLevel = 0;
    barrera.subresourceRange.levelCount = static_cast<uint32_t>(niveles);
    barrera.subresourceRange.layerCount = capas;
    barrera.srcAccessMask =
        origen == VK_IMAGE_LAYOUT_UNDEFINED ? 0 : VK_ACCESS_SHADER_READ_BIT;
    barrera.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    funciones_.vkCmdPipelineBarrier(comandoTransferencia_,
                                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                                    VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                                    nullptr, 0, nullptr, 1, &barrera);

    std::vector<unsigned char> nivel;
    std::vector<unsigned char> siguiente;

    // Copia un nivel (una cara y un nivel de mipmap) al buffer de paso y
    // graba la copia. En un cubemap las filas se invierten: es la diferencia
    // de convenio entre OpenGL y Vulkan para las caras de un cubemap.
    auto copiarNivel = [&](const unsigned char* origen, int anchoNivel,
                           int altoNivel, int nivelMip, uint32_t capa) {
        const std::size_t offset =
            offsetsNivel[static_cast<std::size_t>(nivelMip)] +
            static_cast<std::size_t>(capa) * bytesPorCara;
        unsigned char* destino = static_cast<unsigned char*>(mapa) + offset;

        if (cubemap) {
            for (int y = 0; y < altoNivel; ++y) {
                const unsigned char* fila =
                    origen + static_cast<std::size_t>(altoNivel - 1 - y) * anchoNivel * 4;
                std::memcpy(destino + y * anchoNivel * 4, fila, anchoNivel * 4);
            }
        } else {
            std::memcpy(destino, origen,
                        static_cast<std::size_t>(anchoNivel) * altoNivel * 4);
        }

        VkBufferImageCopy region{};
        region.bufferOffset = offset;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = static_cast<uint32_t>(nivelMip);
        region.imageSubresource.baseArrayLayer = capa;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {static_cast<uint32_t>(anchoNivel),
                              static_cast<uint32_t>(altoNivel), 1};
        funciones_.vkCmdCopyBufferToImage(comandoTransferencia_, staging,
                                          imagen,
                                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                          1, &region);
    };

    const std::size_t bytesNivel =
        static_cast<std::size_t>(ancho) * static_cast<std::size_t>(alto) * 4;

    for (uint32_t capa = 0; capa < capas; ++capa) {
        const unsigned char* base =
            cubemap ? pixeles + capa * bytesNivel : pixeles;

        int anchoNivel = ancho;
        int altoNivel = alto;
        copiarNivel(base, anchoNivel, altoNivel, 0, capa);

        // Mipmaps siguientes: promediado de 2x2 en la CPU, una cadena por cara.
        nivel.assign(base, base + bytesNivel);
        for (int n = 1; n < niveles; ++n) {
            const int anchoSiguiente = std::max(1, anchoNivel / 2);
            const int altoSiguiente = std::max(1, altoNivel / 2);
            siguiente.assign(static_cast<std::size_t>(anchoSiguiente) *
                                 altoSiguiente * 4,
                             0);
            generarMipmap(nivel.data(), anchoNivel, altoNivel, siguiente.data());
            copiarNivel(siguiente.data(), anchoSiguiente, altoSiguiente, n, capa);
            nivel.swap(siguiente);
            anchoNivel = anchoSiguiente;
            altoNivel = altoSiguiente;
        }
    }

    // La imagen pasa de estar en TRANSFER_DST a poder muestrearse.
    VkImageMemoryBarrier barreraFinal{};
    barreraFinal.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barreraFinal.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barreraFinal.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barreraFinal.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barreraFinal.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barreraFinal.image = imagen;
    barreraFinal.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barreraFinal.subresourceRange.baseMipLevel = 0;
    barreraFinal.subresourceRange.levelCount = static_cast<uint32_t>(niveles);
    barreraFinal.subresourceRange.layerCount = capas;
    barreraFinal.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barreraFinal.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    funciones_.vkCmdPipelineBarrier(
        comandoTransferencia_, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barreraFinal);

    funciones_.vkEndCommandBuffer(comandoTransferencia_);
    VkSubmitInfo infoSubmit{};
    infoSubmit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    infoSubmit.commandBufferCount = 1;
    infoSubmit.pCommandBuffers = &comandoTransferencia_;
    funciones_.vkQueueSubmit(colaGrafica_, 1, &infoSubmit, VK_NULL_HANDLE);
    funciones_.vkQueueWaitIdle(colaGrafica_);

    funciones_.vkUnmapMemory(dispositivo_, memoriaStaging);
    funciones_.vkDestroyBuffer(dispositivo_, staging, nullptr);
    funciones_.vkFreeMemory(dispositivo_, memoriaStaging, nullptr);
    return true;
}

Handle BackendVulkan::createTexture2D(const Image2D& image) {
    if (!image.pixels || image.width <= 0 || image.height <= 0)
        return kInvalidHandle;
    TexturaVulkan textura;
    if (!crearTextura(image.pixels, image.width, image.height, false,
                      image.generateMipmaps, textura)) {
        return kInvalidHandle;
    }
    return registrarTextura(textura);
}

void BackendVulkan::destroyTexture2D(Handle texture) {
    auto it = texturas_.find(texture);
    if (it == texturas_.end()) return;
    TexturaVulkan& t = it->second;
    if (t.vista) funciones_.vkDestroyImageView(dispositivo_, t.vista, nullptr);
    if (t.imagen) funciones_.vkDestroyImage(dispositivo_, t.imagen, nullptr);
    if (t.memoria) funciones_.vkFreeMemory(dispositivo_, t.memoria, nullptr);
    if (t.sampler) funciones_.vkDestroySampler(dispositivo_, t.sampler, nullptr);
    texturas_.erase(it);
}

void BackendVulkan::bindTexture2D(Handle texture, int unit) {
    if (unit < 0) return;
    if (static_cast<std::size_t>(unit) >= textura2DPorUnidad_.size())
        textura2DPorUnidad_.resize(static_cast<std::size_t>(unit) + 1,
                                    kInvalidHandle);
    textura2DPorUnidad_[static_cast<std::size_t>(unit)] = texture;
}

void BackendVulkan::updateTexture2D(Handle texture, const Image2D& image) {
    auto it = texturas_.find(texture);
    if (it == texturas_.end() || !image.pixels) return;
    TexturaVulkan& t = it->second;

    if (image.width != t.ancho || image.height != t.alto) {
        // Cambiaron las dimensiones: se reasigna el almacenamiento.
        TexturaVulkan nueva;
        if (!crearTextura(image.pixels, image.width, image.height, false,
                          image.generateMipmaps, nueva)) {
            return;
        }
        if (t.vista) funciones_.vkDestroyImageView(dispositivo_, t.vista, nullptr);
        if (t.imagen) funciones_.vkDestroyImage(dispositivo_, t.imagen, nullptr);
        if (t.memoria) funciones_.vkFreeMemory(dispositivo_, t.memoria, nullptr);
        if (t.sampler) funciones_.vkDestroySampler(dispositivo_, t.sampler, nullptr);
        t = nueva;
        return;
    }

    // Mismas dimensiones: se sube solo el contenido. La imagen ya existe y
    // tiene su numero de niveles: si no tenia mipmaps, solo se reescribe el
    // nivel base (crecer la cadena exigiria recrear la imagen).
    const int niveles = t.niveles;
    const bool mipmaps = niveles > 1;
    if (!subirPixeles(t.imagen, image.pixels, image.width, image.height, false,
                      niveles, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)) {
        return;
    }
    (void)mipmaps;
}

Handle BackendVulkan::createTextureCube(const ImageCube& image) {
    if (image.width <= 0 || image.height <= 0) return kInvalidHandle;
    for (int i = 0; i < 6; ++i)
        if (!image.faces[i]) return kInvalidHandle;

    // Las seis caras van contiguas en un solo bloque: el backend las sube como
    // las capas de una imagen cubemap.
    const std::size_t bytesCara =
        static_cast<std::size_t>(image.width) * image.height * 4;
    std::vector<unsigned char> pixels(bytesCara * 6);
    for (int i = 0; i < 6; ++i)
        std::memcpy(pixels.data() + i * bytesCara, image.faces[i], bytesCara);

    TexturaVulkan textura;
    if (!crearTextura(pixels.data(), image.width, image.height, true,
                      image.generateMipmaps, textura)) {
        return kInvalidHandle;
    }
    return registrarTextura(textura);
}

void BackendVulkan::destroyTextureCube(Handle texture) {
    destroyTexture2D(texture);
}

void BackendVulkan::bindTextureCube(Handle texture, int unit) {
    if (unit < 0) return;
    if (static_cast<std::size_t>(unit) >= cubemapPorUnidad_.size())
        cubemapPorUnidad_.resize(static_cast<std::size_t>(unit) + 1,
                                  kInvalidHandle);
    cubemapPorUnidad_[static_cast<std::size_t>(unit)] = texture;
}

// ---------------------------------------------------------------------------
// Registro de recursos
// ---------------------------------------------------------------------------

// Cada recurso guardado en el mapa recibe un handle opaco. El contador es
// comun a todos los tipos: asi un handle de una malla nunca se confunde con
// el de una textura.
Handle BackendVulkan::registrarMalla(MallaVulkan& malla) {
    const Handle handle = ++contadorRecursos_;
    mallas_[handle] = malla;
    return handle;
}

Handle BackendVulkan::registrarBatch(BatchLineasVulkan& batch) {
    const Handle handle = ++contadorRecursos_;
    batches_[handle] = batch;
    return handle;
}

Handle BackendVulkan::registrarTextura(TexturaVulkan& textura) {
    const Handle handle = ++contadorRecursos_;
    texturas_[handle] = textura;
    return handle;
}

Handle BackendVulkan::registrarRenderTarget(RenderTargetVulkan& target) {
    const Handle handle = ++contadorRecursos_;
    objetivos_[handle] = target;
    return handle;
}

// ---------------------------------------------------------------------------
// Render targets
// ---------------------------------------------------------------------------

bool BackendVulkan::crearRenderTarget(int ancho, int alto,
                                      RenderTargetVulkan& salida) {
    if (ancho <= 0 || alto <= 0) return false;

    // Color: se usa como attachment y ademas se muestrea (la vista previa de
    // la GUI la dibuja como textura).
    VkImageCreateInfo infoColor{};
    infoColor.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    infoColor.imageType = VK_IMAGE_TYPE_2D;
    infoColor.format = VK_FORMAT_R8G8B8A8_UNORM;
    infoColor.extent = {static_cast<uint32_t>(ancho), static_cast<uint32_t>(alto), 1};
    infoColor.mipLevels = 1;
    infoColor.arrayLayers = 1;
    infoColor.samples = VK_SAMPLE_COUNT_1_BIT;
    infoColor.tiling = VK_IMAGE_TILING_OPTIMAL;
    infoColor.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                      VK_IMAGE_USAGE_SAMPLED_BIT |
                      VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    infoColor.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    infoColor.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (!okVulkan(funciones_.vkCreateImage(dispositivo_, &infoColor, nullptr,
                                            &salida.color),
                  "vkCreateImage (render target)"))
        return false;

    VkMemoryRequirements req{};
    funciones_.vkGetImageMemoryRequirements(dispositivo_, salida.color, &req);
    if (!asignarMemoria(req, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                        salida.memoriaColor))
        return false;
    funciones_.vkBindImageMemory(dispositivo_, salida.color, salida.memoriaColor, 0);

    VkImageViewCreateInfo infoVistaColor{};
    infoVistaColor.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    infoVistaColor.image = salida.color;
    infoVistaColor.viewType = VK_IMAGE_VIEW_TYPE_2D;
    infoVistaColor.format = VK_FORMAT_R8G8B8A8_UNORM;
    infoVistaColor.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    infoVistaColor.subresourceRange.levelCount = 1;
    infoVistaColor.subresourceRange.layerCount = 1;
    if (!okVulkan(funciones_.vkCreateImageView(dispositivo_, &infoVistaColor,
                                               nullptr, &salida.vistaColor),
                  "vkCreateImageView (render target)"))
        return false;

    // Profundidad.
    VkImageCreateInfo infoProf{};
    infoProf.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    infoProf.imageType = VK_IMAGE_TYPE_2D;
    infoProf.format = VK_FORMAT_D32_SFLOAT;
    infoProf.extent = {static_cast<uint32_t>(ancho), static_cast<uint32_t>(alto), 1};
    infoProf.mipLevels = 1;
    infoProf.arrayLayers = 1;
    infoProf.samples = VK_SAMPLE_COUNT_1_BIT;
    infoProf.tiling = VK_IMAGE_TILING_OPTIMAL;
    infoProf.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    infoProf.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    infoProf.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (!okVulkan(funciones_.vkCreateImage(dispositivo_, &infoProf, nullptr,
                                            &salida.profundidad),
                  "vkCreateImage (profundidad del render target)"))
        return false;

    VkMemoryRequirements reqProf{};
    funciones_.vkGetImageMemoryRequirements(dispositivo_, salida.profundidad,
                                             &reqProf);
    if (!asignarMemoria(reqProf, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                        salida.memoriaProfundidad))
        return false;
    funciones_.vkBindImageMemory(dispositivo_, salida.profundidad,
                                 salida.memoriaProfundidad, 0);

    VkImageViewCreateInfo infoVistaProf{};
    infoVistaProf.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    infoVistaProf.image = salida.profundidad;
    infoVistaProf.viewType = VK_IMAGE_VIEW_TYPE_2D;
    infoVistaProf.format = VK_FORMAT_D32_SFLOAT;
    infoVistaProf.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    infoVistaProf.subresourceRange.levelCount = 1;
    infoVistaProf.subresourceRange.layerCount = 1;
    if (!okVulkan(funciones_.vkCreateImageView(dispositivo_, &infoVistaProf,
                                               nullptr, &salida.vistaProfundidad),
                  "vkCreateImageView (profundidad del render target)"))
        return false;

    // Sampler de la imagen de color: la interfaz la muestra como textura y
    // para eso necesita un descriptor con vista y sampler.
    VkSamplerCreateInfo infoSampler{};
    infoSampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    infoSampler.magFilter = VK_FILTER_LINEAR;
    infoSampler.minFilter = VK_FILTER_LINEAR;
    infoSampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    infoSampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    infoSampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    infoSampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    infoSampler.mipLodBias = 0.0f;
    infoSampler.anisotropyEnable = VK_FALSE;
    infoSampler.maxLod = 1.0f;
    infoSampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    if (!okVulkan(funciones_.vkCreateSampler(dispositivo_, &infoSampler, nullptr,
                                             &salida.sampler),
                  "vkCreateSampler (render target)"))
        return false;

    VkFramebufferCreateInfo infoFb{};
    infoFb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    infoFb.renderPass = renderPassObjetivo_;
    infoFb.attachmentCount = 2;
    infoFb.pAttachments = nullptr;  // se arma abajo
    VkImageView adjuntos[2] = {salida.vistaColor, salida.vistaProfundidad};
    infoFb.pAttachments = adjuntos;
    infoFb.width = static_cast<uint32_t>(ancho);
    infoFb.height = static_cast<uint32_t>(alto);
    infoFb.layers = 1;
    if (!okVulkan(funciones_.vkCreateFramebuffer(dispositivo_, &infoFb, nullptr,
                                                 &salida.framebuffer),
                  "vkCreateFramebuffer (render target)"))
        return false;

    salida.ancho = ancho;
    salida.alto = alto;

    // La imagen de color arranca en UNDEFINED: el render pass la espera en
    // COLOR_ATTACHMENT_OPTIMAL, asi que se deja en ese layout desde ahora.
    if (!comandoAbierto_) {
        transicionarImagen(salida.color, VK_IMAGE_LAYOUT_UNDEFINED,
                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
                           VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                           VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        transicionarImagen(salida.profundidad, VK_IMAGE_LAYOUT_UNDEFINED,
                           VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0,
                           VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                           VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                           VK_IMAGE_ASPECT_DEPTH_BIT);
    }
    return true;
}

Handle BackendVulkan::createRenderTarget(int width, int height) {
    RenderTargetVulkan target;
    if (!crearRenderTarget(width, height, target)) return kInvalidHandle;
    return registrarRenderTarget(target);
}

void BackendVulkan::destroyRenderTarget(Handle target) {
    auto it = objetivos_.find(target);
    if (it == objetivos_.end()) return;
    RenderTargetVulkan& t = it->second;
    if (t.framebuffer)
        funciones_.vkDestroyFramebuffer(dispositivo_, t.framebuffer, nullptr);
    if (t.vistaColor)
        funciones_.vkDestroyImageView(dispositivo_, t.vistaColor, nullptr);
    if (t.sampler)
        funciones_.vkDestroySampler(dispositivo_, t.sampler, nullptr);
    if (t.color) funciones_.vkDestroyImage(dispositivo_, t.color, nullptr);
    if (t.memoriaColor)
        funciones_.vkFreeMemory(dispositivo_, t.memoriaColor, nullptr);
    if (t.vistaProfundidad)
        funciones_.vkDestroyImageView(dispositivo_, t.vistaProfundidad, nullptr);
    if (t.profundidad)
        funciones_.vkDestroyImage(dispositivo_, t.profundidad, nullptr);
    if (t.memoriaProfundidad)
        funciones_.vkFreeMemory(dispositivo_, t.memoriaProfundidad, nullptr);
    objetivos_.erase(it);
}

Handle BackendVulkan::renderTargetColorTexture(Handle target) const {
    auto it = objetivos_.find(target);
    if (it == objetivos_.end()) return kInvalidHandle;
    // El identificador que la GUI necesita es el de la vista de la imagen de
    // color: es lo que se liga como descriptor para muestrearla.
    return reinterpret_cast<Handle>(it->second.vistaColor);
}

void* BackendVulkan::imguiTextureId(Handle texture) const {
    // La fachada de la interfaz sabe convertir la textura en el identificador
    // que usa ImGui (en Vulkan es un conjunto de descriptores).
    ConversionTextura conversion = conversionTexturaActiva();
    if (conversion)
        return conversion(texture, contextoConversionTextura());

    auto it = texturas_.find(texture);
    if (it == texturas_.end()) {
        // Puede ser el identificador de una vista de render target.
        for (const auto& par : objetivos_) {
            if (reinterpret_cast<Handle>(par.second.vistaColor) == texture)
                return reinterpret_cast<void*>(par.second.vistaColor);
        }
        return nullptr;
    }
    return reinterpret_cast<void*>(it->second.vista);
}

bool BackendVulkan::texturaParaInterfaz(Handle textura,
                                         TexturaInterfaz& salida) const {
    auto it = texturas_.find(textura);
    if (it != texturas_.end()) {
        salida.vista = it->second.vista;
        salida.sampler = it->second.sampler;
        salida.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        return salida.vista != VK_NULL_HANDLE;
    }

    // Puede ser la vista del color de un render target (la vista previa de la
    // interfaz): al cerrar la pasada su imagen queda en el layout de
    // muestreo.
    for (const auto& par : objetivos_) {
        if (reinterpret_cast<Handle>(par.second.vistaColor) == textura) {
            salida.vista = par.second.vistaColor;
            salida.sampler = par.second.sampler;
            salida.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Lectura de pixeles
// ---------------------------------------------------------------------------

// Accessos y etapas de origen de una transicion de imagen, segun el layout
// del que se sale.
//
// El layout dice quien toco la imagen por ultima vez, y de eso depende la
// dependencia de memoria que hay que declarar: si la imagen estaba para
// muestrear, la ultima lectura la hicieron los shaders, y si estaba como
// attachment de color, la ultima escritura la hizo la etapa de color. Sin la
// dependencia correcta, una copia puede leer la imagen antes de que terminen
// las escrituras de la pasada anterior.
void origenDeLayout(VkImageLayout layout, VkAccessFlags& acceso,
                    VkPipelineStageFlags& etapa) {
    switch (layout) {
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            acceso = VK_ACCESS_SHADER_READ_BIT;
            etapa = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                    VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
            return;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            acceso = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            etapa = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            return;
        default:
            acceso = VK_ACCESS_MEMORY_READ_BIT;
            etapa = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            return;
    }
}

void BackendVulkan::copiarImagenABuffer(VkImage imagen, VkImageLayout origen,
                                         VkBuffer buffer, uint32_t ancho,
                                         uint32_t alto) {
    if (!imagen || !buffer || ancho == 0 || alto == 0) return;

    funciones_.vkResetCommandBuffer(comandoTransferencia_, 0);
    VkCommandBufferBeginInfo infoBegin{};
    infoBegin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    infoBegin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VkResult resBegin =
        funciones_.vkBeginCommandBuffer(comandoTransferencia_, &infoBegin);
    if (resBegin != VK_SUCCESS)
        reportarErrorVulkan("vkBeginCommandBuffer (copia)", resBegin);

    VkImageMemoryBarrier barrera{};
    barrera.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrera.oldLayout = origen;
    barrera.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrera.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrera.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrera.image = imagen;
    barrera.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrera.subresourceRange.levelCount = 1;
    barrera.subresourceRange.layerCount = 1;
    VkAccessFlags accesoOrigen = 0;
    VkPipelineStageFlags etapaOrigen = 0;
    origenDeLayout(origen, accesoOrigen, etapaOrigen);
    barrera.srcAccessMask = accesoOrigen;
    barrera.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    funciones_.vkCmdPipelineBarrier(
        comandoTransferencia_, etapaOrigen, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
        nullptr, 0, nullptr, 1, &barrera);

    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {ancho, alto, 1};
    funciones_.vkCmdCopyImageToBuffer(comandoTransferencia_, imagen,
                                      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                      buffer, 1, &region);

    // La imagen vuelve al layout en que estaba: la copia no tiene que dejar
    // la imagen en un estado que otro paso no espere.
    barrera.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrera.newLayout = origen;
    barrera.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrera.dstAccessMask = accesoOrigen;
    funciones_.vkCmdPipelineBarrier(
        comandoTransferencia_, VK_PIPELINE_STAGE_TRANSFER_BIT, etapaOrigen, 0, 0,
        nullptr, 0, nullptr, 1, &barrera);

    VkResult resEnd = funciones_.vkEndCommandBuffer(comandoTransferencia_);
    if (resEnd != VK_SUCCESS)
        reportarErrorVulkan("vkEndCommandBuffer (copia)", resEnd);

    VkSubmitInfo infoSubmit{};
    infoSubmit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    infoSubmit.commandBufferCount = 1;
    infoSubmit.pCommandBuffers = &comandoTransferencia_;
    VkResult resSubmit = funciones_.vkQueueSubmit(colaGrafica_, 1, &infoSubmit,
                                                   VK_NULL_HANDLE);
    if (resSubmit != VK_SUCCESS)
        reportarErrorVulkan("vkQueueSubmit", resSubmit);
    VkResult resEspera = funciones_.vkQueueWaitIdle(colaGrafica_);
    if (resEspera != VK_SUCCESS)
        reportarErrorVulkan("vkQueueWaitIdle", resEspera);
}

bool BackendVulkan::leerPixeles(Handle target,
                                std::vector<unsigned char>& salida) {
    auto it = objetivos_.find(target);
    if (it == objetivos_.end()) return false;
    RenderTargetVulkan& t = it->second;
    if (!t.color || t.ancho <= 0 || t.alto <= 0) return false;

    const std::size_t bytes =
        static_cast<std::size_t>(t.ancho) * static_cast<std::size_t>(t.alto) * 4;

    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memoria = VK_NULL_HANDLE;
    // El buffer de lectura tiene que ser visible para la CPU: es donde se
    // copian los pixeles.
    if (!crearBufferVacio(bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT, buffer,
                          memoria,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        return false;

    // La imagen esta en el layout en el que la dejo la ultima pasada.
    copiarImagenABuffer(t.color,
                         t.colorEnShaderRead
                             ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                             : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                         buffer, static_cast<uint32_t>(t.ancho),
                         static_cast<uint32_t>(t.alto));

    void* mapa = nullptr;
    if (!okVulkan(funciones_.vkMapMemory(dispositivo_, memoria, 0, bytes, 0,
                                         &mapa),
                  "vkMapMemory (lectura de pixeles)")) {
        funciones_.vkDestroyBuffer(dispositivo_, buffer, nullptr);
        funciones_.vkFreeMemory(dispositivo_, memoria, nullptr);
        return false;
    }

    const unsigned char* bytesImagen =
        static_cast<const unsigned char*>(mapa);
    salida.assign(bytesImagen, bytesImagen + bytes);

    funciones_.vkUnmapMemory(dispositivo_, memoria);
    funciones_.vkDestroyBuffer(dispositivo_, buffer, nullptr);
    funciones_.vkFreeMemory(dispositivo_, memoria, nullptr);
    return true;
}

void BackendVulkan::transicionarImagen(VkImage imagen, VkImageLayout origen,
                                        VkImageLayout destino,
                                        VkAccessFlags accesoOrigen,
                                        VkAccessFlags accesoDestino,
                                        VkPipelineStageFlags etapaOrigen,
                                        VkPipelineStageFlags etapaDestino,
                                        VkImageAspectFlags aspecto) {
    if (!imagen) return;

    VkImageMemoryBarrier barrera{};
    barrera.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrera.oldLayout = origen;
    barrera.newLayout = destino;
    barrera.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrera.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrera.image = imagen;
    barrera.subresourceRange.aspectMask = aspecto;
    barrera.subresourceRange.levelCount = 1;
    barrera.subresourceRange.layerCount = 1;
    barrera.srcAccessMask = accesoOrigen;
    barrera.dstAccessMask = accesoDestino;

    if (comandoAbierto_) {
        funciones_.vkCmdPipelineBarrier(comando_, etapaOrigen, etapaDestino, 0, 0,
                                        nullptr, 0, nullptr, 1, &barrera);
        return;
    }

    // Fuera de un frame: la transicion se graba y se espera.
    funciones_.vkResetCommandBuffer(comandoTransferencia_, 0);
    VkCommandBufferBeginInfo infoBegin{};
    infoBegin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    infoBegin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    funciones_.vkBeginCommandBuffer(comandoTransferencia_, &infoBegin);
    funciones_.vkCmdPipelineBarrier(comandoTransferencia_, etapaOrigen,
                                    etapaDestino, 0, 0, nullptr, 0, nullptr, 1,
                                    &barrera);
    funciones_.vkEndCommandBuffer(comandoTransferencia_);
    VkSubmitInfo infoSubmit{};
    infoSubmit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    infoSubmit.commandBufferCount = 1;
    infoSubmit.pCommandBuffers = &comandoTransferencia_;
    funciones_.vkQueueSubmit(colaGrafica_, 1, &infoSubmit, VK_NULL_HANDLE);
    funciones_.vkQueueWaitIdle(colaGrafica_);
}

} // namespace Vulkan
} // namespace Backend
} // namespace Rendering
