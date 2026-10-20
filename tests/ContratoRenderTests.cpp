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
// Pruebas headless del contrato de render.
//
// El motor no dibuja: define que necesita (recursos, estados, uniformes) y el
// backend lo resuelve. Este test fija ese contrato:
//
//  1. La tabla de uniformes por tipo de programa es completa y no tiene
//     nombres inventados: sus nombres son exactamente los que declara el
//     namespace de uniformes del contrato.
//  2. Cada tipo de programa expone los uniformes que su semantica exige (el
//     difuminado radial de las lineas, el recorte de uv de los sprites, el
//     modelo de iluminacion de las mallas, el degradado del cielo).
//  3. Los renderers escriben los uniformes con las constantes del contrato,
//     nunca con literales: si un renderer se salta el contrato, el nombre
//     escrito no llega al backend.
//  4. El backend nulo cumple el contrato entero sin dibujar: el motor arranca
//     y omite las pasadas en vez de dibujar con recursos inexistentes.
//  5. El backend se puede instalar y reemplazar, y el motor pide los
//     programas por tipo (no por fuente de shader).
//
// No hay GPU en este test: el backend de mentira solo registra las llamadas,
// asi que las comprobaciones son deterministas y no dependen del driver.

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "../FunshiEngineGL/src/Rendering/Backend/BackendNulo.h"
#include "../FunshiEngineGL/src/Rendering/Backend/IRenderBackend.h"
#include "../FunshiEngineGL/src/Rendering/ContratoRender.h"
#include "../FunshiEngineGL/src/Rendering/Shaders/ShaderProgram.h"

