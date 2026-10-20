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
#include "MeshRenderer.h"
#include "ContratoRender.h"
#include "../Diagnosticos/RegistroDiagnosticos.h"

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>

#include "../Assets/Mesh.h"
#include "../Assets/TextureManager.h"
#include "../Iluminacion/LightSystem.h"
#include "../Objetos/Componentes/Color.h"
#include "../Objetos/Componentes/Material.h"
#include "../Objetos/Componentes/Model.h"
#include "../Objetos/GameObject.h"
#include "../Objetos/Modelos3D.h"
#include "MeshGPU.h"
#include "Shaders/ShaderProgram.h"
#include "TexturaGPU.h"
#include "Backend/IRenderBackend.h"

namespace {

bool matrizFinita(const glm::mat3& m) {
    const float* p = glm::value_ptr(m);
    for (int i = 0; i < 9; ++i)
        if (!std::isfinite(p[i])) return false;
    return true;
}

} // namespace

MeshRenderer::MeshRenderer() = default;

MeshRenderer::~MeshRenderer() = default;

bool MeshRenderer::inicializar() {
    if (shader_) return shader_->isValid();
    // El backend tiene que estar disponible para poder dibujar: la
    // inicializacion (que necesita la ventana) la hace la Ventana al
    // arrancar.
    if (!Rendering::Backend::activeBackend().available()) return false;
    try {
        shader_ = std::make_unique<ShaderProgram>(
            ShaderProgram::create(Rendering::Contrato::ShaderKind::Malla));
        aplicarLuces();
    } catch (const std::exception& e) {
        registrarExcepcionDiagnostica("Rendering/MeshRenderer", e,
                                      NivelDiagnostico::Error);
        // Sin shader no hay pipeline: available() queda en false y la pasada
        // dibuja menos, sin objeto que avise uno por uno. El error va al log
        // porque es la causa raiz de una escena vacia.
        std::cerr << "[MeshRenderer] Shader por defecto no disponible: "
                  << e.what() << '\n';
        shader_.reset();
    }
    return shader_ != nullptr && shader_->isValid();
}

void MeshRenderer::clearCache() {
    gpu_.clear();
    gpuTexturas_.clear();
    texturasFallidas_.clear();
    mallasSinNormales_.clear();
}

void MeshRenderer::setLuces(const LightData* luces, int lucesCount,
                            const float* globalAmbient) {
    globalAmbient_[0] = globalAmbient[0];
    globalAmbient_[1] = globalAmbient[1];
    globalAmbient_[2] = globalAmbient[2];

    luces_.clear();
    if (luces && lucesCount > 0)
        luces_.assign(luces, luces + lucesCount);

    if (shader_) aplicarLuces();
}

