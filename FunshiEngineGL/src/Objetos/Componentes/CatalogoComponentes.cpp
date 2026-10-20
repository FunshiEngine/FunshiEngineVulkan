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
#include "CatalogoComponentes.h"

#include "Animador2D.h"
#include "AudioSource.h"
#include "CameraComponent.h"
#include "Color.h"
#include "Colliders/CubeCollider.h"
#include "Colliders/EsfereCollider.h"
#include "Colliders/MallaCollider.h"
#include "Colliders/TilemapCollider.h"
#include "Grid.h"
#include "InterfaceComponent.h"
#include "Light.h"
#include "Material.h"
#include "Model.h"
#include "RigidBody/RigidBody.h"
#include "Script.h"
#include "Skybox.h"
#include "SpriteRenderer.h"
#include "Tilemap.h"
#include "Transform.h"
#include "../GameObject.h"

#include "../../GUI/ObjetosGUI/SettingsComponent.h"
#include "../../GUI/ObjetosGUI/Animador2D/SettingsAnimador2D.h"
#include "../../GUI/ObjetosGUI/Audio/SettingsAudioSource.h"
#include "../../GUI/ObjetosGUI/Camera/SettingsCamera.h"
#include "../../GUI/ObjetosGUI/Color/SettingsColor.h"
#include "../../GUI/ObjetosGUI/Colliders/SettingsColliderCubo.h"
#include "../../GUI/ObjetosGUI/Colliders/SettingsColliderEsfera.h"
#include "../../GUI/ObjetosGUI/Colliders/SettingsColliderMalla.h"
#include "../../GUI/ObjetosGUI/Colliders/SettingsColliderTilemap.h"
#include "../../GUI/ObjetosGUI/Grid/SettingsGrid.h"
#include "../../GUI/ObjetosGUI/Interface/SettingsInterface.h"
#include "../../GUI/ObjetosGUI/Light/SettingsLight.h"
#include "../../GUI/ObjetosGUI/Material/SettingsMaterial.h"
#include "../../GUI/ObjetosGUI/Model/SettingsModel.h"
#include "../../GUI/ObjetosGUI/RigidBody/SettingsRigidBody.h"
#include "../../GUI/ObjetosGUI/Script/SettingsScript.h"
#include "../../GUI/ObjetosGUI/Skybox/SettingsSkybox.h"
#include "../../GUI/ObjetosGUI/Sprite/SettingsSpriteRenderer.h"
#include "../../GUI/ObjetosGUI/Tilemap/SettingsTilemap.h"
#include "../../GUI/ObjetosGUI/Transform/SettingsTransform.h"

namespace {

// Tolera nombres de componente ya persistidos por builds viejos de Windows:
// MSVC no demanglea y typeid().name() guardaba "class Transform"/"struct X".
std::string normalizarNombre(const std::string& typeName) {
    constexpr const char* prefijos[] = {"class ", "struct ", "union ", "enum "};
    for (const char* prefijo : prefijos) {
        const std::size_t len = std::char_traits<char>::length(prefijo);
        if (typeName.rfind(prefijo, 0) == 0)
            return typeName.substr(len);
    }
    return typeName;
}

constexpr const char* kSubmenuColliders = "Add Collider";

} // namespace