namespace {
int total = 0;
int fallos = 0;

#define CHECK(cond, msg)                                                      \
    do {                                                                      \
        ++total;                                                              \
        if (!(cond)) {                                                        \
            ++fallos;                                                         \
            std::cout << "FALLO: " << msg << " (linea " << __LINE__ << ")"    \
                      << std::endl;                                           \
        }                                                                     \
    } while (0)

namespace Contrato = Rendering::Contrato;
namespace Backend = Rendering::Backend;

using Backend::Handle;
using Backend::IRenderBackend;
using Backend::kInvalidHandle;

// Uniformes del contrato: la constante con la que el motor los nombra y el
// nombre exacto que el backend tiene que exponer.
struct UniformeEsperado {
    const char* constante;
    const char* nombre;
};

const UniformeEsperado kUniformes[] = {
    {"kModel", Contrato::Uniforme::kModel},
    {"kView", Contrato::Uniforme::kView},
    {"kProjection", Contrato::Uniforme::kProjection},
    {"kNormalMatrix", Contrato::Uniforme::kNormalMatrix},
    {"kCameraPosition", Contrato::Uniforme::kCameraPosition},
    {"kDistanceFadeStart", Contrato::Uniforme::kDistanceFadeStart},
    {"kDistanceFadeEnd", Contrato::Uniforme::kDistanceFadeEnd},
    {"kGlobalAmbient", Contrato::Uniforme::kGlobalAmbient},
    {"kLightCount", Contrato::Uniforme::kLightCount},
    {"kLightTypes", Contrato::Uniforme::kLightTypes},
    {"kLightWorldPos", Contrato::Uniforme::kLightWorldPos},
    {"kLightDir", Contrato::Uniforme::kLightDir},
    {"kLightAmbient", Contrato::Uniforme::kLightAmbient},
    {"kLightDiffuse", Contrato::Uniforme::kLightDiffuse},
    {"kLightSpecular", Contrato::Uniforme::kLightSpecular},
    {"kLightConstant", Contrato::Uniforme::kLightConstant},
    {"kLightLinear", Contrato::Uniforme::kLightLinear},
    {"kLightQuadratic", Contrato::Uniforme::kLightQuadratic},
    {"kSpotCutoffCos", Contrato::Uniforme::kSpotCutoffCos},
    {"kMaterialAmbient", Contrato::Uniforme::kMaterialAmbient},
    {"kMaterialDiffuse", Contrato::Uniforme::kMaterialDiffuse},
    {"kMaterialSpecular", Contrato::Uniforme::kMaterialSpecular},
    {"kMaterialEmission", Contrato::Uniforme::kMaterialEmission},
    {"kMaterialShininess", Contrato::Uniforme::kMaterialShininess},
    {"kDiffuseTex", Contrato::Uniforme::kDiffuseTex},
    {"kSpecularTex", Contrato::Uniforme::kSpecularTex},
    {"kEmissionTex", Contrato::Uniforme::kEmissionTex},
    {"kNormalTex", Contrato::Uniforme::kNormalTex},
    {"kUseTexture", Contrato::Uniforme::kUseTexture},
    {"kUseSpecularMap", Contrato::Uniforme::kUseSpecularMap},
    {"kUseEmissionMap", Contrato::Uniforme::kUseEmissionMap},
    {"kUseNormalMap", Contrato::Uniforme::kUseNormalMap},
    {"kViewport", Contrato::Uniforme::kViewport},
    {"kWidth", Contrato::Uniforme::kWidth},
    {"kNear", Contrato::Uniforme::kNear},
    {"kUvRect", Contrato::Uniforme::kUvRect},
    {"kTexture", Contrato::Uniforme::kTexture},
    {"kTint", Contrato::Uniforme::kTint},
    {"kColorTop", Contrato::Uniforme::kColorTop},
    {"kColorBottom", Contrato::Uniforme::kColorBottom},
    {"kInvViewProj", Contrato::Uniforme::kInvViewProj},
    {"kCamPos", Contrato::Uniforme::kCamPos},
    {"kTransicion", Contrato::Uniforme::kTransicion},
    {"kSkybox", Contrato::Uniforme::kSkybox},
};

// Nombre canonico de cada constante del contrato.
std::string nombreDeConstante(const std::string& constante) {
    for (const UniformeEsperado& u : kUniformes) {
        if (constante == u.constante) return u.nombre;
    }
    return std::string();
}

// Nombre canonico de un uniform del contrato: si no existe, la cadena vacia.
std::string nombreCanonico(const std::string& nombre) {
    for (const UniformeEsperado& u : kUniformes) {
        if (nombre == u.nombre) return u.nombre;
    }
    return std::string();
}

// Nombres que expone el tipo de programa indicado.
std::set<std::string> nombresDe(Contrato::ShaderKind kind) {
    std::set<std::string> resultado;
    int cantidad = 0;
    const Contrato::UniformeRequerido* lista =
        Contrato::uniformesDe(kind, cantidad);
    for (int i = 0; i < cantidad; ++i) {
        resultado.insert(lista[i].nombre);
    }
    return resultado;
}

// Todos los nombres canonicos del contrato.
std::set<std::string> nombresDelContrato() {
    std::set<std::string> resultado;
    for (const UniformeEsperado& u : kUniformes) resultado.insert(u.nombre);
    return resultado;
}

// Tipo de un uniform del tipo de programa indicado (o null si no lo expone).
const Contrato::TipoUniforme* tipoDe(Contrato::ShaderKind kind,
                                     const std::string& nombre) {
    int cantidad = 0;
    const Contrato::UniformeRequerido* lista =
        Contrato::uniformesDe(kind, cantidad);
    for (int i = 0; i < cantidad; ++i) {
        if (nombre == lista[i].nombre) return &lista[i].tipo;
    }
    return nullptr;
}

// Cantidad de elementos que el tipo de programa reserva para un uniform.
int elementosDe(Contrato::ShaderKind kind, const std::string& nombre) {
    int cantidad = 0;
    const Contrato::UniformeRequerido* lista =
        Contrato::uniformesDe(kind, cantidad);
    for (int i = 0; i < cantidad; ++i) {
        if (nombre == lista[i].nombre) return lista[i].elementos;
    }
    return 0;
}

// --- 1. La tabla de uniformes es completa y sin nombres inventados ---------

void testTablaDeUniformesCompleta() {
    const Contrato::ShaderKind tipos[] = {
        Contrato::ShaderKind::Malla,   Contrato::ShaderKind::Lineas,
        Contrato::ShaderKind::Sprites, Contrato::ShaderKind::Cielo,
        Contrato::ShaderKind::Cubemap};

    std::set<std::string> declarados;
    for (const Contrato::ShaderKind kind : tipos) {
        int cantidad = 0;
        const Contrato::UniformeRequerido* lista =
            Contrato::uniformesDe(kind, cantidad);
        CHECK(lista != nullptr, "todo tipo de programa declara sus uniforms");
        CHECK(cantidad > 0, "ningun tipo de programa queda sin uniforms");

        std::set<std::string> delTipo;
        for (int i = 0; i < cantidad; ++i) {
            const std::string nombre = lista[i].nombre;
            CHECK(!nombre.empty(), "ningun uniform tiene el nombre vacio");
            CHECK(nombre.find(' ') == std::string::npos,
                  "ningun uniform tiene espacios en el nombre");
            CHECK(delTipo.insert(nombre).second,
                  "ningun tipo de programa declara un uniform repetido");
            CHECK(nombreCanonico(nombre) == nombre,
                  "el uniform declarado existe en el namespace del contrato");
            CHECK(lista[i].elementos >= 1,
                  "todo uniform reserva al menos un elemento");
            declarados.insert(nombre);
        }
    }

    // Todo uniform del namespace tiene que estar en alguna tabla: si se
    // agrega uno nuevo al contrato y no se declara en ningun tipo, el backend
    // no tendria donde escribirlo.
    for (const UniformeEsperado& u : kUniformes) {
        CHECK(declarados.count(u.nombre) == 1,
              "todo uniform del namespace esta declarado en algun tipo");
    }
    CHECK(declarados.size() ==
              sizeof(kUniformes) / sizeof(kUniformes[0]),
          "las tablas no declaran uniforms que no esten en el namespace");
}

// --- 2. La semantica de cada tipo de programa esta declarada --------------

void testSemanticaPorTipoDePrograma() {
    // Malla: transformaciones, camara, modelo de iluminacion y material.
    for (const char* nombre : {"uModel", "uView", "uProjection",
                               "uNormalMatrix", "uCameraPosition",
                               "uGlobalAmbient", "uLightCount",
                               "uMaterialDiffuse", "uDiffuseTex",
                               "uUseTexture"}) {
        CHECK(nombresDe(Contrato::ShaderKind::Malla).count(nombre) == 1,
              "el programa de malla expone el uniform de transformaciones");
    }
    // Los arrays de luces reservan el maximo de luces del contrato.
    for (const char* nombre : {"uLightTypes", "uLightWorldPos", "uLightDir",
                               "uLightAmbient", "uLightDiffuse",
                               "uLightSpecular", "uLightConstant",
                               "uLightLinear", "uLightQuadratic",
                               "uSpotCutoffCos"}) {
        CHECK(elementosDe(Contrato::ShaderKind::Malla, nombre) ==
                  Contrato::kMaxLuces,
              "los arrays de luces reservan el maximo de luces del contrato");
    }

    // Lineas: ancho en pixeles y difuminado radial por distancia.
    for (const char* nombre : {"uViewport", "uWidth", "uNear",
                               "uCameraPosition", "uDistanceFadeStart",
                               "uDistanceFadeEnd"}) {
        CHECK(nombresDe(Contrato::ShaderKind::Lineas).count(nombre) == 1,
              "el programa de lineas expone el uniform del ancho y del "
              "difuminado radial");
    }

    // Sprites: recorte de uv, tinte y sampler.
    for (const char* nombre : {"uUvRect", "uTint", "uTexture", "uUseTexture"}) {
        CHECK(nombresDe(Contrato::ShaderKind::Sprites).count(nombre) == 1,
              "el programa de sprites expone el recorte, el tinte y el sampler");
    }

    // Cielo: degradado, view-projection inversa y transicion.
    for (const char* nombre : {"uColorTop", "uColorBottom", "uInvViewProj",
                               "uCamPos", "uTransicion"}) {
        CHECK(nombresDe(Contrato::ShaderKind::Cielo).count(nombre) == 1,
              "el programa de cielo expone el degradado y la transicion");
    }

    // Cubemap: sampler del cubemap con la vista y la proyeccion.
    for (const char* nombre : {"uSkybox", "uView", "uProjection"}) {
        CHECK(nombresDe(Contrato::ShaderKind::Cubemap).count(nombre) == 1,
              "el programa del cubemap expone el sampler y las matrices");
    }

    // Tipos: el backend decide donde ubica el uniform, pero el motor tiene que
    // saber que tipo escribe.
    const Contrato::TipoUniforme* modelo =
        tipoDe(Contrato::ShaderKind::Malla, "uModel");
    CHECK(modelo != nullptr &&
              *modelo == Contrato::TipoUniforme::Mat4,
          "uModel es una matriz 4x4");
    const Contrato::TipoUniforme* cantidadLuces =
        tipoDe(Contrato::ShaderKind::Malla, "uLightCount");
    CHECK(cantidadLuces != nullptr &&
              *cantidadLuces == Contrato::TipoUniforme::Int,
          "uLightCount es un entero");
    const Contrato::TipoUniforme* difusa =
        tipoDe(Contrato::ShaderKind::Malla, "uDiffuseTex");
    CHECK(difusa != nullptr &&
              *difusa == Contrato::TipoUniforme::Sampler2D,
          "uDiffuseTex es un sampler de 2D");
    const Contrato::TipoUniforme* cubemap =
        tipoDe(Contrato::ShaderKind::Cubemap, "uSkybox");
    CHECK(cubemap != nullptr &&
              *cubemap == Contrato::TipoUniforme::SamplerCube,
          "uSkybox es un sampler de cubemap");
    const Contrato::TipoUniforme* viewport =
        tipoDe(Contrato::ShaderKind::Lineas, "uViewport");
    CHECK(viewport != nullptr &&
              *viewport == Contrato::TipoUniforme::Vec4,
          "uViewport es un vector de 4 componentes");
}

// --- 3. Los renderers escriben con las constantes del contrato -------------

// Renderers y el tipo de programa que cada uno usa.
struct RendererDelMotor {
    const char* archivo;
    const char* nombre;
    std::vector<Contrato::ShaderKind> tipos;
};

const RendererDelMotor kRenderers[] = {
    {"MeshRenderer.cpp", "malla", {Contrato::ShaderKind::Malla}},
    {"LineRenderer.cpp", "lineas", {Contrato::ShaderKind::Lineas}},
    {"SpriteRenderer.cpp", "sprites", {Contrato::ShaderKind::Sprites}},
    {"TilemapRenderer.cpp", "tilemap", {Contrato::ShaderKind::Sprites}},
    {"SceneRenderer.cpp", "cielo", {Contrato::ShaderKind::Cielo,
                                    Contrato::ShaderKind::Cubemap}},
};

// Raiz del arbol de fuentes, deducida de la ruta de este archivo.
std::filesystem::path raizDeFuentes() {
    std::filesystem::path aqui(__FILE__);
    if (!std::filesystem::exists(aqui)) {
        // Ruta relativa: se resuelve contra el directorio de trabajo.
        aqui = std::filesystem::current_path() / aqui;
    }
    // <raiz>/tests/ContratoRenderTests.cpp -> <raiz>
    return aqui.parent_path().parent_path();
}

std::string leerArchivo(const std::filesystem::path& ruta) {
    std::ifstream archivo(ruta, std::ios::binary);
    if (!archivo) return std::string();
    return std::string((std::istreambuf_iterator<char>(archivo)),
                       std::istreambuf_iterator<char>());
}

void testRenderersUsanElContrato() {
    const std::filesystem::path dirRenderers =
        raizDeFuentes() / "FunshiEngineGL" / "src" / "Rendering";
    CHECK(std::filesystem::exists(dirRenderers),
          "se encuentra el arbol de fuentes del motor");

    for (const RendererDelMotor& renderer : kRenderers) {
        const std::filesystem::path ruta = dirRenderers / renderer.archivo;
        const std::string fuente = leerArchivo(ruta);
        CHECK(!fuente.empty(), "se puede leer el fuente del renderer");

        // Un uniform escrito con un literal no llega al backend: el motor
        // solo reconoce los nombres que expone el programa.
        for (std::size_t pos = fuente.find("\"u");
             pos != std::string::npos; pos = fuente.find("\"u", pos + 1)) {
            const std::size_t fin = fuente.find('"', pos + 1);
            const std::string literal =
                fuente.substr(pos + 1, (fin == std::string::npos)
                                            ? 0
                                            : (fin - pos - 1));
            if (literal.size() > 1 && literal[1] >= 'A' &&
                literal[1] <= 'Z') {
                CHECK(false, "el renderer escribe un uniform con un literal");
                std::cout << "        literal encontrado: \"" << literal
                          << "\" en " << renderer.archivo << std::endl;
                break;
            }
        }

        // Los uniforms que el renderer escribe tienen que estar declarados
        // en el tipo de programa que el renderer usa.
        std::set<std::string> permitidos;
        for (const Contrato::ShaderKind kind : renderer.tipos) {
            for (const std::string& nombre : nombresDe(kind))
                permitidos.insert(nombre);
        }

        const std::string prefijo = "Uniforme::";
        for (std::size_t pos = fuente.find(prefijo);
             pos != std::string::npos;
             pos = fuente.find(prefijo, pos + 1)) {
            std::size_t fin = pos + prefijo.size();
            while (fin < fuente.size() &&
                   (std::isalnum(static_cast<unsigned char>(fuente[fin])) ||
                    fuente[fin] == '_')) {
                ++fin;
            }
            const std::string constante =
                fuente.substr(pos + prefijo.size(), fin - pos - prefijo.size());
            const std::string nombre = nombreDeConstante(constante);
            CHECK(!nombre.empty(),
                  "el renderer usa un uniform que no existe en el contrato");
            CHECK(permitidos.count(nombre) == 1,
                  "el renderer escribe un uniform que su programa no expone");
        }
    }
}

// --- 4. El backend nulo cumple el contrato sin dibujar ---------------------

void testBackendNuloCumpleElContrato() {
    Backend::BackendNulo nulo;

    CHECK(nulo.init(nullptr), "el backend nulo deja arrancar el motor");
    CHECK(!nulo.available(), "el backend nulo no declara gpu disponible");

    Backend::MeshData malla;
    CHECK(nulo.createMesh(malla) == kInvalidHandle,
          "sin gpu no se crea malla");
    CHECK(nulo.createLineBatch(nullptr, 0) == kInvalidHandle,
          "sin gpu no se crea batch de lineas");
    Backend::Image2D imagen;
    CHECK(nulo.createTexture2D(imagen) == kInvalidHandle,
          "sin gpu no se crea textura");
    Backend::IRenderBackend::ImageCube cubemap;
    CHECK(nulo.createTextureCube(cubemap) == kInvalidHandle,
          "sin gpu no se crea cubemap");
    CHECK(nulo.createProgram(Contrato::ShaderKind::Malla) == kInvalidHandle,
          "sin gpu no se crea programa");
    CHECK(nulo.createRenderTarget(64, 64) == kInvalidHandle,
          "sin gpu no se crea render target");

    CHECK(nulo.uniformLocation(1, "uModel") == -1,
          "sin gpu no hay localizador de uniforms");
    CHECK(nulo.renderTargetColorTexture(1) == kInvalidHandle,
          "sin gpu el render target no tiene textura de color");
    CHECK(nulo.imguiTextureId(1) == nullptr,
          "sin gpu la gui no recibe textura");

    const char* diagnostico = nulo.diagnosticoGPU();
    CHECK(diagnostico != nullptr && diagnostico[0] != '\0',
          "el backend nulo informa como esta la gpu");

    // Destruir recursos que no existen no puede hacer nada: el motor llama a
    // los destructores de los handles invalidos.
    nulo.destroyMesh(kInvalidHandle);
    nulo.destroyLineBatch(kInvalidHandle);
    nulo.destroyTexture2D(kInvalidHandle);
    nulo.destroyTextureCube(kInvalidHandle);
    nulo.destroyProgram(kInvalidHandle);
    nulo.destroyRenderTarget(kInvalidHandle);
    CHECK(true, "destruir recursos inexistentes no falla");

    // El estado no puede romper nada: el motor lo llama igual, haya o no gpu.
    const float color[3] = {0.0f, 0.0f, 0.0f};
    nulo.setClearColor(color);
    nulo.clearScreen(color);
    nulo.setViewport(0, 0, 800, 600);
    nulo.setBlendEnabled(true);
    nulo.setDepthTestEnabled(true);
    nulo.setDepthMask(true);
    nulo.setDepthFunc(Backend::kDepthFuncLessEqual);
    nulo.bindRenderTarget(kInvalidHandle);
    nulo.bindDefaultFramebuffer();
    nulo.applyBaseState();
    nulo.drawFullscreenTriangle();
    nulo.drawMesh(kInvalidHandle, 0);
    nulo.drawLineBatch(kInvalidHandle, 0);
    CHECK(true, "el estado y el dibujado sin gpu no fallan");

    // El limite de frame tambien se cumple sin gpu: el motor lo llama en
    // cada vuelta del bucle y no puede romperse por no haber API grafica.
    nulo.beginFrame();
    nulo.endFrame();
    CHECK(true, "abrir y cerrar el frame sin gpu no falla");
}

// --- 5. El backend se instala, se reemplaza y el motor pide por tipo -------

// Backend de mentira: registra lo que el motor le pide y no dibuja nada.
class BackendFalso : public IRenderBackend {
public:
    static int instanciasVivas;

