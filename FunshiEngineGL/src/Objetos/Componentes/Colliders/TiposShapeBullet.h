/*
    FunshiEngineGL - Motor de juegos 3D con OpenGL e ImGui
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
#ifndef OBJETOS_TIPOSSHAPEBULLET_H
#define OBJETOS_TIPOSSHAPEBULLET_H
#include <btBulletDynamicsCommon.h>

// Identificacion de las shapes de Bullet SIN RTTI.
//
// Bullet se compila normalmente sin informacion de tipos: el paquete de vcpkg
// instala `bullet3` con `USE_MSVC_DISABLE_RTTI=ON` (la feature `rtti` esta
// invertida a proposito) y el paquete del sistema tambien la omite. Sus
// vtables no llevan entonces puntero a typeinfo, de modo que un
// `dynamic_cast` sobre un objeto de Bullet lee un puntero nulo y el proceso
// aborta con un access violation dentro de `__RTDynamicCast` (VCRUNTIME140).
//
// Bullet trae para eso el "shape type": un entero que cada clase fija en su
// constructor y que `getShapeType()` devuelve. Con el se reconoce la clase
// sin RTTI y el `static_cast` que lo sigue es seguro.
namespace TiposShapeBullet {

// Devuelve la shape como compuesta, o nullptr si no lo es.
inline btCompoundShape* comoCompuesta(btCollisionShape* shape) {
    if (!shape || shape->getShapeType() != COMPOUND_SHAPE_PROXYTYPE) {
        return nullptr;
    }
    return static_cast<btCompoundShape*>(shape);
}

// Devuelve la shape como caja, o nullptr si no lo es.
inline btBoxShape* comoCaja(btCollisionShape* shape) {
    if (!shape || shape->getShapeType() != BOX_SHAPE_PROXYTYPE) {
        return nullptr;
    }
    return static_cast<btBoxShape*>(shape);
}

// Devuelve la shape como casco convexo, o nullptr si no lo es.
inline btConvexHullShape* comoCascoConvexo(btCollisionShape* shape) {
    if (!shape || shape->getShapeType() != CONVEX_HULL_SHAPE_PROXYTYPE) {
        return nullptr;
    }
    return static_cast<btConvexHullShape*>(shape);
}

}  // namespace TiposShapeBullet

#endif