CatalogoComponentes::CatalogoComponentes() {
    using Creador = std::function<std::unique_ptr<Component>(GameObject&)>;
    using CreadorPanel = std::function<std::unique_ptr<SettingsComponent>(
        Component*, GameObject*, const ContextoPanel&)>;

    auto add = [this](std::string nombre, std::string etiqueta,
                      DimensionesComponente dim, std::string submenu,
                      Creador crear, CreadorPanel panel,
                      std::function<bool(GameObject&)> prereq = nullptr,
                      std::string alias = std::string()) {
        DescriptorComponente d;
        d.nombre = std::move(nombre);
        d.etiqueta = std::move(etiqueta);
        d.alias = std::move(alias);
        d.dimensiones = dim;
        d.submenu = std::move(submenu);
        d.prerequisito = std::move(prereq);
        d.crear = std::move(crear);
        d.crearPanel = std::move(panel);
        items_.push_back(std::move(d));
    };

    // Nucleo: presentes en cualquier dimension.
    add("Transform", "Transform", DimensionesComponente::Ambas, "",
        [](GameObject&) { return std::unique_ptr<Component>(new Transform()); },
        [](Component* c, GameObject* o, const ContextoPanel& ctx) {
            auto* s = new SettingsTransform(dynamic_cast<Transform*>(c), o);
            s->setDimension(ctx.dimension);
            return std::unique_ptr<SettingsComponent>(s);
        });
    add("Color", "Color", DimensionesComponente::Ambas, "",
        [](GameObject&) { return std::unique_ptr<Component>(new Color()); },
        [](Component*, GameObject* o, const ContextoPanel&) {
            return std::unique_ptr<SettingsComponent>(new SettingsColor(o));
        });
    add("CameraComponent", "CameraComponent", DimensionesComponente::Ambas, "",
        [](GameObject&) {
            return std::unique_ptr<Component>(new CameraComponent());
        },
        [](Component*, GameObject* o, const ContextoPanel& ctx) {
            auto* s = new SettingsCamera(o);
            s->setDimension(ctx.dimension);
            return std::unique_ptr<SettingsComponent>(s);
        },
        nullptr, "Camera");

    // Render 3D: solo tienen sentido con profundidad.
    add("Material", "Material", DimensionesComponente::TresD, "",
        [](GameObject&) { return std::unique_ptr<Component>(new Material()); },
        [](Component*, GameObject* o, const ContextoPanel&) {
            return std::unique_ptr<SettingsComponent>(new SettingsMaterial(o));
        });
    add("Light", "Light", DimensionesComponente::TresD, "",
        [](GameObject&) { return std::unique_ptr<Component>(new Light()); },
        [](Component*, GameObject* o, const ContextoPanel&) {
            return std::unique_ptr<SettingsComponent>(new SettingsLight(o));
        });
    add("Model", "Model", DimensionesComponente::TresD, "",
        [](GameObject&) { return std::unique_ptr<Component>(new Model()); },
        [](Component*, GameObject* o, const ContextoPanel&) {
            return std::unique_ptr<SettingsComponent>(new SettingsModel(o));
        });
    add("Grid", "Grid", DimensionesComponente::TresD, "",
        [](GameObject&) { return std::unique_ptr<Component>(new Grid()); },
        [](Component*, GameObject* o, const ContextoPanel&) {
            return std::unique_ptr<SettingsComponent>(new SettingsGrid(o));
        });
    add("Skybox", "Skybox", DimensionesComponente::TresD, "",
        [](GameObject&) { return std::unique_ptr<Component>(new Skybox()); },
        [](Component*, GameObject* o, const ContextoPanel&) {
            return std::unique_ptr<SettingsComponent>(new SettingsSkybox(o));
        });

    // Render 2D: el sprite se puede usar en ambas; el animador es propio del
    // plano.
    add("SpriteRenderer", "SpriteRenderer", DimensionesComponente::Ambas, "",
        [](GameObject&) {
            return std::unique_ptr<Component>(new SpriteRenderer());
        },
        [](Component*, GameObject* o, const ContextoPanel&) {
            return std::unique_ptr<SettingsComponent>(
                new SettingsSpriteRenderer(o));
        });
    add("Animador2D", "Animador2D", DimensionesComponente::DosD, "",
        [](GameObject& o) -> std::unique_ptr<Component> {
            if (!o.getComponent<SpriteRenderer>()) return nullptr;
            return std::unique_ptr<Component>(new Animador2D());
        },
        [](Component*, GameObject* o, const ContextoPanel&) {
            return std::unique_ptr<SettingsComponent>(
                new SettingsAnimador2D(o));
        },
        [](GameObject& o) {
            return o.getComponent<SpriteRenderer>() != nullptr;
        });
    add("Tilemap", "Tilemap", DimensionesComponente::DosD, "",
        [](GameObject&) { return std::unique_ptr<Component>(new Tilemap()); },
        [](Component*, GameObject* o, const ContextoPanel&) {
            return std::unique_ptr<SettingsComponent>(new SettingsTilemap(o));
        });

    // Colision: requieren Transform y en el menu se agrupan en un submenu.
    auto exigeTransform = [](GameObject& o) {
        return o.getComponent<Transform>() != nullptr;
    };
    add("EsfereCollider", "EsfereCollider", DimensionesComponente::Ambas,
        kSubmenuColliders,
        [](GameObject& o) -> std::unique_ptr<Component> {
            Transform* t = o.getComponent<Transform>();
            if (!t) return nullptr;
            return std::unique_ptr<Component>(new EsfereCollider(5.0f, t, &o));
        },
        [](Component*, GameObject* o, const ContextoPanel& ctx) {
            auto* s = new SettingsColliderEsfera(o);
            s->setEditor(ctx.editor);
            return std::unique_ptr<SettingsComponent>(s);
        },
        exigeTransform);
    add("CubeCollider", "CubeCollider", DimensionesComponente::Ambas,
        kSubmenuColliders,
        [](GameObject& o) -> std::unique_ptr<Component> {
            Transform* t = o.getComponent<Transform>();
            if (!t) return nullptr;
            return std::unique_ptr<Component>(new CubeCollider(5.0f, t, &o));
        },
        [](Component*, GameObject* o, const ContextoPanel& ctx) {
            auto* s = new SettingsColliderCubo(o);
            s->setEditor(ctx.editor);
            return std::unique_ptr<SettingsComponent>(s);
        },
        exigeTransform);
    add("MallaCollider", "MallaCollider", DimensionesComponente::TresD,
        kSubmenuColliders,
        [](GameObject& o) -> std::unique_ptr<Component> {
            Transform* t = o.getComponent<Transform>();
            if (!t) return nullptr;
            return std::unique_ptr<Component>(new MallaCollider(5.0f, t, &o));
        },
        [](Component*, GameObject* o, const ContextoPanel& ctx) {
            auto* s = new SettingsColliderMalla(o);
            s->setEditor(ctx.editor);
            return std::unique_ptr<SettingsComponent>(s);
        },
        exigeTransform);
    add("TilemapCollider", "TilemapCollider", DimensionesComponente::DosD,
        kSubmenuColliders,
        [](GameObject& o) -> std::unique_ptr<Component> {
            Transform* t = o.getComponent<Transform>();
            if (!t) return nullptr;
            return std::unique_ptr<Component>(new TilemapCollider(5.0f, t, &o));
        },
        [](Component*, GameObject* o, const ContextoPanel& ctx) {
            auto* s = new SettingsColliderTilemap(o);
            s->setEditor(ctx.editor);
            return std::unique_ptr<SettingsComponent>(s);
        },
        [](GameObject& o) {
            return o.getComponent<Transform>() != nullptr &&
                   o.getComponent<Tilemap>() != nullptr;
        });
    add("RigidBody", "RigidBody", DimensionesComponente::Ambas, "",
        [](GameObject& o) -> std::unique_ptr<Component> {
            Collider* col = o.getComponent<Collider>();
            if (!col) return nullptr;
            return std::unique_ptr<Component>(new RigidBody(col, 1.0f));
        },
        [](Component*, GameObject* o, const ContextoPanel& ctx) {
            auto* s = new SettingsRigidBody(o);
            s->setEditor(ctx.editor);
            return std::unique_ptr<SettingsComponent>(s);
        },
        [](GameObject& o) { return o.getComponent<Collider>() != nullptr; });

    // Script, audio e interfaz: disponibles en cualquier dimension.
    add("Script", "Script", DimensionesComponente::Ambas, "",
        [](GameObject&) { return std::unique_ptr<Component>(new Script()); },
        [](Component* c, GameObject*, const ContextoPanel&) {
            return std::unique_ptr<SettingsComponent>(
                new SettingsScript(dynamic_cast<Script*>(c)));
        });
    add("AudioSource", "AudioSource", DimensionesComponente::Ambas, "",
        [](GameObject&) {
            return std::unique_ptr<Component>(new AudioSource());
        },
        [](Component*, GameObject* o, const ContextoPanel& ctx) {
            auto* s = new SettingsAudioSource(o);
            s->setAudioEngine(ctx.audio);
            return std::unique_ptr<SettingsComponent>(s);
        });
    add("InterfaceComponent", "Interfaz de juego (HTML)",
        DimensionesComponente::Ambas, "",
        [](GameObject&) {
            return std::unique_ptr<Component>(new InterfaceComponent());
        },
        [](Component*, GameObject* o, const ContextoPanel&) {
            return std::unique_ptr<SettingsComponent>(new SettingsInterface(o));
        });
}

const CatalogoComponentes& CatalogoComponentes::instancia() {
    static const CatalogoComponentes catalogo;
    return catalogo;
}

const DescriptorComponente* CatalogoComponentes::buscar(
    const std::string& nombre) const {
    const std::string normalizado = normalizarNombre(nombre);
    for (const DescriptorComponente& d : items_) {
        if (d.nombre == normalizado ||
            (!d.alias.empty() && d.alias == normalizado))
            return &d;
    }
    return nullptr;
}