    std::set<std::string> uniformesConocidos;
    std::vector<Contrato::ShaderKind> programasPedidos;
    std::vector<std::string> uniformesConsultados;
    std::vector<int> uniformesEscritos;
    int programasDestruidos = 0;
    int framesAbiertos = 0;
    int framesCerrados = 0;

    explicit BackendFalso(std::set<std::string> uniforms = {})
        : uniformesConocidos(std::move(uniforms)) {
        ++instanciasVivas;
    }
    ~BackendFalso() override { --instanciasVivas; }

    bool init(void*) override { return true; }
    bool available() const override { return true; }

    void beginFrame() override { ++framesAbiertos; }
    void endFrame() override { ++framesCerrados; }

    Handle createMesh(const Backend::MeshData&) override { return 1; }
    void destroyMesh(Handle) override {}
    void drawMesh(Handle, unsigned int) override {}

    Handle createLineBatch(const float*, std::size_t) override { return 2; }
    void destroyLineBatch(Handle) override {}
    void updateLineBatch(Handle, const float*, std::size_t) override {}
    void drawLineBatch(Handle, unsigned int) override {}

    Handle createTexture2D(const Backend::Image2D&) override { return 3; }
    void destroyTexture2D(Handle) override {}
    void bindTexture2D(Handle, int) override {}
    void updateTexture2D(Handle, const Backend::Image2D&) override {}
    Handle createTextureCube(const IRenderBackend::ImageCube&) override {
        return 4;
    }
    void destroyTextureCube(Handle) override {}
    void bindTextureCube(Handle, int) override {}

