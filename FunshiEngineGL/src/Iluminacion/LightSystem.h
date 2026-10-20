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
#ifndef LIGHTSYSTEM_H
#define LIGHTSYSTEM_H

// Subsistema de iluminacion (solo CPU, sin estado grafico): cada frame escanea
// los GameObjects de la escena, toma sus componentes Light y exporta los datos
// para que el renderer de la escena los suba como uniforms del shader. No
// queda estado de luz en la API grafica: la iluminacion viaja por el shader, y
// aca no se toca ninguna funcion de la API. Light es solo data.
class GameObject;

template <typename T>
class Position;

template <typename T>
class ListaDE;

// Descripcion CPU de una luz del escenario, lista para uniforms del shader.
// Es la contraparte de aplicarLightComponent: position/direction se derivan
// del Transform del objeto dueño, no de ningun estado de la API grafica.
struct LightData {
    int type = 0;              // 0 direccional, 1 punto, 2 spot (LightType)
    float worldPos[3] = {0.f, 0.f, 0.f};   // dir para direccional, pos punto/spot
    float direction[3] = {0.f, 0.f, -1.f}; // forward del objeto (spot/dir)
    float ambient[3] = {0.f, 0.f, 0.f};
    float diffuse[3] = {1.f, 1.f, 1.f};
    float specular[3] = {1.f, 1.f, 1.f};
    float constant = 1.f;
    float linear = 0.f;
    float quadratic = 0.f;
    float spotCutoffDegrees = 45.f;
};

class LightSystem {
public:
    // Cantidad de luces que el motor envia al shader en un frame. El backend
    // tiene que reservar esta cantidad en su bloque de uniformes.
    static constexpr int kMaxLights = 8;

    LightSystem();
    ~LightSystem() = default;

    // Luz global de la escena, default gris tenue.
    void setGlobalAmbient(float r, float g, float b);

    const float* getGlobalAmbient() const noexcept { return globalAmbient_; }

    // Version CPU de la pasada de luces: llena luces[] (hasta kMaxLights) con
    // la semantica que el shader espera (escaneo del arbol, posicion =
    // Transform, direccional usa la rotacion del forward). outCount queda con
    // la cantidad de luces recogidas.
    void collectLights(ListaDE<GameObject*>* objects, LightData luces[],
                       int& outCount) const;

private:
    float globalAmbient_[4] = {0.15f, 0.15f, 0.15f, 1.f};
};

#endif