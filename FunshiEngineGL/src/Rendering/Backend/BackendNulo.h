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
#ifndef BACKENDNULO_H
#define BACKENDNULO_H

#include "IRenderBackend.h"

namespace Rendering {
namespace Backend {

// Implementacion en blanco del contrato de backend grafico.
//
// El motor define que necesita y como se llama cada recurso, pero no sabe
// como se dibuja: eso es lo que este repositorio deja por implementar. Este
// backend cumple el contrato sin dibujar nada, de modo que el motor arranca,
// la escena se recorre y la suite de pruebas corre sin que haya una API
// grafica instalada.
//
// No es un stub que devuelve cero sin mas: init() devuelve true (el motor
// arranca), available() devuelve false (no hay nada que dibujar) y cada
// creacion devuelve el handle invalido, de modo que los renderers detectan
// que no hay pipeline y omiten la pasada en vez de dibujar con recursos
// inexistentes.
class BackendNulo : public IRenderBackend {
public:
    bool init(void* ventanaNativa) override;
    bool available() const override;

    // Limite de frame: sin API grafica no hay nada que abrir ni que cerrar.
    void beginFrame() override;
    void endFrame() override;

    // Malla GPU.
    Handle createMesh(const MeshData& data) override;
    void destroyMesh(Handle mesh) override;
    void drawMesh(Handle mesh, unsigned int indexCount) override;

    // Batch de lineas.
    Handle createLineBatch(const float* vertices,
                           std::size_t vertexCount) override;
    void destroyLineBatch(Handle batch) override;
    void updateLineBatch(Handle batch, const float* vertices,
                         std::size_t vertexCount) override;
    void drawLineBatch(Handle batch, unsigned int vertexCount) override;

    // Texturas.
    Handle createTexture2D(const Image2D& image) override;
    void destroyTexture2D(Handle texture) override;
    void bindTexture2D(Handle texture, int unit) override;
    void updateTexture2D(Handle texture, const Image2D& image) override;
    Handle createTextureCube(const ImageCube& image) override;
    void destroyTextureCube(Handle texture) override;
    void bindTextureCube(Handle texture, int unit) override;

    // Programas de shaders.
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

    // Render targets.
    Handle createRenderTarget(int width, int height) override;
    void destroyRenderTarget(Handle target) override;
    void bindRenderTarget(Handle target) override;
    void bindDefaultFramebuffer() override;
    Handle renderTargetColorTexture(Handle target) const override;
    void* imguiTextureId(Handle texture) const override;

    // Estado de la pasada.
    void setViewport(int x, int y, int width, int height) override;
    void clearScreen(const float color[3]) override;
    void applyBaseState() override;
    void setClearColor(const float color[3]) override;
    const char* diagnosticoGPU() const override;
    void setBlendEnabled(bool enabled) override;
    void setDepthTestEnabled(bool enabled) override;
    void setDepthMask(bool enabled) override;
    void setDepthFunc(DepthFunc func) override;
    void drawFullscreenTriangle() override;
};

} // namespace Backend
} // namespace Rendering

#endif
