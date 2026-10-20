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
#include "BackendNulo.h"

namespace Rendering {
namespace Backend {

bool BackendNulo::init(void* ventanaNativa) {
    // El motor arranca igual sin API grafica: no hay contexto que crear ni
    // funciones que cargar, asi que la inicializacion es satisfactoria. La
    // ventana no se usa: sin API grafica no hay superficie que crear.
    (void)ventanaNativa;
    return true;
}

void BackendNulo::beginFrame() {
    // Sin API grafica no hay frame que abrir.
}

void BackendNulo::endFrame() {
    // Sin API grafica no hay nada que enviar ni que mostrar.
}

bool BackendNulo::available() const {
    // No hay pipeline: los renderers consultan esto antes de dibujar y omiten
    // la pasada en vez de emitir draws sin recursos.
    return false;
}

Handle BackendNulo::createMesh(const MeshData& data) {
    (void)data;
    return kInvalidHandle;
}

void BackendNulo::destroyMesh(Handle mesh) {
    (void)mesh;
}

void BackendNulo::drawMesh(Handle mesh, unsigned int indexCount) {
    (void)mesh;
    (void)indexCount;
}

Handle BackendNulo::createLineBatch(const float* vertices,
                                    std::size_t vertexCount) {
    (void)vertices;
    (void)vertexCount;
    return kInvalidHandle;
}

void BackendNulo::destroyLineBatch(Handle batch) {
    (void)batch;
}

void BackendNulo::updateLineBatch(Handle batch, const float* vertices,
                                  std::size_t vertexCount) {
    (void)batch;
    (void)vertices;
    (void)vertexCount;
}

void BackendNulo::drawLineBatch(Handle batch, unsigned int vertexCount) {
    (void)batch;
    (void)vertexCount;
}

Handle BackendNulo::createTexture2D(const Image2D& image) {
    (void)image;
    return kInvalidHandle;
}

void BackendNulo::destroyTexture2D(Handle texture) {
    (void)texture;
}

void BackendNulo::bindTexture2D(Handle texture, int unit) {
    (void)texture;
    (void)unit;
}

void BackendNulo::updateTexture2D(Handle texture, const Image2D& image) {
    (void)texture;
    (void)image;
}

Handle BackendNulo::createTextureCube(const ImageCube& image) {
    (void)image;
    return kInvalidHandle;
}

void BackendNulo::destroyTextureCube(Handle texture) {
    (void)texture;
}

void BackendNulo::bindTextureCube(Handle texture, int unit) {
    (void)texture;
    (void)unit;
}

Handle BackendNulo::createProgram(Contrato::ShaderKind kind) {
    (void)kind;
    return kInvalidHandle;
}

void BackendNulo::destroyProgram(Handle program) {
    (void)program;
}

void BackendNulo::useProgram(Handle program) {
    (void)program;
}

int BackendNulo::uniformLocation(Handle program, const char* name) {
    (void)program;
    (void)name;
    return -1;
}

void BackendNulo::setUniformMat4(int location, const glm::mat4& value) {
    (void)location;
    (void)value;
}

void BackendNulo::setUniformMat3(int location, const glm::mat3& value) {
    (void)location;
    (void)value;
}

void BackendNulo::setUniformVec3(int location, const glm::vec3& value) {
    (void)location;
    (void)value;
}

void BackendNulo::setUniformVec4(int location, const float* value) {
    (void)location;
    (void)value;
}

void BackendNulo::setUniformFloat(int location, float value) {
    (void)location;
    (void)value;
}

void BackendNulo::setUniformInt(int location, int value) {
    (void)location;
    (void)value;
}

Handle BackendNulo::createRenderTarget(int width, int height) {
    (void)width;
    (void)height;
    return kInvalidHandle;
}

void BackendNulo::destroyRenderTarget(Handle target) {
    (void)target;
}

void BackendNulo::bindRenderTarget(Handle target) {
    (void)target;
}

void BackendNulo::bindDefaultFramebuffer() {
}

Handle BackendNulo::renderTargetColorTexture(Handle target) const {
    (void)target;
    return kInvalidHandle;
}

void* BackendNulo::imguiTextureId(Handle texture) const {
    (void)texture;
    return nullptr;
}

void BackendNulo::setViewport(int x, int y, int width, int height) {
    (void)x;
    (void)y;
    (void)width;
    (void)height;
}

void BackendNulo::clearScreen(const float color[3]) {
    (void)color;
}

void BackendNulo::applyBaseState() {
}

void BackendNulo::setClearColor(const float color[3]) {
    (void)color;
}

const char* BackendNulo::diagnosticoGPU() const {
    return "[Render] Sin API grafica implementada: el motor corre sin dibujar.";
}

void BackendNulo::setBlendEnabled(bool enabled) {
    (void)enabled;
}

void BackendNulo::setDepthTestEnabled(bool enabled) {
    (void)enabled;
}

void BackendNulo::setDepthMask(bool enabled) {
    (void)enabled;
}

void BackendNulo::setDepthFunc(DepthFunc func) {
    (void)func;
}

void BackendNulo::drawFullscreenTriangle() {
}

} // namespace Backend
} // namespace Rendering