void MeshRenderer::aplicarLuces() {
    // Los seters de ShaderProgram no hacen glUseProgram: sin este use() los
    // uniforms de luz se descartarian (no hay programa activo al venir desde
    // setLuces/prepararLucesFrame antes de dibujar).
    if (shader_) shader_->use();
    const int count =
        std::min(LightSystem::kMaxLights, static_cast<int>(luces_.size()));

    shader_->setInt(Rendering::Contrato::Uniforme::kLightCount, count);
    shader_->setVec3(Rendering::Contrato::Uniforme::kGlobalAmbient,
                     glm::vec3(globalAmbient_[0], globalAmbient_[1],
                               globalAmbient_[2]));

    for (int i = 0; i < count; ++i) {
        const LightData& l = luces_[static_cast<std::size_t>(i)];
        char name[32];

        std::snprintf(name, sizeof(name), "%s[%d]",
                      Rendering::Contrato::Uniforme::kLightTypes, i);
        shader_->setInt(name, l.type);
        std::snprintf(name, sizeof(name), "%s[%d]",
                      Rendering::Contrato::Uniforme::kLightWorldPos, i);
        shader_->setVec3(name, glm::vec3(l.worldPos[0], l.worldPos[1],
                                         l.worldPos[2]));
        std::snprintf(name, sizeof(name), "%s[%d]",
                      Rendering::Contrato::Uniforme::kLightDir, i);
        shader_->setVec3(name, glm::vec3(l.direction[0], l.direction[1],
                                         l.direction[2]));
        std::snprintf(name, sizeof(name), "%s[%d]",
                      Rendering::Contrato::Uniforme::kLightAmbient, i);
        shader_->setVec3(name, glm::vec3(l.ambient[0], l.ambient[1],
                                         l.ambient[2]));
        std::snprintf(name, sizeof(name), "%s[%d]",
                      Rendering::Contrato::Uniforme::kLightDiffuse, i);
        shader_->setVec3(name, glm::vec3(l.diffuse[0], l.diffuse[1],
                                         l.diffuse[2]));
        std::snprintf(name, sizeof(name), "%s[%d]",
                      Rendering::Contrato::Uniforme::kLightSpecular, i);
        shader_->setVec3(name, glm::vec3(l.specular[0], l.specular[1],
                                         l.specular[2]));
        std::snprintf(name, sizeof(name), "%s[%d]",
                      Rendering::Contrato::Uniforme::kLightConstant, i);
        shader_->setFloat(name, l.constant);
        std::snprintf(name, sizeof(name), "%s[%d]",
                      Rendering::Contrato::Uniforme::kLightLinear, i);
        shader_->setFloat(name, l.linear);
        std::snprintf(name, sizeof(name), "%s[%d]",
                      Rendering::Contrato::Uniforme::kLightQuadratic, i);
        shader_->setFloat(name, l.quadratic);
        std::snprintf(name, sizeof(name), "%s[%d]",
                      Rendering::Contrato::Uniforme::kSpotCutoffCos, i);
        shader_->setFloat(
            name, std::cos(l.spotCutoffDegrees * 3.14159265358979f / 180.f));
    }
}

void MeshRenderer::aplicarMaterial(GameObject* objeto) {
    if (Material* material = objeto->getComponent<Material>()) {
        shader_->setVec4(Rendering::Contrato::Uniforme::kMaterialAmbient, material->getAmbient());
        shader_->setVec4(Rendering::Contrato::Uniforme::kMaterialDiffuse, material->getDiffuse());
        shader_->setVec4(Rendering::Contrato::Uniforme::kMaterialSpecular, material->getSpecular());
        shader_->setVec4(Rendering::Contrato::Uniforme::kMaterialEmission, material->getEmission());
        shader_->setFloat(Rendering::Contrato::Uniforme::kMaterialShininess, material->getShininess());
        return;
    }

    if (Color* color = objeto->getComponent<Color>()) {
        // El pipeline inmediato solo seteaba GL_DIFFUSE al color; el resto
        // quedaba con el default GL (ambiente gris tenue, sin especular).
        const float* c = color->getColor();
        const float ambient[4] = {0.2f, 0.2f, 0.2f, 1.f};
        const float specular[4] = {0.f, 0.f, 0.f, 1.f};
        const float emission[4] = {0.f, 0.f, 0.f, 1.f};
        shader_->setVec4(Rendering::Contrato::Uniforme::kMaterialAmbient, ambient);
        shader_->setVec4(Rendering::Contrato::Uniforme::kMaterialDiffuse, c);
        shader_->setVec4(Rendering::Contrato::Uniforme::kMaterialSpecular, specular);
        shader_->setVec4(Rendering::Contrato::Uniforme::kMaterialEmission, emission);
        shader_->setFloat(Rendering::Contrato::Uniforme::kMaterialShininess, 0.f);
        return;
    }

    // Sin material ni color: defaults del componente Material.
    const Material defaults;
    shader_->setVec4(Rendering::Contrato::Uniforme::kMaterialAmbient, defaults.getAmbient());
    shader_->setVec4(Rendering::Contrato::Uniforme::kMaterialDiffuse, defaults.getDiffuse());
    shader_->setVec4(Rendering::Contrato::Uniforme::kMaterialSpecular, defaults.getSpecular());
    shader_->setVec4(Rendering::Contrato::Uniforme::kMaterialEmission, defaults.getEmission());
    shader_->setFloat(Rendering::Contrato::Uniforme::kMaterialShininess, defaults.getShininess());
}

