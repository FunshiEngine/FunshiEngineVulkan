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

#include "BackendVulkanUniformes.h"
#include "BackendVulkanUtil.h"
#include "ShadersSPIRV.h"

#include <cstdlib>
#include <cstring>
#include <iostream>

#include <glm/gtc/type_ptr.hpp>

namespace Rendering {
namespace Backend {
namespace Vulkan {

namespace {

// Floats por vertice de malla en el buffer del backend (posicion, normal, uv,
// tangente y bitangente, siempre en ese orden). El motor entrega los campos
// en arrays separados; el backend decide como los empaqueta, y lo hace igual
// para todas las mallas para que el pipeline tenga un solo tipo de vertice.
constexpr int kFloatsVerticeMalla = 14;

// Floats por vertice del batch de lineas (el formato de LineBuilder).
constexpr int kFloatsVerticeLinea = 12;

// Slices del anillo de uniforms que se reservan al principio. El anillo crece
// solo si un frame necesita mas draws de los que caben.
constexpr int kSlicesIniciales = 64;

// Nombre del uniform que vive en un offset del bloque, o nullptr si ese
// offset no corresponde a ningun uniform del programa.
const char* nombreDeOffset(const ProgramaVulkan& programa, int offset) {
    for (const EntradaUniforme& e : programa.layout.entradas()) {
        if (e.esSampler) continue;
        if (e.offset == offset) return e.nombre.c_str();
    }
    return nullptr;
}

} // namespace

// ---------------------------------------------------------------------------
// Creacion del programa
// ---------------------------------------------------------------------------

VkShaderModule BackendVulkan::moduloDesdeSpirv(const std::uint32_t* codigo,
                                                std::size_t palabras) {
    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = palabras * sizeof(std::uint32_t);
    info.pCode = codigo;
    VkShaderModule modulo = VK_NULL_HANDLE;
    if (!okVulkan(funciones_.vkCreateShaderModule(dispositivo_, &info, nullptr,
                                                  &modulo),
                  "vkCreateShaderModule"))
        return VK_NULL_HANDLE;
    return modulo;
}

bool BackendVulkan::crearPrograma(Contrato::ShaderKind kind) {
    auto existente = programasPorTipo_.find(kind);
    if (existente != programasPorTipo_.end() && existente->second) {
        return true;
    }

    auto programa = std::make_unique<ProgramaVulkan>();
    programa->kind = kind;
    programa->layout = LayoutUniformes(kind);

    // Modulos de shader (SPIR-V embebido en el ejecutable).
    switch (kind) {
        case Contrato::ShaderKind::Malla:
            programa->moduloVertex = moduloDesdeSpirv(Shaders::kMallaVert,
                                                      Shaders::kMallaVertBytes / 4);
            programa->moduloFragment = moduloDesdeSpirv(Shaders::kMallaFrag,
                                                        Shaders::kMallaFragBytes / 4);
            break;
        case Contrato::ShaderKind::Lineas:
            programa->moduloVertex = moduloDesdeSpirv(Shaders::kLineasVert,
                                                      Shaders::kLineasVertBytes / 4);
            programa->moduloFragment = moduloDesdeSpirv(Shaders::kLineasFrag,
                                                        Shaders::kLineasFragBytes / 4);
            break;
        case Contrato::ShaderKind::Sprites:
            programa->moduloVertex = moduloDesdeSpirv(Shaders::kSpritesVert,
                                                      Shaders::kSpritesVertBytes / 4);
            programa->moduloFragment = moduloDesdeSpirv(Shaders::kSpritesFrag,
                                                        Shaders::kSpritesFragBytes / 4);
            break;
        case Contrato::ShaderKind::Cielo:
            programa->moduloVertex = moduloDesdeSpirv(Shaders::kCieloVert,
                                                      Shaders::kCieloVertBytes / 4);
            programa->moduloFragment = moduloDesdeSpirv(Shaders::kCieloFrag,
                                                        Shaders::kCieloFragBytes / 4);
            break;
        case Contrato::ShaderKind::Cubemap:
            programa->moduloVertex = moduloDesdeSpirv(Shaders::kCubemapVert,
                                                      Shaders::kCubemapVertBytes / 4);
            programa->moduloFragment = moduloDesdeSpirv(Shaders::kCubemapFrag,
                                                        Shaders::kCubemapFragBytes / 4);
            break;
    }
    if (!programa->moduloVertex || !programa->moduloFragment) {
        std::cerr << "[Vulkan] No se pudo crear el modulo de shader del "
                  << "programa " << Contrato::nombreDe(kind) << "." << std::endl;
        return false;
    }

    // Layout de descriptores: binding 0 = bloque de uniforms (con offset
    // dinamico, una slice por draw) y a partir de ahi un binding por sampler,
    // en el orden en que aparecen en la tabla del contrato.
    const std::vector<EntradaUniforme>& samplers = programa->layout.samplers();
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    VkDescriptorSetLayoutBinding bloque{};
    bloque.binding = 0;
    bloque.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    bloque.descriptorCount = 1;
    bloque.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings.push_back(bloque);
    for (std::size_t i = 0; i < samplers.size(); ++i) {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = static_cast<uint32_t>(i + 1);
        binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings.push_back(binding);
    }

    VkDescriptorSetLayoutCreateInfo infoLayout{};
    infoLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    infoLayout.bindingCount = static_cast<uint32_t>(bindings.size());
    infoLayout.pBindings = bindings.data();
    if (!okVulkan(funciones_.vkCreateDescriptorSetLayout(
                     dispositivo_, &infoLayout, nullptr,
                     &programa->layoutDescriptores),
                  "vkCreateDescriptorSetLayout"))
        return false;

    VkPipelineLayoutCreateInfo infoPipelineLayout{};
    infoPipelineLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    infoPipelineLayout.setLayoutCount = 1;
    infoPipelineLayout.pSetLayouts = &programa->layoutDescriptores;
    if (!okVulkan(funciones_.vkCreatePipelineLayout(
                     dispositivo_, &infoPipelineLayout, nullptr,
                     &programa->layoutPipeline),
                  "vkCreatePipelineLayout"))
        return false;

    // Anillo de uniforms: un buffer visible para la CPU, con una slice por
    // draw (el motor escribe los valores, dibuja, los reescribe y vuelve a
    // dibujar; cada draw necesita su propia copia de los valores).
    programa->tamanoSlice = static_cast<int>(
        ((static_cast<std::size_t>(programa->layout.tamano()) +
          alineacionUniformes_ - 1) /
         alineacionUniformes_) *
        alineacionUniformes_);
    if (programa->tamanoSlice <= 0) programa->tamanoSlice = 256;
    programa->slicesTotales = kSlicesIniciales;
    programa->valores.assign(static_cast<std::size_t>(programa->layout.tamano()),
                              0);

    VkBufferCreateInfo infoBuffer{};
    infoBuffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    infoBuffer.size = static_cast<VkDeviceSize>(programa->tamanoSlice) *
                      static_cast<std::size_t>(programa->slicesTotales);
    infoBuffer.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    infoBuffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (!okVulkan(funciones_.vkCreateBuffer(dispositivo_, &infoBuffer, nullptr,
                                            &programa->bufferUniformes),
                  "vkCreateBuffer (uniforms)"))
        return false;

    VkMemoryRequirements req{};
    funciones_.vkGetBufferMemoryRequirements(dispositivo_, programa->bufferUniformes,
                                             &req);
    if (!asignarMemoria(req,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        programa->memoriaUniformes))
        return false;
    funciones_.vkBindBufferMemory(dispositivo_, programa->bufferUniformes,
                                 programa->memoriaUniformes, 0);
    if (!okVulkan(funciones_.vkMapMemory(dispositivo_, programa->memoriaUniformes,
                                         0, VK_WHOLE_SIZE, 0,
                                         &programa->mapaUniformes),
                  "vkMapMemory (uniforms)"))
        return false;

    // Unidad de texturizado por sampler: por defecto es el indice del
    // sampler (difusa 0, especular 1, emision 2, normal 3), que es lo que el
    // motor indica con setUniformInt.
    programa->unidadesSampler.resize(samplers.size());
    for (std::size_t i = 0; i < samplers.size(); ++i)
        programa->unidadesSampler[i] = static_cast<int>(i);

    programasPorTipo_[kind] = std::move(programa);
    return true;
}

// ---------------------------------------------------------------------------
// Pipelines
// ---------------------------------------------------------------------------

VkPipeline BackendVulkan::crearPipeline(ProgramaVulkan& programa,
                                        const ClavePipeline& clave) {
    const ClaveEstado& estado = clave.estado;

    // El render pass y el numero de muestras dependen del destino: el del
    // swapchain resuelve desde una imagen con multisample y el de los render
    // targets dibuja sobre una imagen de una sola muestra.
    VkRenderPass renderPass = renderPassObjetivo_;
    VkSampleCountFlagBits muestras = VK_SAMPLE_COUNT_1_BIT;
    if (clave.destino == DestinoActual::Swapchain) {
        renderPass = renderPassSwapchain_;
        muestras = static_cast<VkSampleCountFlagBits>(muestras_);
    }
    if (!renderPass) return VK_NULL_HANDLE;

    // Layout de vertices: depende del tipo de programa.
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription atributos[5];
    uint32_t cantidadAtributos = 0;

    switch (programa.kind) {
        case Contrato::ShaderKind::Malla:
            binding.stride = kFloatsVerticeMalla * sizeof(float);
            cantidadAtributos = 5;
            atributos[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0};
            atributos[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, 3 * sizeof(float)};
            atributos[2] = {2, 0, VK_FORMAT_R32G32_SFLOAT, 6 * sizeof(float)};
            atributos[3] = {3, 0, VK_FORMAT_R32G32B32_SFLOAT, 8 * sizeof(float)};
            atributos[4] = {4, 0, VK_FORMAT_R32G32B32_SFLOAT, 11 * sizeof(float)};
            break;
        case Contrato::ShaderKind::Lineas:
            binding.stride = kFloatsVerticeLinea * sizeof(float);
            cantidadAtributos = 5;
            atributos[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0};
            atributos[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, 3 * sizeof(float)};
            atributos[2] = {2, 0, VK_FORMAT_R32_SFLOAT, 6 * sizeof(float)};
            atributos[3] = {3, 0, VK_FORMAT_R32_SFLOAT, 7 * sizeof(float)};
            atributos[4] = {4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 8 * sizeof(float)};
            break;
        case Contrato::ShaderKind::Sprites:
            // El quad de los sprites usa el layout de la malla (posicion en el
            // atributo 0 y uv en el 2); los demas atributos no los usa.
            binding.stride = kFloatsVerticeMalla * sizeof(float);
            cantidadAtributos = 2;
            atributos[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0};
            atributos[1] = {2, 0, VK_FORMAT_R32G32_SFLOAT, 6 * sizeof(float)};
            break;
        case Contrato::ShaderKind::Cielo:
            // Triangulo a pantalla completa: la geometria la arma el vertex
            // shader con el indice del vertice, no hay buffer.
            break;
        case Contrato::ShaderKind::Cubemap:
            binding.stride = kFloatsVerticeMalla * sizeof(float);
            cantidadAtributos = 1;
            atributos[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0};
            break;
    }

    VkPipelineVertexInputStateCreateInfo infoEntrada{};
    infoEntrada.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    infoEntrada.vertexBindingDescriptionCount = cantidadAtributos > 0 ? 1 : 0;
    infoEntrada.pVertexBindingDescriptions =
        cantidadAtributos > 0 ? &binding : nullptr;
    infoEntrada.vertexAttributeDescriptionCount = cantidadAtributos;
    infoEntrada.pVertexAttributeDescriptions =
        cantidadAtributos > 0 ? atributos : nullptr;


    VkPipelineInputAssemblyStateCreateInfo infoEnsamblado{};
    infoEnsamblado.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    infoEnsamblado.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    infoEnsamblado.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo infoViewport{};
    infoViewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    infoViewport.viewportCount = 1;
    infoViewport.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo infoRaster{};
    infoRaster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    infoRaster.depthClampEnable = VK_FALSE;
    infoRaster.rasterizerDiscardEnable = VK_FALSE;
    infoRaster.polygonMode = VK_POLYGON_MODE_FILL;
    infoRaster.cullMode = VK_CULL_MODE_NONE;
    infoRaster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    infoRaster.depthBiasEnable = VK_FALSE;
    infoRaster.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo infoMuestras{};
    infoMuestras.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    infoMuestras.rasterizationSamples = muestras;
    infoMuestras.sampleShadingEnable = VK_FALSE;

    VkPipelineDepthStencilStateCreateInfo infoProfundidad{};
    infoProfundidad.sType =
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    infoProfundidad.depthTestEnable = estado.testProfundidad ? VK_TRUE : VK_FALSE;
    infoProfundidad.depthWriteEnable =
        estado.escrituraProfundidad ? VK_TRUE : VK_FALSE;
    infoProfundidad.depthCompareOp = estado.funcion == kDepthFuncLessEqual
                                         ? VK_COMPARE_OP_LESS_OR_EQUAL
                                         : VK_COMPARE_OP_LESS;
    infoProfundidad.depthBoundsTestEnable = VK_FALSE;
    infoProfundidad.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState adjuntoColor{};
    if (estado.blend) {
        adjuntoColor.blendEnable = VK_TRUE;
        adjuntoColor.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        adjuntoColor.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        adjuntoColor.colorBlendOp = VK_BLEND_OP_ADD;
        adjuntoColor.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        adjuntoColor.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        adjuntoColor.alphaBlendOp = VK_BLEND_OP_ADD;
        adjuntoColor.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                      VK_COLOR_COMPONENT_G_BIT |
                                      VK_COLOR_COMPONENT_B_BIT |
                                      VK_COLOR_COMPONENT_A_BIT;
    } else {
        adjuntoColor.blendEnable = VK_FALSE;
        adjuntoColor.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                      VK_COLOR_COMPONENT_G_BIT |
                                      VK_COLOR_COMPONENT_B_BIT |
                                      VK_COLOR_COMPONENT_A_BIT;
    }

    VkPipelineColorBlendStateCreateInfo infoColor{};
    infoColor.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    infoColor.attachmentCount = 1;
    infoColor.pAttachments = &adjuntoColor;

    // El viewport y el scissor son dinamicos: el motor cambia de encuadre
    // entre la pasada principal y las vistas previas.
    VkDynamicState estadosDinamicos[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                          VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo infoDinamica{};
    infoDinamica.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    infoDinamica.dynamicStateCount = 2;
    infoDinamica.pDynamicStates = estadosDinamicos;

    VkPipelineShaderStageCreateInfo etapas[2]{};
    etapas[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    etapas[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    etapas[0].module = programa.moduloVertex;
    etapas[0].pName = "main";
    etapas[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    etapas[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    etapas[1].module = programa.moduloFragment;
    etapas[1].pName = "main";

    VkGraphicsPipelineCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.stageCount = 2;
    info.pStages = etapas;
    info.pVertexInputState = &infoEntrada;
    info.pInputAssemblyState = &infoEnsamblado;
    info.pViewportState = &infoViewport;
    info.pRasterizationState = &infoRaster;
    info.pMultisampleState = &infoMuestras;
    info.pDepthStencilState = &infoProfundidad;
    info.pColorBlendState = &infoColor;
    info.pDynamicState = &infoDinamica;
    info.layout = programa.layoutPipeline;
    info.renderPass = renderPass;
    info.subpass = 0;

    VkPipeline pipeline = VK_NULL_HANDLE;
    if (!okVulkan(funciones_.vkCreateGraphicsPipelines(
                     dispositivo_, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline),
                  "vkCreateGraphicsPipelines"))
        return VK_NULL_HANDLE;
    return pipeline;
}

VkPipeline BackendVulkan::pipelinePara(ProgramaVulkan& programa,
                                       const ClaveEstado& estado,
                                       DestinoActual destino) {
    const ClavePipeline clave{estado, destino};
    auto it = programa.pipelines.find(clave);
    if (it != programa.pipelines.end()) return it->second.pipeline;

    // El pipeline se crea la primera vez que se usa esa combinacion de
    // estado: en Vulkan el estado de profundidad y de blend es del pipeline, y
    // el motor puede pedir varias combinaciones del mismo programa.
    VkPipeline pipeline = crearPipeline(programa, clave);
    if (!pipeline) return VK_NULL_HANDLE;
    programa.pipelines[clave] = PipelineVulkan{pipeline, estado};
    return pipeline;
}

// ---------------------------------------------------------------------------
// Programa: ciclo de vida
// ---------------------------------------------------------------------------

Handle BackendVulkan::createProgram(Contrato::ShaderKind kind) {
    if (!dispositivo_) return kInvalidHandle;

    if (!crearPrograma(kind)) {
        programasPorTipo_.erase(kind);
        return kInvalidHandle;
    }

    // El programa de un tipo es compartido: el motor pide uno por renderer y
    // el backend reparte el mismo pipeline, layout y bloque de uniforms.
    ++programasPorTipo_[kind]->refs;

    return registrarPrograma(kind);
}

// Registra un programa y devuelve su handle opaco.
Handle BackendVulkan::registrarPrograma(Contrato::ShaderKind kind) {
    const Handle handle = ++contadorRecursos_;
    programas_[handle] = kind;
    return handle;
}

void BackendVulkan::destroyProgram(Handle program) {
    auto it = programas_.find(program);
    if (it == programas_.end()) return;
    const Contrato::ShaderKind kind = it->second;
    programas_.erase(it);

    auto encontrado = programasPorTipo_.find(kind);
    if (encontrado == programasPorTipo_.end()) return;
    ProgramaVulkan& p = *encontrado->second;
    if (p.refs > 0) --p.refs;
    if (p.refs > 0) return;

    // Ultimo usuario del programa de este tipo: se liberan los recursos.
    if (p.mapaUniformes)
        funciones_.vkUnmapMemory(dispositivo_, p.memoriaUniformes);
    if (p.bufferUniformes)
        funciones_.vkDestroyBuffer(dispositivo_, p.bufferUniformes, nullptr);
    if (p.memoriaUniformes)
        funciones_.vkFreeMemory(dispositivo_, p.memoriaUniformes, nullptr);
    for (auto& par : p.pipelines)
        if (par.second.pipeline)
            funciones_.vkDestroyPipeline(dispositivo_, par.second.pipeline, nullptr);
    if (p.layoutPipeline)
        funciones_.vkDestroyPipelineLayout(dispositivo_, p.layoutPipeline, nullptr);
    if (p.layoutDescriptores)
        funciones_.vkDestroyDescriptorSetLayout(dispositivo_,
                                                 p.layoutDescriptores, nullptr);
    if (p.moduloVertex)
        funciones_.vkDestroyShaderModule(dispositivo_, p.moduloVertex, nullptr);
    if (p.moduloFragment)
        funciones_.vkDestroyShaderModule(dispositivo_, p.moduloFragment, nullptr);
    programasPorTipo_.erase(encontrado);
}

void BackendVulkan::useProgram(Handle program) {
    auto it = programas_.find(program);
    if (it == programas_.end()) {
        hayProgramaEnUso_ = false;
        programaEnUso_ = nullptr;
        return;
    }
    auto encontrado = programasPorTipo_.find(it->second);
    if (encontrado == programasPorTipo_.end()) {
        hayProgramaEnUso_ = false;
        programaEnUso_ = nullptr;
        return;
    }
    kindEnUso_ = it->second;
    programaEnUso_ = encontrado->second.get();
    hayProgramaEnUso_ = true;
}

int BackendVulkan::uniformLocation(Handle program, const char* name) {
    auto it = programas_.find(program);
    if (it == programas_.end()) return kTokenInvalido;
    auto encontrado = programasPorTipo_.find(it->second);
    if (encontrado == programasPorTipo_.end()) return kTokenInvalido;
    return encontrado->second->layout.resolver(name);
}

// ---------------------------------------------------------------------------
// Uniformes
// ---------------------------------------------------------------------------

void BackendVulkan::convertirMatriz(Contrato::ShaderKind kind, const char* nombre,
                                     float* destino) {
    if (!nombre || !destino) return;

    if (std::strcmp(nombre, Contrato::Uniforme::kProjection) == 0) {
        // El motor entrega la proyeccion en el convenio de OpenGL: la Y
        // crece hacia arriba y la profundidad va de -1 a 1. Vulkan usa la Y
        // hacia abajo y la profundidad de 0 a 1.
        //
        // La matriz esta en orden column-major (el de glm): el elemento
        // col*4+fila. La fila 1 (la Y) son los indices 1, 5, 9 y 13; la fila
        // 2 (la profundidad) son 2, 6, 10 y 14, y la fila 3 (la de W) son
        // 3, 7, 11 y 15.
        destino[1] = -destino[1];
        destino[5] = -destino[5];
        destino[9] = -destino[9];
        destino[13] = -destino[13];
        for (int i = 0; i < 4; ++i) {
            destino[2 + i * 4] = 0.5f * destino[2 + i * 4] + 0.5f * destino[3 + i * 4];
        }
        return;
    }

    if (std::strcmp(nombre, Contrato::Uniforme::kInvViewProj) == 0) {
        // La inversa de projection*view se usa para des-proyectar el NDC y
        // sacar la direccion de vista de cada pixel. Con la Y invertida (la
        // misma convencion del pixel en Vulkan) el rayo sale igual que en
        // OpenGL: la columna 1 de la matriz son los indices 4 a 7.
        destino[4] = -destino[4];
        destino[5] = -destino[5];
        destino[6] = -destino[6];
        destino[7] = -destino[7];
    }
}

void BackendVulkan::setUniformMat4(int location, const glm::mat4& value) {
    if (!programaEnUso_ || location < 0) return;
    ProgramaVulkan& p = *programaEnUso_;
    // La escritura la hace escribirMatriz4: el limite se comprueba contra el
    // tamano real del bloque y en bytes, que es como esta el UBO.
    if (!escribirMatriz4(reinterpret_cast<unsigned char*>(p.valores.data()),
                         p.valores.size(), static_cast<std::size_t>(location),
                         glm::value_ptr(value)))
        return;
    convertirMatriz(p.kind, nombreDeOffset(p, location),
                    reinterpret_cast<float*>(p.valores.data() + location));
}

void BackendVulkan::setUniformMat3(int location, const glm::mat3& value) {
    if (!programaEnUso_ || location < 0) return;
    ProgramaVulkan& p = *programaEnUso_;
    if (!escribirMatriz3(reinterpret_cast<unsigned char*>(p.valores.data()),
                         p.valores.size(), static_cast<std::size_t>(location),
                         glm::value_ptr(value)))
        return;
}

void BackendVulkan::setUniformVec3(int location, const glm::vec3& value) {
    if (!programaEnUso_ || location < 0) return;
    ProgramaVulkan& p = *programaEnUso_;
    if (location + 12 > static_cast<int>(p.valores.size())) return;
    float* destino = reinterpret_cast<float*>(p.valores.data() + location);
    destino[0] = value.x;
    destino[1] = value.y;
    destino[2] = value.z;
}

void BackendVulkan::setUniformVec4(int location, const float* value) {
    if (!programaEnUso_ || location < 0 || !value) return;
    ProgramaVulkan& p = *programaEnUso_;
    if (location + 16 > static_cast<int>(p.valores.size())) return;
    float* destino = reinterpret_cast<float*>(p.valores.data() + location);
    std::memcpy(destino, value, 16);
}

void BackendVulkan::setUniformFloat(int location, float value) {
    if (!programaEnUso_ || location < 0) return;
    ProgramaVulkan& p = *programaEnUso_;
    if (location + 4 > static_cast<int>(p.valores.size())) return;
    float* destino = reinterpret_cast<float*>(p.valores.data() + location);
    destino[0] = value;
}

void BackendVulkan::setUniformInt(int location, int value) {
    if (!programaEnUso_ || location == kTokenInvalido) return;
    ProgramaVulkan& p = *programaEnUso_;

    if (location < 0) {
        // Token de sampler: el valor es la unidad de texturizado a la que
        // apunta. En OpenGL el sampler era una location de texture unit; en
        // Vulkan el sampler es un descriptor, asi que la unidad se resuelve
        // al armar el descriptor.
        const int indice = kPrimerTokenSampler - location;
        if (indice < 0 || indice >= static_cast<int>(p.unidadesSampler.size()))
            return;
        p.unidadesSampler[static_cast<std::size_t>(indice)] = value;
        return;
    }

    if (location + 4 > static_cast<int>(p.valores.size())) return;
    int* destino = reinterpret_cast<int*>(p.valores.data() + location);
    *destino = value;
}

// ---------------------------------------------------------------------------
// Uniformes: slice del anillo y descriptor set
// ---------------------------------------------------------------------------

void BackendVulkan::ajustarCapacidadUniformes(ProgramaVulkan& programa,
                                               int slices) {
    if (slices <= programa.slicesTotales) return;

    // El anillo crece: se crea un buffer mas grande. El buffer viejo se
    // retira (los comandos ya grabados en el frame lo referencian) y se
    // libera al principio del frame siguiente.
    const int nuevas = slices;
    VkBufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = static_cast<VkDeviceSize>(programa.tamanoSlice) *
                 static_cast<VkDeviceSize>(nuevas);
    info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkBuffer buffer = VK_NULL_HANDLE;
    if (!okVulkan(funciones_.vkCreateBuffer(dispositivo_, &info, nullptr, &buffer),
                  "vkCreateBuffer (uniforms)"))
        return;

    VkMemoryRequirements req{};
    funciones_.vkGetBufferMemoryRequirements(dispositivo_, buffer, &req);
    VkDeviceMemory memoria = VK_NULL_HANDLE;
    if (!asignarMemoria(req,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        memoria)) {
        funciones_.vkDestroyBuffer(dispositivo_, buffer, nullptr);
        return;
    }
    funciones_.vkBindBufferMemory(dispositivo_, buffer, memoria, 0);
    void* mapa = nullptr;
    if (!okVulkan(funciones_.vkMapMemory(dispositivo_, memoria, 0, VK_WHOLE_SIZE, 0,
                                         &mapa),
                  "vkMapMemory (uniforms)")) {
        funciones_.vkDestroyBuffer(dispositivo_, buffer, nullptr);
        funciones_.vkFreeMemory(dispositivo_, memoria, nullptr);
        return;
    }

    if (programa.mapaUniformes)
        funciones_.vkUnmapMemory(dispositivo_, programa.memoriaUniformes);
    if (programa.bufferUniformes) {
        buffersRetirados_.push_back(programa.bufferUniformes);
        memoriasRetiradas_.push_back(programa.memoriaUniformes);
    }
    programa.bufferUniformes = buffer;
    programa.memoriaUniformes = memoria;
    programa.mapaUniformes = mapa;
    programa.slicesTotales = nuevas;
    programa.siguienteSlice = 0;
}

void BackendVulkan::escribirSlice(ProgramaVulkan& programa) {
    if (!programa.mapaUniformes) return;

    if (programa.siguienteSlice >= programa.slicesTotales) {
        // No quedan slices: el anillo crece y se vuelve a empezar.
        ajustarCapacidadUniformes(programa, programa.slicesTotales * 2);
        programa.siguienteSlice = 0;
    }

    const std::size_t offset = static_cast<std::size_t>(programa.siguienteSlice) *
                               static_cast<std::size_t>(programa.tamanoSlice);
    auto* base = static_cast<unsigned char*>(programa.mapaUniformes);
    std::memcpy(base + offset, programa.valores.data(), programa.valores.size());
    ++programa.siguienteSlice;
}

VkDescriptorSet BackendVulkan::prepararDescriptorSet(ProgramaVulkan& programa) {
    if (!poolDescriptores_) return VK_NULL_HANDLE;

    VkDescriptorSetAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.descriptorPool = poolDescriptores_;
    info.descriptorSetCount = 1;
    info.pSetLayouts = &programa.layoutDescriptores;
    VkDescriptorSet conjunto = VK_NULL_HANDLE;
    if (!okVulkan(funciones_.vkAllocateDescriptorSets(dispositivo_, &info, &conjunto),
                  "vkAllocateDescriptorSets"))
        return VK_NULL_HANDLE;

    // Bloque de uniforms: el offset dinamico lo fija el draw (la slice del
    // anillo que le corresponde).
    VkDescriptorBufferInfo infoBuffer{};
    infoBuffer.buffer = programa.bufferUniformes;
    infoBuffer.offset = 0;
    infoBuffer.range = static_cast<VkDeviceSize>(programa.layout.tamano());

    VkWriteDescriptorSet escritura{};
    escritura.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    escritura.dstSet = conjunto;
    escritura.dstBinding = 0;
    escritura.dstArrayElement = 0;
    escritura.descriptorCount = 1;
    escritura.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    escritura.pBufferInfo = &infoBuffer;

    std::vector<VkDescriptorImageInfo> imagenes;
    std::vector<VkWriteDescriptorSet> escrituras;
    escrituras.push_back(escritura);

    const std::vector<EntradaUniforme>& samplers = programa.layout.samplers();
    imagenes.resize(samplers.size());
    for (std::size_t i = 0; i < samplers.size(); ++i) {
        const int unidad = i < programa.unidadesSampler.size()
                               ? programa.unidadesSampler[i]
                               : static_cast<int>(i);
        const bool esCubemap =
            samplers[i].tipo == Contrato::TipoUniforme::SamplerCube;

        VkImageView vista = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        if (esCubemap) {
            const Handle handle = unidad < static_cast<int>(cubemapPorUnidad_.size())
                                      ? cubemapPorUnidad_[static_cast<std::size_t>(unidad)]
                                      : kInvalidHandle;
            auto it = texturas_.find(handle);
            if (it != texturas_.end()) {
                vista = it->second.vista;
                sampler = it->second.sampler;
            }
        } else {
            const Handle handle = unidad < static_cast<int>(textura2DPorUnidad_.size())
                                      ? textura2DPorUnidad_[static_cast<std::size_t>(unidad)]
                                      : kInvalidHandle;
            auto it = texturas_.find(handle);
            if (it != texturas_.end()) {
                vista = it->second.vista;
                sampler = it->second.sampler;
            }
        }
        if (!vista) {
            // Sin textura: se liga la blanca de 1x1, para que el descriptor
            // sea valido (un descriptor sin imagen no lo es).
            vista = texturaBlanca_.vista;
            sampler = texturaBlanca_.sampler;
        }

        imagenes[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imagenes[i].imageView = vista;
        imagenes[i].sampler = sampler;

        VkWriteDescriptorSet escrituraSampler{};
        escrituraSampler.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        escrituraSampler.dstSet = conjunto;
        escrituraSampler.dstBinding = static_cast<uint32_t>(i + 1);
        escrituraSampler.dstArrayElement = 0;
        escrituraSampler.descriptorCount = 1;
        escrituraSampler.descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        escrituraSampler.pImageInfo = &imagenes[i];
        escrituras.push_back(escrituraSampler);
    }

    funciones_.vkUpdateDescriptorSets(dispositivo_,
                                      static_cast<uint32_t>(escrituras.size()),
                                      escrituras.data(), 0, nullptr);
    return conjunto;
}

} // namespace Vulkan
} // namespace Backend
} // namespace Rendering