    Handle createProgram(Contrato::ShaderKind kind) override {
        programasPedidos.push_back(kind);
        return 10;
    }
    void destroyProgram(Handle) override { ++programasDestruidos; }
    void useProgram(Handle) override {}
    int uniformLocation(Handle, const char* name) override {
        uniformesConsultados.emplace_back(name);
        auto it = localizadores.find(name);
        if (it != localizadores.end()) return it->second;
        if (uniformesConocidos.count(name) == 0) return -1;
        const int id = siguienteLocalizador++;
        localizadores.emplace(name, id);
        return id;
    }
    void setUniformMat4(int location, const glm::mat4&) override {
        uniformesEscritos.push_back(location);
    }
    void setUniformMat3(int location, const glm::mat3&) override {
        uniformesEscritos.push_back(location);
    }
    void setUniformVec3(int location, const glm::vec3&) override {
        uniformesEscritos.push_back(location);
    }
    void setUniformVec4(int location, const float*) override {
        uniformesEscritos.push_back(location);
    }
    void setUniformFloat(int location, float) override {
        uniformesEscritos.push_back(location);
    }
    void setUniformInt(int location, int) override {
        uniformesEscritos.push_back(location);
    }

    Handle createRenderTarget(int, int) override { return 5; }
    void destroyRenderTarget(Handle) override {}
    void bindRenderTarget(Handle) override {}
    void bindDefaultFramebuffer() override {}
    Handle renderTargetColorTexture(Handle) const override { return 6; }
    void* imguiTextureId(Handle) const override {
        return reinterpret_cast<void*>(static_cast<std::uintptr_t>(7));
    }