bool MeshRenderer::enlazarSlotTextura(const std::string& path, int unit,
                                      const char* samplerUniform) {
    if (path.empty() || texturasFallidas_.count(path) != 0) return false;
    try {
        std::shared_ptr<const Image> image = textureManager_->getTexture(path);
        auto it = gpuTexturas_.find(image.get());
        if (it == gpuTexturas_.end()) {
            auto gpu = std::make_unique<TexturaGPU>();
            gpu->upload(*image);
            it = gpuTexturas_.emplace(image.get(), std::move(gpu)).first;
        }
        it->second->bindUnit(unit);
        shader_->setInt(samplerUniform, unit);
        return true;
    } catch (const std::exception& e) {
        texturasFallidas_.insert(path); // no reintentar por frame
        registrarExcepcionDiagnostica("Rendering/MeshRenderer", e,
                                      NivelDiagnostico::Advertencia);
        std::cerr << "[MeshRenderer] Textura '" << path << "': " << e.what()
                  << '\n';
        return false;
    }
}

void MeshRenderer::aplicarTexturas(GameObject* objeto, const Mesh* mesh) {
    // Por defecto: sin texturas en ningun slot.
    shader_->setInt(Rendering::Contrato::Uniforme::kUseTexture, 0);
    shader_->setInt(Rendering::Contrato::Uniforme::kUseSpecularMap, 0);
    shader_->setInt(Rendering::Contrato::Uniforme::kUseEmissionMap, 0);
    shader_->setInt(Rendering::Contrato::Uniforme::kUseNormalMap, 0);

    if (!textureManager_ || !mesh || !mesh->hasUvs()) {
        return;
    }

    Material* material = objeto->getComponent<Material>();
    if (!material) return;

    shader_->setInt(Rendering::Contrato::Uniforme::kUseTexture,
                    enlazarSlotTextura(material->getDiffuseMapPath(), 0,
                                       Rendering::Contrato::Uniforme::kDiffuseTex)
                        ? 1
                        : 0);
    shader_->setInt(Rendering::Contrato::Uniforme::kUseSpecularMap,
                    enlazarSlotTextura(material->getSpecularMapPath(), 1,
                                       Rendering::Contrato::Uniforme::kSpecularTex)
                        ? 1
                        : 0);
    shader_->setInt(Rendering::Contrato::Uniforme::kUseEmissionMap,
                    enlazarSlotTextura(material->getEmissionMapPath(), 2,
                                       Rendering::Contrato::Uniforme::kEmissionTex)
                        ? 1
                        : 0);

    // El normal map ademas exige el marco tangente de la malla.
    shader_->setInt(Rendering::Contrato::Uniforme::kUseNormalMap,
                    mesh->hasTangents()
                        ? (enlazarSlotTextura(material->getNormalMapPath(), 3,
                                              Rendering::Contrato::Uniforme::kNormalTex)
                               ? 1
                               : 0)
                        : 0);
}

bool MeshRenderer::intentarRender(Modelos3D* objeto, const float view[16],
                                  const float projection[16],
                                  float deltaTime) {
    if (!objeto) return false;

    // El componente Model (path) manda sobre el path actual. Se toma el
    // puntero a la malla DESPUES de este paso, porque setPath() puede
    // reemplazar la malla compartida (y el AssetManager eviccionar la
    // anterior).
    if (Model* model = objeto->getComponent<Model>();
        model && model->getPath() != objeto->getPath())
        objeto->setPath(model->getPath());

    Transform* transform = objeto->getGlobalTransform();
    if (!transform) return false;

    float modelo[16];
    buildMatrixFromTransform(transform, modelo);
    return intentarRenderMesh(objeto, objeto->getMesh(), modelo, view,
                              projection, deltaTime);
}

