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
#ifndef SETTINGSSPRITERENDERER_H
#define SETTINGSSPRITERENDERER_H
#include "../SettingsComponent.h"

class GameObject;

// Panel del componente SpriteRenderer en el inspector: ruta de textura (con
// drag & drop de archivos de imagen), tamano en unidades de mundo, tinte RGBA y
// region UV. El estado vive en el componente; el panel solo lo edita.
class SettingsSpriteRenderer : public SettingsComponent {
protected:
    GameObject* gameObject_ = nullptr;
public:
    explicit SettingsSpriteRenderer(GameObject* objeto);
    void showDataComponent() override;
    Component* getComponent() override;
};
#endif