    void setViewport(int, int, int, int) override {}
    void clearScreen(const float[3]) override {}
    void applyBaseState() override {}
    void setClearColor(const float[3]) override {}
    const char* diagnosticoGPU() const override { return "backend de mentira"; }
    void setBlendEnabled(bool) override {}
    void setDepthTestEnabled(bool) override {}
    void setDepthMask(bool) override {}
    void setDepthFunc(Backend::DepthFunc) override {}
    void drawFullscreenTriangle() override {}

private:
    std::map<std::string, int> localizadores;
    int siguienteLocalizador = 1;
};

int BackendFalso::instanciasVivas = 0;

void testBackendActivoPorDefectoEsElNulo() {
    CHECK(dynamic_cast<Backend::BackendNulo*>(&Backend::activeBackend()) !=
              nullptr,
          "sin instalar nada, el backend activo es el nulo");
}

void testInstalarBackendReemplazaElActivo() {
    Backend::instalarBackend(nullptr);
    {
        auto falso = std::make_unique<BackendFalso>();
        Backend::instalarBackend(std::move(falso));
        CHECK(dynamic_cast<BackendFalso*>(&Backend::activeBackend()) != nullptr,
              "el backend instalado queda activo");
        CHECK(BackendFalso::instanciasVivas == 1,
              "el backend instalado vive mientras esta activo");
    }
    // Reemplazar el backend lo destruye: la propiedad pasa al motor.
    Backend::instalarBackend(nullptr);
    CHECK(BackendFalso::instanciasVivas == 0,
          "el backend reemplazado se destruye");
    CHECK(dynamic_cast<Backend::BackendNulo*>(&Backend::activeBackend()) !=
              nullptr,
          "instalar el backend nulo restaura el backend en blanco");
}

void testElLimiteDeFrameEsParteDelContrato() {
    Backend::instalarBackend(nullptr);
    auto falso = std::make_unique<BackendFalso>();
    BackendFalso* observed = falso.get();
    Backend::instalarBackend(std::move(falso));

    // El backend se prepara con la ventana del sistema: es quien crea la
    // superficie sobre la que dibuja.
    CHECK(observed->init(nullptr),
          "el backend se inicializa con la ventana nativa");

    // El limite de frame es del contrato: el motor lo llama en cada vuelta
    // del bucle y el backend tiene que poder cumplirlo.
    observed->beginFrame();
    CHECK(observed->framesAbiertos == 1, "el frame se abre");
    observed->endFrame();
    CHECK(observed->framesCerrados == 1, "el frame se cierra");

    Backend::instalarBackend(nullptr);
}

void testElMotorPideLosProgramasPorTipo() {
    Backend::instalarBackend(nullptr);
    auto falso = std::make_unique<BackendFalso>(nombresDelContrato());
    BackendFalso* observed = falso.get();
    Backend::instalarBackend(std::move(falso));

    {
        ShaderProgram programa = ShaderProgram::create(Contrato::ShaderKind::Malla);
        CHECK(observed->programasPedidos.size() == 1,
              "se pide un solo programa");
        CHECK(observed->programasPedidos[0] == Contrato::ShaderKind::Malla,
              "el programa se pide por tipo, no por fuente de shader");
        CHECK(programa.isValid(), "el programa es valido con un backend real");

        programa.setMat4(Contrato::Uniforme::kModel, glm::mat4(1.0f));
        CHECK(observed->uniformesConsultados.size() == 1,
              "se consulta el localizador del uniform escrito");
        CHECK(observed->uniformesConsultados[0] == "uModel",
              "el uniform se consulta por su nombre canonico");
        CHECK(observed->uniformesEscritos.size() == 1,
              "el valor del uniform llega al backend");

        // El localizador se cachea: la segunda escritura no vuelve a buscar.
        programa.setMat4(Contrato::Uniforme::kModel, glm::mat4(2.0f));
        CHECK(observed->uniformesConsultados.size() == 1,
              "el localizador del uniform se cachea");
        CHECK(observed->uniformesEscritos.size() == 2,
              "la segunda escritura del uniform tambien llega");

        // Un uniform que el programa no expone no se escribe.
        programa.setFloat("uNoExiste", 1.0f);
        CHECK(observed->uniformesConsultados.size() == 2,
              "un uniform desconocido se consulta");
        CHECK(observed->uniformesEscritos.size() == 2,
              "un uniform desconocido no se escribe");
    }

    CHECK(observed->programasDestruidos == 1,
          "el programa se libera en el backend al salir del ambito");

    Backend::instalarBackend(nullptr);
}

void testMotorSinBackendNoRompe() {
    Backend::instalarBackend(nullptr);
    {
        ShaderProgram programa =
            ShaderProgram::create(Contrato::ShaderKind::Lineas);
        CHECK(!programa.isValid(),
              "sin backend el programa no es valido");
        programa.use();
        programa.setMat4(Contrato::Uniforme::kModel, glm::mat4(1.0f));
        programa.setFloat(Contrato::Uniforme::kWidth, 2.0f);
        programa.setInt(Contrato::Uniforme::kUseTexture, 0);
        ShaderProgram::unbind();
        CHECK(true, "escribir uniforms sin backend no falla");
    }
    CHECK(BackendFalso::instanciasVivas == 0,
          "el backend de mentira del caso anterior quedo destruido");
}

} // namespace

int main() {
    std::cout << "Pruebas del contrato de render" << std::endl;

    testTablaDeUniformesCompleta();
    testSemanticaPorTipoDePrograma();
    testRenderersUsanElContrato();
    testBackendNuloCumpleElContrato();
    testBackendActivoPorDefectoEsElNulo();
    testInstalarBackendReemplazaElActivo();
    testElLimiteDeFrameEsParteDelContrato();
    testElMotorPideLosProgramasPorTipo();
    testMotorSinBackendNoRompe();

    std::cout << (fallos == 0 ? "OK" : "FALLOS") << ": " << total
              << " comprobaciones" << std::endl;
    return fallos == 0 ? 0 : 1;
}
