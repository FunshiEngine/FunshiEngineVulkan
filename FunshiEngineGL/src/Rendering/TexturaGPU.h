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
#ifndef TEXTURAGPU_H
#define TEXTURAGPU_H

#include "../Assets/Image.h"
#include "Backend/IRenderBackend.h"

// Textura 2D en GPU: posee un handle opaco del backend con RAII y formato
// estandarizado RGBA8 + LINEAR/REPEAT. Es la contraparte GPU de una Image CPU
// compartida; el renderer la cachea por identidad de la imagen (al igual que
// MeshGPU se cachea por identidad de Mesh) para no re-subir pixels redundantes
// en cada frame.
class TexturaGPU {
private:
    Rendering::Backend::Handle texture_ = Rendering::Backend::kInvalidHandle;
    int width_ = 0;
    int height_ = 0;

    void destroy();

public:
    TexturaGPU() = default;
    ~TexturaGPU();
    TexturaGPU(const TexturaGPU&) = delete;
    TexturaGPU& operator=(const TexturaGPU&) = delete;
    TexturaGPU(TexturaGPU&& other) noexcept;
    TexturaGPU& operator=(TexturaGPU&& other) noexcept;

    // Sube la imagen CPU a GPU (reemplaza cualquier textura previa).
    void upload(const Image& image);

    bool isUploaded() const { return texture_ != Rendering::Backend::kInvalidHandle; }
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }

    // Activa la unidad de texturizado 'unit' y la deja bindeada como objetivo
    // de sampleo.
    void bindUnit(int unit) const;
};

#endif