bool MeshRenderer::intentarRenderMesh(GameObject* contexto, const Mesh* mesh,
                                      const float modelo[16],
                                      const float view[16],
                                      const float projection[16],
                                      float deltaTime) {
    if (!inicializar()) return false;

    // El render NO simula: los scripts y la fisica se actualizan una sola vez
    // por frame en GameScene::update (y solo en play). Antes se llamaba aca a
    // objeto->update(), lo que corria los scripts tambien en el editor, los
    // duplicaba en play y reactivaba el loop al detener el play.
    (void)deltaTime;

    if (!contexto) return false;

    // Sin malla, vacia o sin normales no hay nada que pintar. Ya no existe el
    // modo inmediato de respaldo, asi que se avisa una vez por malla (el loader
    // rellena las normales que faltan, de modo que esto solo deberia tocar a
    // mallas construidas a mano).
    if (!mesh || mesh->isEmpty() || !mesh->hasNormals()) {
        if (mesh && mallasSinNormales_.insert(mesh).second) {
            std::cerr << "[MeshRenderer] Malla '" << mesh->name
                      << "' sin normales por vertice: no se dibuja. Use "
                         "Mesh::computeNormals() para generarlas."
                      << '\n';
        }
        return false;
    }

    const glm::mat4 model = glm::make_mat4(modelo);

    // Deja la matriz normal como transpose(inverse(model)) pero identidad si la
    // inversa degenero (fracciones no finitas); evita NaN en el shader.
    glm::mat3 normalMatrix =
        glm::mat3(glm::transpose(glm::inverse(model)));
    if (!matrizFinita(normalMatrix)) normalMatrix = glm::mat3(1.0f);

    shader_->use();
    shader_->setMat4(Rendering::Contrato::Uniforme::kModel, model);
    shader_->setMat4(Rendering::Contrato::Uniforme::kView, glm::make_mat4(view));
    shader_->setMat4(Rendering::Contrato::Uniforme::kProjection, glm::make_mat4(projection));
    shader_->setMat3(Rendering::Contrato::Uniforme::kNormalMatrix, normalMatrix);

    const glm::mat4 invView = glm::inverse(glm::make_mat4(view));
    shader_->setVec3(Rendering::Contrato::Uniforme::kCameraPosition,
                     glm::vec3(invView[3][0], invView[3][1], invView[3][2]));
    shader_->setFloat(Rendering::Contrato::Uniforme::kDistanceFadeStart, horizonteInicio_);
    shader_->setFloat(Rendering::Contrato::Uniforme::kDistanceFadeEnd, horizonteFin_);

    aplicarMaterial(contexto);
    aplicarTexturas(contexto, mesh);

    auto it = gpu_.find(mesh);
    if (it == gpu_.end()) {
        auto gpu = std::make_unique<MeshGPU>();
        gpu->upload(&mesh->vertices[0].x, mesh->vertices.size(),
                    mesh->hasNormals() ? &mesh->normals[0].x : nullptr,
                    mesh->normals.size(),
                    mesh->hasUvs() ? &mesh->uvs[0].x : nullptr,
                    mesh->uvs.size(),
                    mesh->hasTangents() ? &mesh->tangents[0].x : nullptr,
                    mesh->tangents.size(),
                    mesh->hasTangents() ? &mesh->bitangents[0].x : nullptr,
                    mesh->bitangents.size(),
                    mesh->indices.empty() ? nullptr : &mesh->indices[0],
                    mesh->indices.size());
        it = gpu_.emplace(mesh, std::move(gpu)).first;
    }

    // El relleno de poligonos es el unico estado valido en un contexto core
    // (no hay glPolygonMode) y el depth test ya lo activa applyBaseState().
    auto& backend = Rendering::Backend::activeBackend();
    backend.setBlendEnabled(true);
    it->second->draw();
    backend.setBlendEnabled(false);
    ShaderProgram::unbind();
    return true;
}
