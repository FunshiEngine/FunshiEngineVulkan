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
#ifndef SHADERPROGRAM_H
#define SHADERPROGRAM_H

#include <glm/glm.hpp>

#include <string>
#include <unordered_map>

#include "../Backend/IRenderBackend.h"

// Programa de shaders RAII: lo pide al backend activo y libera el handle en
// el destructor. La factory create() lanza ShaderCompileException /
// ShaderLinkException / ShaderUnavailableException (se propagan desde el
// backend) si el pipeline no esta disponible o el shader falla. Los setters de
// uniforms cachean el localizador contra el backend (la busqueda es costosa y
// los shaders optimizados no exponen los uniforms que no usan, que se ignoran
// sin ruido).
class ShaderProgram {
private:
    Rendering::Backend::Handle program_ = Rendering::Backend::kInvalidHandle;
    mutable std::unordered_map<std::string, int> uniformLocations_;

    int findUniform(const char* name) const;

    ShaderProgram() = default;

public:
    ~ShaderProgram();
    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;
    ShaderProgram(ShaderProgram&& other) noexcept;
    ShaderProgram& operator=(ShaderProgram&& other) noexcept;

    // Pide al backend el programa del tipo indicado. El fuente del shader
    // vive en el backend: cada API grafica tiene su propio dialecto.
    static ShaderProgram create(Rendering::Contrato::ShaderKind kind);

    Rendering::Backend::Handle getProgramId() const { return program_; }
    bool isValid() const {
        return program_ != Rendering::Backend::kInvalidHandle;
    }

    void use() const;
    static void unbind();

    void setMat4(const char* name, const glm::mat4& value);
    void setMat3(const char* name, const glm::mat3& value);
    void setVec3(const char* name, const glm::vec3& value);
    void setVec4(const char* name, const float* value);
    void setFloat(const char* name, float value);
    void setInt(const char* name, int value);
};

#endif